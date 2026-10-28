// BluetoothManager, part 2: the MeshCore session on top of the BLE link.
//
// Reacts to what the node reports through the companion protocol (session
// start, channels, messages, contacts, delivery confirmations) and offers the
// actions the QML pages call (send, add/remove channel, manage contacts,
// administer repeaters). Frames themselves are built and parsed in
// companionprotocol.cpp.

#include "bluetoothmanager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

namespace {

// Messages from the node's offline cache can be hours old, so the sender's
// timestamp is kept - unless the sender's clock is obviously wrong (in the
// future, or older than 30 days). Then the receive time is used instead.
qint64 plausibleSendTime(quint32 ts)
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (ts > 0 && qint64(ts) <= now + 3600 && qint64(ts) >= now - 30 * 86400)
        return qint64(ts);
    return now;
}

quint16 rdU16(const QByteArray &d, int o)
{
    return quint16(quint8(d.at(o))) | (quint16(quint8(d.at(o + 1))) << 8);
}

quint32 rdU32(const QByteArray &d, int o)
{
    return quint32(rdU16(d, o)) | (quint32(rdU16(d, o + 2)) << 16);
}

QString durationText(quint32 secs)
{
    const quint32 d = secs / 86400, h = (secs % 86400) / 3600, m = (secs % 3600) / 60;
    if (d > 0)
        return BluetoothManager::tr("%1 d %2 h").arg(d).arg(h);
    if (h > 0)
        return BluetoothManager::tr("%1 h %2 min").arg(h).arg(m);
    return BluetoothManager::tr("%1 min").arg(m);
}

QVariantMap row(const QString &label, const QString &value)
{
    return QVariantMap{ { QStringLiteral("label"), label },
                        { QStringLiteral("value"), value } };
}

} // namespace

// ---------------------------------------------------------------------------
// Reactions to the companion protocol
// ---------------------------------------------------------------------------

void BluetoothManager::connectCompanionSignals()
{
    const CompanionProtocol *cp = &m_companion;

    // Bytes to and from the node.
    connect(cp, &CompanionProtocol::writeRequested, this, &BluetoothManager::writeToUart);
    connect(cp, &CompanionProtocol::traffic, this, &BluetoothManager::appendTraffic);

    // Session, channels and messages.
    connect(cp, &CompanionProtocol::selfInfoReceived, this, &BluetoothManager::onSessionStarted);
    connect(cp, &CompanionProtocol::deviceInfoReceived, this, &BluetoothManager::onDeviceInfo);
    connect(cp, &CompanionProtocol::batteryReceived, this, &BluetoothManager::setBattery);
    connect(cp, &CompanionProtocol::channelInfoReceived, this, &BluetoothManager::onChannelInfo);
    connect(cp, &CompanionProtocol::channelMessageReceived,
            this, &BluetoothManager::onChannelMessage);
    connect(cp, &CompanionProtocol::contactMessageReceived,
            this, &BluetoothManager::onContactMessage);
    connect(cp, &CompanionProtocol::noMoreMessages, this, &BluetoothManager::onNoMoreMessages);

    // Answers to our own commands.
    connect(cp, &CompanionProtocol::sendQueued, this, &BluetoothManager::onSendQueued);
    connect(cp, &CompanionProtocol::sendConfirmed,
            this, [this](const QByteArray &ack, quint32 rttMs) {
                emit dmDelivered(QString::fromLatin1(ack.toHex()), int(rttMs));
            });
    connect(cp, &CompanionProtocol::okReceived, this, &BluetoothManager::onNodeOk);
    connect(cp, &CompanionProtocol::errorReported, this, &BluetoothManager::onNodeError);
    connect(cp, &CompanionProtocol::loginResult,
            this, [this](const QByteArray &prefix, bool ok, bool admin) {
                emit nodeLoginResult(keyHexForPrefix(prefix), ok, admin);
            });
    connect(cp, &CompanionProtocol::statusResponse,
            this, [this](const QByteArray &prefix, const QByteArray &data) {
                emit nodeStatusReceived(keyHexForPrefix(prefix), parseNodeStatus(data));
            });

    // Contact table.
    connect(cp, &CompanionProtocol::contactsSyncStarted,
            this, &BluetoothManager::onContactsSyncStarted);
    connect(cp, &CompanionProtocol::contactReceived,
            &m_contacts, &ContactListModel::upsert);
    connect(cp, &CompanionProtocol::contactsSyncFinished,
            this, &BluetoothManager::onContactsSyncFinished);
    connect(cp, &CompanionProtocol::contactChanged,
            &m_contactRefreshDebounce, [this]() { m_contactRefreshDebounce.start(); });
}

void BluetoothManager::setupSessionTimers()
{
    // A new advert was heard (repeater, companion, room...). The push only
    // carries the key, so fetch the changed entries - debounced, because a
    // flood advert is often heard several times via different repeaters.
    m_contactRefreshDebounce.setSingleShot(true);
    m_contactRefreshDebounce.setInterval(1500);
    connect(&m_contactRefreshDebounce, &QTimer::timeout, this, [this]() {
        if (m_sessionReady)
            m_companion.requestContacts(m_contactsLastMod);
    });

    // Keeps the "x min ago" texts in the contact list current.
    m_contactAgeTimer.setInterval(30000);
    connect(&m_contactAgeTimer, &QTimer::timeout,
            &m_contacts, &ContactListModel::refreshAges);
    m_contactAgeTimer.start();

    // Safety net: a PUSH_MSG_WAITING that arrives while Lomiri has the app
    // suspended is lost, but the message stays queued on the node. Polling
    // now and then picks such messages up.
    m_messagePollTimer.setInterval(60000);
    connect(&m_messagePollTimer, &QTimer::timeout, this, [this]() {
        if (m_sessionReady) {
            syncNextMessage();
            m_companion.requestBattery();
        }
    });
    m_messagePollTimer.start();

    // The node answers SEND_SELF_ADVERT with OK or ERR; see sendAdvert().
    m_advertTimeout.setSingleShot(true);
    m_advertTimeout.setInterval(8000);
    connect(&m_advertTimeout, &QTimer::timeout, this, &BluetoothManager::onAdvertTimeout);
}

// The node answered APP_START: the session is up. Read everything the app
// needs from it.
void BluetoothManager::onSessionStarted(const QString &nodeName)
{
    m_nodeName = nodeName;
    m_sessionReady = true;
    emit nodeInfoChanged();
    setConnectionState(nodeName.isEmpty() ? tr("Session started")
                                          : tr("Session started - ") + nodeName);
    emit connectionChanged();

    // Ask how many channel slots the node has; the reply then triggers
    // reading every slot (see onDeviceInfo). If an old firmware does not
    // answer, fall back to the first 4.
    m_maxChannels = 0;
    m_channelNames.clear();
    emit channelsChanged();
    m_companion.sendDeviceQuery();
    QTimer::singleShot(2500, this, [this]() {
        if (m_sessionReady && m_maxChannels == 0)
            requestAllChannels(4);
    });

    // Fetch everything the node cached while we were away.
    syncNextMessage();

    // Align the node clock first: it stamps lastmod on every advert, which is
    // what "last heard" is computed from.
    m_companion.setDeviceTime();
    m_companion.requestBattery();
    QTimer::singleShot(600, this, [this]() {
        if (m_sessionReady)
            refreshContacts();
    });
}

void BluetoothManager::onDeviceInfo(int /*firmwareVersion*/, int maxChannels, int maxContacts)
{
    if (maxContacts > 0 && maxContacts != m_maxContacts) {
        m_maxContacts = maxContacts;
        emit contactLimitChanged();
    }
    if (maxChannels <= 0 || m_maxChannels > 0)
        return;
    m_maxChannels = maxChannels;
    emit channelsChanged();
    requestAllChannels(maxChannels);
}

void BluetoothManager::onChannelInfo(int idx, const QString &name)
{
    while (m_channelNames.size() <= idx)
        m_channelNames.append(QString());
    if (m_channelNames.at(idx) == name)
        return;
    m_channelNames[idx] = name;
    emit channelsChanged();
}

void BluetoothManager::onChannelMessage(int idx, const QString &text, quint32 ts,
                                        const MessagePath &path)
{
    const qint64 sent = plausibleSendTime(ts);
    const QString time = QDateTime::fromSecsSinceEpoch(sent).toString(QStringLiteral("HH:mm"));
    ++m_drainCount;
    emit channelMessage(idx, text, time, double(sent), routeInfo(path));
}

// A direct message, or the reply to a CLI command sent to a repeater.
void BluetoothManager::onContactMessage(const QByteArray &prefix, quint8 txtType,
                                        const QString &text, quint32 ts)
{
    const QString key = keyHexForPrefix(prefix);
    if (txtType == CompanionProtocol::TxtCliData) {
        emit nodeCliReply(key, text);
        return;
    }
    ++m_drainCount;
    emit directMessage(key, m_contacts.nameForKey(key), text, double(plausibleSendTime(ts)));
}

// RESP_SENT does not say which command it answers, but the node answers in
// order: m_sentFifo holds one entry per sent command that produces a
// RESP_SENT (DM, channel text, login, status, CLI) - the contact key for
// DMs, "" for the others.
void BluetoothManager::onSendQueued(const QByteArray &ack, bool flood, quint32 timeoutMs)
{
    if (m_sentFifo.isEmpty())
        return;
    const QString key = m_sentFifo.takeFirst();
    if (!key.isEmpty())
        emit dmSent(key, QString::fromLatin1(ack.toHex()), flood, int(timeoutMs));
}

// The node caches messages while the app is away and hands them over one per
// SYNC_NEXT_MESSAGE until NO_MORE_MESSAGES. Each drain is counted so the UI
// can say how many cached messages were fetched.
void BluetoothManager::onNoMoreMessages()
{
    // Live messages pass through the same queue (PUSH_MSG_WAITING -> one
    // sync); only report drains we started, i.e. actual cached messages.
    if (m_reportDrain && m_drainCount > 0) {
        m_lastSyncInfo = tr("%1 message(s) fetched from node at %2")
                             .arg(m_drainCount)
                             .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm")));
        qDebug() << "BluetoothManager:" << m_lastSyncInfo;
        emit syncInfoChanged();
    }
    m_drainCount = 0;
    m_reportDrain = false;
}

void BluetoothManager::onNodeOk()
{
    if (!m_advertPending)
        return;
    m_advertPending = false;
    m_advertTimeout.stop();
    const QString now = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    setAdvertInfo(true, QStringLiteral("✓ ") + tr("%1 advert sent at %2")
                            .arg(m_advertFlood ? tr("Flood") : tr("Zero-hop"))
                            .arg(now));
    qDebug() << "BluetoothManager:" << m_lastAdvertInfo;
}

void BluetoothManager::onNodeError(const QString &what)
{
    if (m_advertPending) {
        m_advertPending = false;
        m_advertTimeout.stop();
        setAdvertInfo(false, tr("Advert failed: ") + what);
        return;
    }
    emit sendFailed(what);
}

void BluetoothManager::onAdvertTimeout()
{
    if (!m_advertPending)
        return;
    m_advertPending = false;
    setAdvertInfo(false, tr("No answer from the node - advert may not have been sent"));
}

void BluetoothManager::onContactsSyncStarted(int total)
{
    qDebug() << "BluetoothManager: contact sync," << total << "contact(s)";
    m_contactsSyncing = true;
    emit contactsSyncingChanged();
}

void BluetoothManager::onContactsSyncFinished(quint32 lastMod)
{
    if (lastMod > m_contactsLastMod)
        m_contactsLastMod = lastMod;
    m_contactsSyncing = false;
    emit contactsSyncingChanged();
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

bool BluetoothManager::checkSession()
{
    if (!m_uartReady) {
        emit sendFailed(tr("Not connected to a node"));
        return false;
    }
    if (!m_sessionReady) {
        emit sendFailed(tr("Node session not started yet"));
        return false;
    }
    return true;
}

void BluetoothManager::requestAllChannels(int count)
{
    qDebug() << "BluetoothManager: reading" << count << "channel slot(s)";
    for (int i = 0; i < count; ++i)
        m_companion.requestChannel(i);
}

void BluetoothManager::setBattery(int milliVolts)
{
    if (milliVolts == m_batteryMv)
        return;
    m_batteryMv = milliVolts;
    emit batteryChanged();
}

int BluetoothManager::batteryPercent() const
{
    if (m_batteryMv <= 0)
        return -1;
    // Typical LiPo discharge curve under light load: millivolts -> percent.
    static const int curve[][2] = {
        {4200, 100}, {4100, 90}, {4000, 80}, {3900, 65}, {3800, 50},
        {3700, 35}, {3600, 20}, {3500, 10}, {3400, 5}, {3300, 0}
    };
    if (m_batteryMv >= curve[0][0])
        return 100;
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (m_batteryMv >= curve[i][0]) {
            const int v0 = curve[i][0], v1 = curve[i - 1][0];
            const int p0 = curve[i][1], p1 = curve[i - 1][1];
            return p0 + (m_batteryMv - v0) * (p1 - p0) / (v1 - v0);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Channels and messages
// ---------------------------------------------------------------------------

bool BluetoothManager::sendChannelMessage(int channelIdx, const QString &text)
{
    if (text.isEmpty())
        return false;
    if (!m_uartReady) {
        emit sendFailed(tr("Not connected to a node"));
        return false;
    }
    if (!m_sessionReady) {
        emit sendFailed(tr("Node session not started yet"));
        return false;
    }

    m_sentFifo.append(QString());
    m_companion.sendChannelText(channelIdx, text);
    return true;
}

void BluetoothManager::syncNextMessage()
{
    if (!m_uartReady)
        return;
    m_drainCount = 0;
    m_reportDrain = true;
    m_companion.syncNextMessage();
}

QString BluetoothManager::normalizeChannelName(const QString &input) const
{
    QString name = input.trimmed();
    if (name.isEmpty())
        return QString();
    if (!name.startsWith(QLatin1Char('#')))
        name.prepend(QLatin1Char('#'));
    // Hashtag names are case-sensitive in MeshCore ("#P53" != "#p53"), so the
    // case is kept as typed. 31 bytes leave room for the terminating NUL.
    if (name.size() < 2 || name.toUtf8().size() > 31 || name.contains(QLatin1Char(' ')))
        return QString();
    return name;
}

int BluetoothManager::channelIndexOf(const QString &name) const
{
    if (name.isEmpty())
        return -1;
    for (int i = 0; i < m_channelNames.size(); ++i) {
        if (m_channelNames.at(i) == name)
            return i;
    }
    return -1;
}

int BluetoothManager::addChannel(const QString &input)
{
    if (!checkSession())
        return -1;
    const QString name = normalizeChannelName(input);
    if (name.isEmpty()) {
        emit sendFailed(tr("\"%1\" is not a valid channel name").arg(input));
        return -1;
    }
    const int existing = channelIndexOf(name);
    if (existing >= 0)
        return existing;
    if (m_maxChannels <= 0) {
        emit sendFailed(tr("Channel slots not read yet - try again in a moment"));
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < m_maxChannels; ++i) {
        if (i >= m_channelNames.size() || m_channelNames.at(i).isEmpty()) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        emit sendFailed(tr("All %1 channel slots are in use - remove one first")
                            .arg(m_maxChannels));
        return -1;
    }

    // Hashtag channel key = first 16 bytes of SHA-256 over the name incl. '#'
    // (verified against the node: "#ping", "#test").
    const QByteArray secret =
        QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha256).left(16);
    m_companion.setChannel(slot, name, secret);
    // Read the slot back: commands run in order, so this returns the new
    // state (and the protocol learns the channel hash for message paths).
    m_companion.requestChannel(slot);

    while (m_channelNames.size() <= slot)
        m_channelNames.append(QString());
    m_channelNames[slot] = name;
    emit channelSlotCleared(slot);
    emit channelsChanged();
    qDebug() << "BluetoothManager: added channel" << name << "in slot" << slot;
    return slot;
}

bool BluetoothManager::removeChannel(int channelIdx)
{
    if (!checkSession())
        return false;
    if (channelIdx < 0 || channelIdx >= m_channelNames.size()
        || m_channelNames.at(channelIdx).isEmpty()) {
        emit sendFailed(tr("No channel in that slot"));
        return false;
    }
    const QString name = m_channelNames.at(channelIdx);
    // Empty name + all-zero secret is the documented "delete" form. (A NAMED
    // slot with a zero secret would instead receive null-key traffic.)
    m_companion.setChannel(channelIdx, QString(), QByteArray(16, char(0)));
    m_companion.requestChannel(channelIdx);

    m_channelNames[channelIdx] = QString();
    emit channelSlotCleared(channelIdx);
    emit channelsChanged();
    qDebug() << "BluetoothManager: removed channel" << name << "from slot" << channelIdx;
    return true;
}

QVariantMap BluetoothManager::routeInfo(const MessagePath &path) const
{
    // Flat strings rather than nested lists: they go straight into a QML
    // ListModel row and into the JSON message history.
    // Deliberately NOT translated (no tr()): the route is part of the message,
    // stored in the history and sent to the mesh by "Reply with route" - it
    // must read the same for everyone, whatever the phone's language.
    QString summary;
    QStringList detail;

    if (path.direct) {
        summary = QStringLiteral("direct");
        detail << QStringLiteral("Sent direct (routed), not flooded");
    } else if (path.hops == 0) {
        summary = QStringLiteral("0 hops - heard directly");
        detail << QStringLiteral("Sender") << QStringLiteral("   ↓  no repeater")
               << QStringLiteral("You");
    } else {
        const QString hopsText = path.hops == 1 ? QStringLiteral("1 hop")
                                                : QStringLiteral("%1 hops").arg(path.hops);
        if (!path.pathKnown) {
            // Offline-cache messages: the raw packet was never seen.
            summary = hopsText;
            detail << QStringLiteral("%1 (%2-byte hashes) - route not recorded:")
                          .arg(hopsText).arg(path.hashSize)
                   << QStringLiteral("the message came from the node's cache, so its "
                         "raw packet was not seen by the app.");
        } else {
            QStringList hashes;
            detail << QStringLiteral("Sender");
            for (const QByteArray &h : path.hashes) {
                const QString hex = QString::fromLatin1(h.toHex());
                hashes << hex;
                detail << QStringLiteral("   ↓  %1  %2").arg(hex, repeaterNames(h));
            }
            detail << QStringLiteral("You");
            summary = hopsText + QStringLiteral(" · ") + hashes.join(QStringLiteral(" › "));
        }
    }
    if (path.radioKnown)
        detail << QStringLiteral("Last hop: SNR %1 dB, RSSI %2 dBm")
                      .arg(path.snr, 0, 'f', 1).arg(path.rssi);

    return QVariantMap{
        { QStringLiteral("hops"),        path.direct ? -1 : path.hops },
        { QStringLiteral("route"),       summary },
        { QStringLiteral("routeDetail"), detail.join(QLatin1Char('\n')) }
    };
}

// Up to three contacts whose key starts with the hop hash - a 1-byte hash is
// often shared by several repeaters.
QString BluetoothManager::repeaterNames(const QByteArray &hopHash) const
{
    const QStringList names = m_contacts.namesForHopHash(hopHash);
    if (names.isEmpty())
        return QStringLiteral("unknown repeater");
    if (names.size() <= 3)
        return names.join(QStringLiteral(" / "));
    return QStringList(names.mid(0, 3)).join(QStringLiteral(" / "))
           + QStringLiteral(" / +%1").arg(names.size() - 3);
}

// ---------------------------------------------------------------------------
// Contacts and repeater administration
// ---------------------------------------------------------------------------

QString BluetoothManager::keyHexForPrefix(const QByteArray &prefix) const
{
    const MeshContact *c = m_contacts.findByPrefix(prefix);
    return QString::fromLatin1(c ? c->publicKey.toHex() : prefix.toHex());
}

void BluetoothManager::refreshContacts()
{
    if (!checkSession())
        return;
    m_contactsLastMod = 0;
    m_contacts.clearContacts();
    m_companion.requestContacts(0);
}

void BluetoothManager::sendAdvert(bool flood)
{
    if (!checkSession()) {
        setAdvertInfo(false, tr("Advert not sent - not connected to your node"));
        return;
    }
    // The node answers SEND_SELF_ADVERT with OK once it has queued the
    // advert for transmission (ERR if it could not). Commands are answered
    // in order, so the next OK/ERR after this belongs to it.
    m_advertPending = true;
    m_advertFlood = flood;
    m_advertTimeout.start();
    setAdvertInfo(true, tr("Sending advert..."));
    m_companion.sendSelfAdvert(flood);
}

void BluetoothManager::setAdvertInfo(bool ok, const QString &text)
{
    m_advertOk = ok;
    m_lastAdvertInfo = text;
    emit advertInfoChanged();
}

bool BluetoothManager::loginToNode(const QString &keyHex, const QString &password)
{
    if (!checkSession())
        return false;
    const MeshContact *c = m_contacts.findByKeyHex(keyHex);
    if (!c) {
        emit sendFailed(tr("Unknown node"));
        return false;
    }
    m_sentFifo.append(QString());
    m_companion.sendLogin(c->publicKey, password);
    return true;
}

bool BluetoothManager::requestNodeStatus(const QString &keyHex)
{
    if (!checkSession())
        return false;
    const MeshContact *c = m_contacts.findByKeyHex(keyHex);
    if (!c) {
        emit sendFailed(tr("Unknown node"));
        return false;
    }
    m_sentFifo.append(QString());
    m_companion.sendStatusRequest(c->publicKey);
    return true;
}

bool BluetoothManager::sendNodeCommand(const QString &keyHex, const QString &command)
{
    const QString cmd = command.trimmed();
    if (cmd.isEmpty() || !checkSession())
        return false;
    const MeshContact *c = m_contacts.findByKeyHex(keyHex);
    if (!c) {
        emit sendFailed(tr("Unknown node"));
        return false;
    }
    m_sentFifo.append(QString());
    m_companion.sendDirectText(c->publicKey, cmd, CompanionProtocol::TxtCliData);
    return true;
}

bool BluetoothManager::sendDirectMessage(const QString &keyHex, const QString &text)
{
    if (text.trimmed().isEmpty() || !checkSession())
        return false;
    const MeshContact *found = m_contacts.findByKeyHex(keyHex);
    if (!found) {
        emit sendFailed(tr("Unknown contact"));
        return false;
    }
    const MeshContact c = *found;
    // The node only sends to contacts in its own table - otherwise it answers
    // "not found". Contacts known only from an advert it did not store (its
    // table is full, or manual-add mode) are saved first.
    if (!c.onNode) {
        if (!storeContactOnNode(c))
            return false;
    }
    m_sentFifo.append(QString::fromLatin1(c.publicKey.toHex()));
    m_companion.sendDirectText(c.publicKey, text.trimmed(), CompanionProtocol::TxtPlain);
    return true;
}

bool BluetoothManager::storeContactOnNode(const MeshContact &c)
{
    if (m_maxContacts > 0 && m_contacts.nodeCount() >= m_maxContacts) {
        emit sendFailed(tr("Your node's contact list is full (%1 of %2). Remove a contact "
                           "first: long-press it in the contact list → Delete contact.")
                            .arg(m_contacts.nodeCount()).arg(m_maxContacts));
        return false;
    }
    qDebug() << "BluetoothManager: storing contact on node:" << c.name;
    m_companion.addUpdateContact(c);
    m_contacts.setOnNode(c.publicKey, true);
    return true;
}

bool BluetoothManager::saveContactToNode(const QString &keyHex)
{
    if (!checkSession())
        return false;
    const MeshContact *found = m_contacts.findByKeyHex(keyHex);
    if (!found) {
        emit sendFailed(tr("Unknown contact"));
        return false;
    }
    if (found->onNode)
        return true;
    return storeContactOnNode(*found);
}

bool BluetoothManager::removeContactFromNode(const QString &keyHex)
{
    if (!checkSession())
        return false;
    const MeshContact *found = m_contacts.findByKeyHex(keyHex);
    if (!found) {
        emit sendFailed(tr("Unknown contact"));
        return false;
    }
    const QByteArray key = found->publicKey;
    // Only contacts in the node's table exist there; the others (known only
    // from an advert the node did not store) are just dropped from the list.
    if (found->onNode) {
        qDebug() << "BluetoothManager: removing contact from node:" << found->name;
        m_companion.removeContact(key);
    } else {
        qDebug() << "BluetoothManager: dropping contact (not on node):" << found->name;
    }
    // It reappears the next time its advert is heard.
    m_contacts.removeContact(key);
    return true;
}

int BluetoothManager::deleteContacts(const QStringList &keyHexes)
{
    if (!checkSession())
        return 0;
    int n = 0;
    for (const QString &k : keyHexes) {
        if (removeContactFromNode(k))
            ++n;
    }
    qDebug() << "BluetoothManager: deleted" << n << "contact(s)";
    return n;
}

QVariantList BluetoothManager::parseNodeStatus(const QByteArray &d)
{
    // Repeater status block (simple_repeater RepeaterStats, little endian):
    //  0 batt_mv u16      2 tx_queue u16    4 noise_floor i16  6 last_rssi i16
    //  8 pkts_recv u32   12 pkts_sent u32  16 air_time_s u32  20 uptime_s u32
    // 24 sent_flood u32  28 sent_direct    32 recv_flood      36 recv_direct
    // 40 err_events u16  42 last_snr i16 (x4)  44 direct_dups  46 flood_dups
    // 48 rx_air_time_s u32
    // Fields are read only as far as the block reaches, so older firmware
    // with a shorter block still shows what it has.
    QVariantList out;
    const int n = d.size();
    if (n >= 2)
        out << row(tr("Battery"),
                   QStringLiteral("%1 V").arg(rdU16(d, 0) / 1000.0, 0, 'f', 2));
    if (n >= 24) {
        out << row(tr("Uptime"), durationText(rdU32(d, 20)));
        out << row(tr("Noise floor"),
                   QStringLiteral("%1 dBm").arg(qint16(rdU16(d, 4))));
        out << row(QStringLiteral("Last RSSI"),
                   QStringLiteral("%1 dBm").arg(qint16(rdU16(d, 6))));
        out << row(tr("TX queue"), QString::number(rdU16(d, 2)));
        out << row(tr("Packets rx / tx"),
                   QStringLiteral("%1 / %2").arg(rdU32(d, 8)).arg(rdU32(d, 12)));
        out << row(tr("TX airtime"), durationText(rdU32(d, 16)));
    }
    if (n >= 40) {
        out << row(tr("Flood rx / tx"),
                   QStringLiteral("%1 / %2").arg(rdU32(d, 32)).arg(rdU32(d, 24)));
        out << row(tr("Direct rx / tx"),
                   QStringLiteral("%1 / %2").arg(rdU32(d, 36)).arg(rdU32(d, 28)));
    }
    if (n >= 44) {
        out << row(tr("Last SNR"),
                   QStringLiteral("%1 dB").arg(qint16(rdU16(d, 42)) / 4.0, 0, 'f', 1));
        out << row(tr("Errors"), QString::number(rdU16(d, 40)));
    }
    if (n >= 48)
        out << row(tr("Duplicates direct / flood"),
                   QStringLiteral("%1 / %2").arg(rdU16(d, 44)).arg(rdU16(d, 46)));
    if (n >= 52)
        out << row(tr("RX airtime"), durationText(rdU32(d, 48)));
    if (out.isEmpty())
        out << row(tr("Raw"), QString::fromLatin1(d.toHex(' ')));
    return out;
}

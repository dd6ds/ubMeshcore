#include "companionprotocol.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

namespace {

void appendLE32(QByteArray &out, quint32 value)
{
    out.append(char(value & 0xFF));
    out.append(char((value >> 8) & 0xFF));
    out.append(char((value >> 16) & 0xFF));
    out.append(char((value >> 24) & 0xFF));
}

quint32 readLE32(const QByteArray &data, int offset)
{
    if (offset + 4 > data.size())
        return 0;
    return quint32(quint8(data.at(offset)))
         | (quint32(quint8(data.at(offset + 1))) << 8)
         | (quint32(quint8(data.at(offset + 2))) << 16)
         | (quint32(quint8(data.at(offset + 3))) << 24);
}

} // namespace

CompanionProtocol::CompanionProtocol(QObject *parent)
    : QObject(parent)
{
}

QString CompanionProtocol::commandName(quint8 code)
{
    switch (code) {
    case CmdAppStart:          return QStringLiteral("APP_START");
    case CmdSendTxtMsg:        return QStringLiteral("SEND_TXT_MSG");
    case CmdSendChannelTxtMsg: return QStringLiteral("SEND_CHANNEL_TXT_MSG");
    case CmdGetContacts:       return QStringLiteral("GET_CONTACTS");
    case CmdGetDeviceTime:     return QStringLiteral("GET_DEVICE_TIME");
    case CmdSetDeviceTime:     return QStringLiteral("SET_DEVICE_TIME");
    case CmdSendSelfAdvert:    return QStringLiteral("SEND_SELF_ADVERT");
    case CmdAddUpdateContact:  return QStringLiteral("ADD_UPDATE_CONTACT");
    case CmdRemoveContact:     return QStringLiteral("REMOVE_CONTACT");
    case CmdSyncNextMessage:   return QStringLiteral("SYNC_NEXT_MESSAGE");
    case CmdDeviceQuery:       return QStringLiteral("DEVICE_QUERY");
    case CmdSendLogin:         return QStringLiteral("SEND_LOGIN");
    case CmdSendStatusReq:     return QStringLiteral("SEND_STATUS_REQ");
    case CmdGetChannel:        return QStringLiteral("GET_CHANNEL");
    case CmdSetChannel:        return QStringLiteral("SET_CHANNEL");
    default:
        return QStringLiteral("CMD(0x%1)").arg(code, 2, 16, QLatin1Char('0'));
    }
}

QString CompanionProtocol::responseName(quint8 code)
{
    switch (code) {
    case RespContactMsgRecv3: return QStringLiteral("CONTACT_MSG_RECV_V3");
    case RespChannelMsgRecv2: return QStringLiteral("CHANNEL_MSG_RECV");
    case RespChannelInfo:     return QStringLiteral("CHANNEL_INFO");
    case RespOk:             return QStringLiteral("OK");
    case RespErr:            return QStringLiteral("ERR");
    case RespContactsStart:  return QStringLiteral("CONTACTS_START");
    case RespContact:        return QStringLiteral("CONTACT");
    case RespEndOfContacts:  return QStringLiteral("END_OF_CONTACTS");
    case RespSelfInfo:       return QStringLiteral("SELF_INFO");
    case RespSent:           return QStringLiteral("SENT");
    case RespContactMsgRecv: return QStringLiteral("CONTACT_MSG_RECV");
    case RespChannelMsgRecv: return QStringLiteral("CHANNEL_MSG_RECV_8");
    case RespCurrTime:       return QStringLiteral("CURR_TIME");
    case RespNoMoreMessages: return QStringLiteral("NO_MORE_MESSAGES");
    case RespDeviceInfo:     return QStringLiteral("DEVICE_INFO");
    case PushAdvert:         return QStringLiteral("PUSH_ADVERT");
    case PushPathUpdated:    return QStringLiteral("PUSH_PATH_UPDATED");
    case PushSendConfirmed:  return QStringLiteral("PUSH_SEND_CONFIRMED");
    case PushMsgWaiting:     return QStringLiteral("PUSH_MSG_WAITING");
    case PushRawData:        return QStringLiteral("PUSH_RAW_DATA");
    case PushLoginSuccess:   return QStringLiteral("PUSH_LOGIN_SUCCESS");
    case PushLoginFail:      return QStringLiteral("PUSH_LOGIN_FAIL");
    case PushStatusResponse: return QStringLiteral("PUSH_STATUS_RESPONSE");
    case PushLogRxData:      return QStringLiteral("PUSH_LOG_RX_DATA");
    case PushTraceData:      return QStringLiteral("PUSH_TRACE_DATA");
    case PushNewAdvert:      return QStringLiteral("PUSH_NEW_ADVERT");
    default:
        return QStringLiteral("UNKNOWN(0x%1)").arg(code, 2, 16, QLatin1Char('0'));
    }
}

QString CompanionProtocol::trailingText(const QByteArray &payload, int minLength)
{
    int start = payload.size();
    while (start > 0) {
        const unsigned char c = static_cast<unsigned char>(payload.at(start - 1));
        // Accept printable ASCII plus UTF-8 continuation/lead bytes.
        if (c == 0x00 || c < 0x20 || c == 0x7F)
            break;
        --start;
    }
    if (payload.size() - start < minLength)
        return QString();
    return QString::fromUtf8(payload.mid(start)).trimmed();
}

void CompanionProtocol::emitFrame(const QByteArray &frame)
{
    emit traffic(QStringLiteral("TX %1: %2")
                     .arg(commandName(quint8(frame.at(0))))
                     .arg(QString::fromLatin1(frame.toHex(' '))));
    qDebug() << "Companion TX" << commandName(quint8(frame.at(0))) << frame.toHex(' ');
    emit writeRequested(frame);
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void CompanionProtocol::sendAppStart(const QString &appName)
{
    // 01 <app_ver> <6 reserved bytes> <app name>
    QByteArray frame;
    frame.append(char(CmdAppStart));
    frame.append(char(1));              // app protocol version
    frame.append(6, char(0));           // reserved
    frame.append(appName.left(16).toUtf8());
    emitFrame(frame);
}

void CompanionProtocol::requestChannel(int channelIdx)
{
    m_pendingChannelQuery = channelIdx;
    QByteArray frame;
    frame.append(char(CmdGetChannel));
    frame.append(char(channelIdx & 0xFF));
    emitFrame(frame);
}

void CompanionProtocol::sendDeviceQuery()
{
    QByteArray frame;
    frame.append(char(CmdDeviceQuery));
    frame.append(char(3));          // app protocol version we understand
    emitFrame(frame);
}

void CompanionProtocol::addUpdateContact(const MeshContact &c)
{
    // 09 <pub_key:32> <type:1> <flags:1> <out_path_len:i8> <out_path:64>
    //    <adv_name:32, NUL-padded> <last_advert:u32> <adv_lat:i32> <adv_lon:i32>
    if (c.publicKey.size() != 32)
        return;
    QByteArray path = c.outPath.left(64);
    path.append(64 - path.size(), char(0));
    QByteArray name = c.name.toUtf8().left(31);
    name.append(32 - name.size(), char(0));

    QByteArray frame;
    frame.append(char(CmdAddUpdateContact));
    frame.append(c.publicKey);
    frame.append(char(c.type));
    frame.append(char(c.flags));
    frame.append(char(c.outPathLen));
    frame.append(path);
    frame.append(name);
    appendLE32(frame, c.lastAdvert);
    appendLE32(frame, quint32(qint32(qRound(c.lat * 1e6))));
    appendLE32(frame, quint32(qint32(qRound(c.lon * 1e6))));
    emitFrame(frame);
}

void CompanionProtocol::removeContact(const QByteArray &publicKey)
{
    if (publicKey.size() != 32)
        return;
    QByteArray frame;
    frame.append(char(CmdRemoveContact));
    frame.append(publicKey);
    emitFrame(frame);
}

void CompanionProtocol::setChannel(int channelIdx, const QString &name,
                                   const QByteArray &secret16)
{
    // 20 <idx> <name:32, NUL-padded> <secret:16>  = 50 bytes
    QByteArray nameField = name.toUtf8().left(31);   // keep a terminating NUL
    nameField.append(32 - nameField.size(), char(0));
    QByteArray secret = secret16.left(16);
    secret.append(16 - secret.size(), char(0));

    QByteArray frame;
    frame.append(char(CmdSetChannel));
    frame.append(char(channelIdx & 0xFF));
    frame.append(nameField);
    frame.append(secret);
    // Not emitFrame(): the secret must not end up in the traffic log.
    emit traffic(QStringLiteral("TX SET_CHANNEL: %1 \"%2\" <secret hidden>")
                     .arg(channelIdx).arg(name));
    qDebug() << "Companion TX SET_CHANNEL" << channelIdx << name;
    emit writeRequested(frame);
}

void CompanionProtocol::requestContacts(quint32 since)
{
    QByteArray frame;
    frame.append(char(CmdGetContacts));
    if (since > 0)
        appendLE32(frame, since);
    emitFrame(frame);
}

void CompanionProtocol::setDeviceTime()
{
    QByteArray frame;
    frame.append(char(CmdSetDeviceTime));
    appendLE32(frame, quint32(QDateTime::currentSecsSinceEpoch()));
    emitFrame(frame);
}

void CompanionProtocol::sendSelfAdvert(bool flood)
{
    QByteArray frame;
    frame.append(char(CmdSendSelfAdvert));
    frame.append(char(flood ? 1 : 0));
    emitFrame(frame);
}

void CompanionProtocol::sendLogin(const QByteArray &publicKey, const QString &password)
{
    // 1a <pub_key:32> <password, max 15 bytes>
    if (publicKey.size() != 32)
        return;
    QByteArray frame;
    frame.append(char(CmdSendLogin));
    frame.append(publicKey);
    frame.append(password.toUtf8().left(15));
    // Not emitFrame(): that would put the password into the traffic log.
    emit traffic(QStringLiteral("TX SEND_LOGIN: %1 <password hidden>")
                     .arg(QString::fromLatin1(publicKey.left(6).toHex())));
    emit writeRequested(frame);
}

void CompanionProtocol::sendStatusRequest(const QByteArray &publicKey)
{
    if (publicKey.size() != 32)
        return;
    QByteArray frame;
    frame.append(char(CmdSendStatusReq));
    frame.append(publicKey);
    emitFrame(frame);
}

void CompanionProtocol::sendDirectText(const QByteArray &publicKey, const QString &text,
                                       quint8 txtType)
{
    // 02 <txt_type> <attempt> <timestamp LE32> <pubkey_prefix:6> <text>
    if (publicKey.size() < 6 || text.isEmpty())
        return;
    QByteArray frame;
    frame.append(char(CmdSendTxtMsg));
    frame.append(char(txtType));
    frame.append(char(0));                           // attempt
    appendLE32(frame, quint32(QDateTime::currentSecsSinceEpoch()));
    frame.append(publicKey.left(6));
    frame.append(text.toUtf8().left(140));
    emitFrame(frame);
}

void CompanionProtocol::syncNextMessage()
{
    QByteArray frame;
    frame.append(char(CmdSyncNextMessage));
    emitFrame(frame);
}

void CompanionProtocol::sendChannelText(int channelIdx, const QString &text)
{
    // 03 <txt_type> <channel_idx> <timestamp LE32> <utf8 text>
    QByteArray frame;
    frame.append(char(CmdSendChannelTxtMsg));
    frame.append(char(0));                       // txt_type 0 = plain
    frame.append(char(channelIdx & 0xFF));
    appendLE32(frame, quint32(QDateTime::currentSecsSinceEpoch()));

    // The negotiated ATT MTU on this link carries ~150 byte frames; keep well
    // inside that so a long message cannot be silently truncated.
    frame.append(text.toUtf8().left(130));
    emitFrame(frame);
}

// ---------------------------------------------------------------------------
// Incoming frames
// ---------------------------------------------------------------------------

bool CompanionProtocol::parseContact(const QByteArray &payload, MeshContact *out)
{
    // <pub_key:32> <type:1> <flags:1> <out_path_len:i8> <out_path:64>
    // <adv_name:32, NUL-terminated> <last_advert:u32> <adv_lat:i32>
    // <adv_lon:i32> <lastmod:u32>                      = 147 bytes
    // Name + last_advert (offset 131..135) are the minimum we can use; the
    // trailing location/lastmod fields are optional in case the BLE MTU cut
    // the frame short.
    constexpr int kName = 32 + 3 + 64;
    constexpr int kLastAdvert = kName + 32;
    if (payload.size() < kLastAdvert + 4)
        return false;

    MeshContact c;
    c.publicKey = payload.left(32);
    c.type = quint8(payload.at(32));
    c.flags = quint8(payload.at(33));
    c.outPathLen = qint8(payload.at(34));
    c.outPath = payload.mid(35, 64);

    const QByteArray nameField = payload.mid(kName, 32);
    const int nul = nameField.indexOf('\0');
    c.name = QString::fromUtf8(nul >= 0 ? nameField.left(nul) : nameField).trimmed();

    c.lastAdvert = readLE32(payload, kLastAdvert);
    if (payload.size() >= kLastAdvert + 12) {
        c.lat = qint32(readLE32(payload, kLastAdvert + 4)) / 1e6;
        c.lon = qint32(readLE32(payload, kLastAdvert + 8)) / 1e6;
    }
    if (payload.size() >= kLastAdvert + 16)
        c.lastMod = readLE32(payload, kLastAdvert + 12);

    *out = c;
    return true;
}

void CompanionProtocol::handleDirectMessage(const QByteArray &payload, int prefixOffset)
{
    // ... <pubkey_prefix:6> <path_len:1> <txt_type:1> <timestamp:u32> <text>
    const int textOffset = prefixOffset + 6 + 1 + 1 + 4;
    if (payload.size() < textOffset)
        return;
    const QByteArray prefix = payload.mid(prefixOffset, 6);
    const quint8 txtType = quint8(payload.at(prefixOffset + 7));
    const quint32 ts = readLE32(payload, prefixOffset + 8);
    QByteArray body = payload.mid(textOffset);
    // Signed posts (room servers) carry a 4-byte author prefix before the text.
    if (txtType == TxtSignedPlain && body.size() >= 4)
        body = body.mid(4);
    const QString text = QString::fromUtf8(body).trimmed();
    qDebug() << "Companion: DM from" << prefix.toHex() << "type" << txtType << text;
    emit contactMessageReceived(prefix, txtType, text, ts);
}

// ---------------------------------------------------------------------------
// Message path: RX log decoding and correlation
// ---------------------------------------------------------------------------

void CompanionProtocol::recordRxLog(const QByteArray &payload)
{
    // 0x88 payload: <snr*4:i8> <rssi:i8> <raw LoRa packet>
    // Raw packet:   <header> [transport codes:4] <path_len> <path> <payload>
    //   header   = 0bVVTTTTRR (RR route type, TTTT payload type)
    //   path_len = 0bSSCCCCCC (CCCCCC hop count, SS hash size - 1)
    if (payload.size() < 4)
        return;
    const QByteArray raw = payload.mid(2);
    const quint8 header = quint8(raw.at(0));
    const int route = header & 0x03;
    const int payloadType = (header >> 2) & 0x0F;
    constexpr int kPayloadGrpTxt = 0x05;
    if (payloadType != kPayloadGrpTxt)
        return;

    int off = 1;
    if (route == 0x00 || route == 0x03)      // TRANSPORT_FLOOD / TRANSPORT_DIRECT
        off += 4;
    if (raw.size() <= off)
        return;
    const quint8 pathLen = quint8(raw.at(off));
    const int hashSize = (pathLen >> 6) + 1;
    const int count = pathLen & 0x3F;
    const int pathBytes = count * hashSize;
    const int payloadOff = off + 1 + pathBytes;
    // GRP_TXT payload: <channel_hash:1> <mac:2> <ciphertext>
    if (hashSize > 3 || raw.size() < payloadOff + 3)
        return;

    RxGroupPacket p;
    p.receivedMs = QDateTime::currentMSecsSinceEpoch();
    p.pathLenByte = pathLen;
    p.hashSize = hashSize;
    p.path = raw.mid(off + 1, pathBytes);
    p.channelHash = quint8(raw.at(payloadOff));
    p.cipherLen = raw.size() - payloadOff - 3;
    p.snr = qint8(payload.at(0)) / 4.0;
    p.rssi = qint8(payload.at(1));

    // Keep only recent packets: the decrypted message follows within a
    // second or two (PUSH_MSG_WAITING -> SYNC_NEXT_MESSAGE).
    const qint64 cutoff = p.receivedMs - 30000;
    while (!m_rxGroupPackets.isEmpty() && m_rxGroupPackets.first().receivedMs < cutoff)
        m_rxGroupPackets.removeFirst();
    if (m_rxGroupPackets.size() >= 64)
        m_rxGroupPackets.removeFirst();
    m_rxGroupPackets.append(p);
}

MessagePath CompanionProtocol::resolvePath(int channelIdx, quint8 pathLenByte, int textBytes)
{
    MessagePath mp;
    if (pathLenByte == 0xFF) {
        mp.direct = true;
        return mp;
    }
    mp.hops = pathLenByte & 0x3F;
    mp.hashSize = (pathLenByte >> 6) + 1;

    // Candidate RX log packets: same channel hash (when known), same path_len
    // byte, recent and not yet used. Group text plaintext is
    // <timestamp:4> <flags:1> <"name: text">, AES-padded to 16-byte blocks,
    // which pins the ciphertext length - preferred, but only as a tie
    // breaker, in case a firmware pads differently. Among equal candidates the
    // OLDEST wins: the node keeps the first copy of a flood message and drops
    // the duplicates that arrive later via other repeaters.
    const int expectedCipher = ((5 + textBytes + 15) / 16) * 16;
    const bool hashKnown = m_channelHashes.contains(channelIdx);
    const quint8 wantHash = m_channelHashes.value(channelIdx);
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - 30000;

    int best = -1;
    bool bestSizeMatch = false;
    for (int i = 0; i < m_rxGroupPackets.size(); ++i) {
        const RxGroupPacket &p = m_rxGroupPackets.at(i);
        if (p.used || p.receivedMs < cutoff || p.pathLenByte != pathLenByte)
            continue;
        if (hashKnown && p.channelHash != wantHash)
            continue;
        const bool sizeMatch = (p.cipherLen == expectedCipher);
        if (best < 0 || (sizeMatch && !bestSizeMatch)) {
            best = i;
            bestSizeMatch = sizeMatch;
        }
    }
    if (best < 0)
        return mp;

    RxGroupPacket &hit = m_rxGroupPackets[best];
    hit.used = true;
    // Later copies of the same message (same channel + size) are duplicates
    // the node dropped; retire them so they cannot be matched to the next one.
    for (RxGroupPacket &p : m_rxGroupPackets) {
        if (!p.used && p.channelHash == hit.channelHash && p.cipherLen == hit.cipherLen
            && p.receivedMs >= hit.receivedMs && p.receivedMs - hit.receivedMs < 10000)
            p.used = true;
    }

    for (int i = 0; i + hit.hashSize <= hit.path.size(); i += hit.hashSize)
        mp.hashes.append(hit.path.mid(i, hit.hashSize));
    mp.pathKnown = true;
    mp.snr = hit.snr;
    mp.rssi = hit.rssi;
    mp.radioKnown = true;
    return mp;
}

void CompanionProtocol::handleFrame(const QByteArray &frame)
{
    if (frame.isEmpty())
        return;

    const quint8 code = quint8(frame.at(0));
    const QByteArray payload = frame.mid(1);

    // The RX log is a firehose - every LoRa packet the node overhears. Keep it
    // out of the UI, but keep a one-line trace with the radio metadata.
    if (code == PushLogRxData) {
        recordRxLog(payload);
        if (payload.size() >= 2) {
            const int snrQuarters = static_cast<qint8>(payload.at(0));
            const int rssi = static_cast<qint8>(payload.at(1));
            emit traffic(QStringLiteral("RX_LOG  %1 bytes  SNR %2 dB  RSSI %3 dBm")
                             .arg(payload.size() - 2)
                             .arg(snrQuarters / 4.0, 0, 'f', 1)
                             .arg(rssi));
        }
        return;
    }

    emit traffic(QStringLiteral("RX %1: %2")
                     .arg(responseName(code))
                     .arg(QString::fromLatin1(payload.toHex(' '))));
    qDebug() << "Companion RX" << responseName(code) << payload.toHex(' ');

    switch (code) {
    case RespSelfInfo: {
        // Layout drifts between firmware revisions; the node name is the
        // trailing string in every revision seen so far.
        const QString name = trailingText(payload, 1);
        qDebug() << "Companion: SELF_INFO, node name =" << name;
        emit selfInfoReceived(name);
        break;
    }

    case RespChannelInfo: {
        // <channel_idx:1> <name, NUL-terminated> <channel secret>
        // The name field is fixed width and NOT zeroed by the firmware, so it
        // must be cut at the first NUL - reading to the end of the payload
        // drags the secret in with it.
        if (payload.isEmpty())
            break;
        const int idx = int(quint8(payload.at(0)));
        const QByteArray nameField = payload.mid(1);
        const int nul = nameField.indexOf('\0');
        const QString name = QString::fromUtf8(
            nul >= 0 ? nameField.left(nul) : nameField.left(16));
        qDebug() << "Companion: channel" << idx << "=" << name;
        // <idx:1> <name:32> <secret:16>. The on-air channel hash is the first
        // byte of SHA-256(secret); it identifies the channel's raw packets in
        // the RX log (verified: "#ping" -> 0x28, "#test" -> 0xd9).
        if (!name.isEmpty() && payload.size() >= 1 + 32 + 16) {
            const QByteArray secret = payload.mid(33, 16);
            m_channelHashes[idx] = quint8(
                QCryptographicHash::hash(secret, QCryptographicHash::Sha256).at(0));
        } else {
            m_channelHashes.remove(idx);
        }
        // Empty slots are reported too, so the app knows which are free.
        emit channelInfoReceived(idx, name);
        break;
    }

    case RespDeviceInfo: {
        // <fw_ver:1> [v3+: <max_contacts/2:1> <max_channels:1> <ble_pin:4> ...]
        const int fwVer = payload.isEmpty() ? 0 : int(quint8(payload.at(0)));
        const int maxChannels = (fwVer >= 3 && payload.size() >= 3)
                                    ? int(quint8(payload.at(2))) : 0;
        const int maxContacts = (fwVer >= 3 && payload.size() >= 2)
                                    ? int(quint8(payload.at(1))) * 2 : 0;
        qDebug() << "Companion: DEVICE_INFO fw" << fwVer << "max channels" << maxChannels
                 << "max contacts" << maxContacts;
        emit deviceInfoReceived(fwVer, maxChannels, maxContacts);
        break;
    }

    case RespChannelMsgRecv:
    case RespChannelMsgRecv2: {
        // Two layouts, chosen by the node per frame:
        //  code 0x08 (v1/v2): <channel_idx:1> <path_len:1> <txt_type:1>
        //                     <sender_timestamp LE32> <text...>
        //  code 0x11 (v3):    <snr:1> <rsvd:2> <channel_idx:1> <path_len:1>
        //                     <txt_type:1> <sender_timestamp LE32> <text...>
        // Parsing a 0x08 frame with the v3 offsets read a timestamp byte as
        // the channel (random "Ch191") and cut 3 chars off the sender name.
        const int base = (code == RespChannelMsgRecv2) ? 3 : 0;
        const int textOffset = base + 7;
        if (payload.size() <= textOffset)
            break;
        const int idx = int(quint8(payload.at(base)));
        const quint32 ts = readLE32(payload, base + 3);
        // The text starts at a fixed offset. Hunting for the trailing
        // printable run instead drags timestamp bytes in: 6a cf b9 cb all
        // pass a naive "is printable" test and show up as garbage in front
        // of the message.
        const QByteArray textBytes = payload.mid(textOffset);
        const QString text = QString::fromUtf8(textBytes).trimmed();
        const MessagePath path = resolvePath(idx, quint8(payload.at(base + 1)), textBytes.size());
        qDebug() << "Companion: channel msg, idx" << idx << "hops" << path.hops
                 << (path.pathKnown ? "path known" : "path unknown") << "text" << text;
        if (!text.isEmpty())
            emit channelMessageReceived(idx, text, ts, path);
        // Keep draining until the node says there is nothing left.
        syncNextMessage();
        break;
    }

    case RespContactMsgRecv:
        // v1/v2: <pubkey_prefix:6> ...
        handleDirectMessage(payload, 0);
        syncNextMessage();
        break;

    case RespContactMsgRecv3:
        // v3: <snr:1> <reserved:2> <pubkey_prefix:6> ...
        handleDirectMessage(payload, 3);
        syncNextMessage();
        break;

    case RespContactsStart:
        emit contactsSyncStarted(int(readLE32(payload, 0)));
        break;

    case RespContact:
    case PushNewAdvert: {
        MeshContact contact;
        if (parseContact(payload, &contact)) {
            // PUSH_NEW_ADVERT is sent for adverts the node did NOT add to its
            // table (manual-add mode, or the table is full).
            contact.onNode = (code == RespContact);
            qDebug() << "Companion: contact" << contact.name
                     << ContactListModel::typeName(contact.type)
                     << "hops" << contact.outPathLen;
            emit contactReceived(contact);
        } else {
            qWarning() << "Companion: short CONTACT frame," << payload.size() << "bytes";
        }
        break;
    }

    case RespEndOfContacts:
        emit contactsSyncFinished(readLE32(payload, 0));
        break;

    case PushAdvert:
    case PushPathUpdated:
        emit contactChanged(payload.left(32));
        break;

    case PushLoginSuccess:
        // <permissions:1> <pub_key_prefix:6> <tag:i32> [new_permissions:1]
        emit loginResult(payload.mid(1, 6), true,
                         !payload.isEmpty() && (quint8(payload.at(0)) & 0x01));
        break;

    case PushLoginFail:
        // <reserved:1> <pub_key_prefix:6>
        emit loginResult(payload.mid(1, 6), false, false);
        break;

    case PushStatusResponse:
        // <reserved:1> <pub_key_prefix:6> <status_data...>
        emit statusResponse(payload.mid(1, 6), payload.mid(7));
        break;

    case RespNoMoreMessages:
        emit noMoreMessages();
        break;

    case RespSent:
        // <type:1, 1 = flood> <expected_ack_or_tag:4> <suggested_timeout:u32>
        emit sendAccepted(readLE32(payload, 5));
        if (payload.size() >= 5)
            emit sendQueued(payload.mid(1, 4), !payload.isEmpty() && payload.at(0) == 1,
                            readLE32(payload, 5));
        emit sendAcknowledged();
        break;

    case RespOk:
        emit okReceived();
        emit sendAcknowledged();
        break;

    case RespErr: {
        const int err = payload.isEmpty() ? 0 : int(quint8(payload.at(0)));
        QString why;
        switch (err) {
        case 1: why = tr("unsupported command"); break;
        case 2: why = tr("not found"); break;
        case 3: why = tr("table full"); break;
        case 4: why = tr("bad state"); break;
        case 5: why = tr("storage error"); break;
        case 6: why = tr("illegal argument"); break;
        default: break;
        }
        emit errorReported(why.isEmpty()
                               ? tr("Node rejected the last command")
                               : tr("Node rejected the last command (%1)").arg(why));
        break;
    }

    case PushMsgWaiting:
        // The node is holding a message for us - go fetch it.
        qDebug() << "Companion: PushMsgWaiting received - fetching next message";
        emit traffic(QStringLiteral("PUSH_MSG_WAITING - syncing next message"));
        syncNextMessage();
        break;

    case PushSendConfirmed:
        // <ack_code:4> <round_trip_ms:u32>
        if (payload.size() >= 4)
            emit sendConfirmed(payload.left(4), readLE32(payload, 4));
        emit sendAcknowledged();
        break;

    default:
        break;
    }
}

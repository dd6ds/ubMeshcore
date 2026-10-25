// CompanionProtocol, part 2: frames coming from the node.
//
// handleFrame() splits replies to our commands (codes < 0x80) from pushes the
// node sends on its own (codes >= 0x80) and turns each into a signal. Channel
// messages get their route from the node's RX log (path correlation below).

#include "companionprotocol.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>

namespace {

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

void CompanionProtocol::handleFrame(const QByteArray &frame)
{
    if (frame.isEmpty())
        return;

    const quint8 code = quint8(frame.at(0));
    const QByteArray payload = frame.mid(1);

    // The RX log is a firehose - every LoRa packet the node overhears. Keep it
    // out of the UI, but keep a one-line trace with the radio metadata.
    if (code == PushLogRxData) {
        handleRxLog(payload);
        return;
    }

    emit traffic(QStringLiteral("RX %1: %2")
                     .arg(responseName(code))
                     .arg(QString::fromLatin1(payload.toHex(' '))));
    qDebug() << "CompanionProtocol: RX" << responseName(code) << payload.toHex(' ');

    if (code >= PushAdvert)
        handlePush(code, payload);
    else
        handleReply(code, payload);
}

void CompanionProtocol::handleReply(quint8 code, const QByteArray &payload)
{
    switch (code) {
    case RespSelfInfo:
        // Layout drifts between firmware revisions; the node name is the
        // trailing string in every revision seen so far.
        emit selfInfoReceived(trailingText(payload, 1));
        break;
    case RespChannelInfo:
        handleChannelInfo(payload);
        break;
    case RespDeviceInfo:
        handleDeviceInfo(payload);
        break;
    case RespBattAndStorage:
        // <battery_mv:u16> [<used_kb:u32> <total_kb:u32>]
        if (payload.size() >= 2)
            emit batteryReceived(int(quint8(payload.at(0))) | (int(quint8(payload.at(1))) << 8));
        break;
    case RespChannelMsgRecv:
    case RespChannelMsgRecv2:
        handleChannelMessage(code, payload);
        syncNextMessage();      // keep draining until the node says it is empty
        break;
    case RespContactMsgRecv:
    case RespContactMsgRecv3:
        // v3 puts <snr:1> <reserved:2> in front of the v1/v2 layout.
        handleDirectMessage(payload, code == RespContactMsgRecv3 ? 3 : 0);
        syncNextMessage();
        break;
    case RespContactsStart:
        emit contactsSyncStarted(int(readLE32(payload, 0)));
        break;
    case RespContact:
        handleContact(payload, true);
        break;
    case RespEndOfContacts:
        emit contactsSyncFinished(readLE32(payload, 0));
        break;
    case RespNoMoreMessages:
        emit noMoreMessages();
        break;
    case RespSent:
        handleSent(payload);
        break;
    case RespOk:
        emit okReceived();
        emit sendAcknowledged();
        break;
    case RespErr:
        handleError(payload);
        break;
    default:
        break;
    }
}

void CompanionProtocol::handlePush(quint8 code, const QByteArray &payload)
{
    switch (code) {
    case PushNewAdvert:
        // Sent for adverts the node did NOT add to its table (manual-add
        // mode, or the table is full).
        handleContact(payload, false);
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
    case PushMsgWaiting:
        // The node is holding a message for us - go fetch it.
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

// ---------------------------------------------------------------------------
// Replies and pushes with a payload worth parsing
// ---------------------------------------------------------------------------

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

void CompanionProtocol::handleChannelInfo(const QByteArray &payload)
{
    // <channel_idx:1> <name:32, NUL-terminated> <secret:16>
    // The name field is not zeroed by the firmware, so it must be cut at the
    // first NUL - reading to the end of the payload drags the secret in with it.
    if (payload.isEmpty())
        return;
    const int idx = int(quint8(payload.at(0)));
    const QByteArray nameField = payload.mid(1);
    const int nul = nameField.indexOf('\0');
    const QString name = QString::fromUtf8(nul >= 0 ? nameField.left(nul) : nameField.left(16));
    // The on-air channel hash is the first byte of SHA-256(secret); it
    // identifies the channel's raw packets in the RX log (verified: "#ping" ->
    // 0x28, "#test" -> 0xd9).
    if (!name.isEmpty() && payload.size() >= 1 + 32 + 16) {
        const QByteArray secret = payload.mid(33, 16);
        m_channelHashes[idx] = quint8(
            QCryptographicHash::hash(secret, QCryptographicHash::Sha256).at(0));
    } else {
        m_channelHashes.remove(idx);
    }
    // Empty slots are reported too, so the app knows which are free.
    emit channelInfoReceived(idx, name);
}

void CompanionProtocol::handleDeviceInfo(const QByteArray &payload)
{
    // <fw_ver:1> [v3+: <max_contacts/2:1> <max_channels:1> <ble_pin:4> ...]
    const int fwVer = payload.isEmpty() ? 0 : int(quint8(payload.at(0)));
    const int maxChannels = (fwVer >= 3 && payload.size() >= 3)
                                ? int(quint8(payload.at(2))) : 0;
    const int maxContacts = (fwVer >= 3 && payload.size() >= 2)
                                ? int(quint8(payload.at(1))) * 2 : 0;
    emit deviceInfoReceived(fwVer, maxChannels, maxContacts);
}

void CompanionProtocol::handleChannelMessage(quint8 code, const QByteArray &payload)
{
    // Two layouts, chosen by the node per frame:
    //  code 0x08 (v1/v2): <channel_idx:1> <path_len:1> <txt_type:1>
    //                     <sender_timestamp LE32> <text...>
    //  code 0x11 (v3):    <snr:1> <rsvd:2> <channel_idx:1> <path_len:1>
    //                     <txt_type:1> <sender_timestamp LE32> <text...>
    // Reading a 0x08 frame with the v3 offsets takes a timestamp byte as the
    // channel and cuts 3 characters off the sender name.
    const int base = (code == RespChannelMsgRecv2) ? 3 : 0;
    const int textOffset = base + 7;
    if (payload.size() <= textOffset)
        return;
    const int idx = int(quint8(payload.at(base)));
    const quint32 ts = readLE32(payload, base + 3);
    // The text starts at a fixed offset. Hunting for the trailing printable
    // run instead drags timestamp bytes in: 6a cf b9 cb all pass a naive
    // "is printable" test and show up as garbage in front of the message.
    const QByteArray textBytes = payload.mid(textOffset);
    const QString text = QString::fromUtf8(textBytes).trimmed();
    const MessagePath path = resolvePath(idx, quint8(payload.at(base + 1)), textBytes.size());
    if (!text.isEmpty())
        emit channelMessageReceived(idx, text, ts, path);
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
    emit contactMessageReceived(prefix, txtType, QString::fromUtf8(body).trimmed(), ts);
}

void CompanionProtocol::handleContact(const QByteArray &payload, bool onNode)
{
    MeshContact contact;
    if (!parseContact(payload, &contact)) {
        qWarning() << "CompanionProtocol: short CONTACT frame," << payload.size() << "bytes";
        return;
    }
    contact.onNode = onNode;
    emit contactReceived(contact);
}

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

void CompanionProtocol::handleSent(const QByteArray &payload)
{
    // <type:1, 1 = flood> <expected_ack_or_tag:4> <suggested_timeout:u32>
    emit sendAccepted(readLE32(payload, 5));
    if (payload.size() >= 5)
        emit sendQueued(payload.mid(1, 4), payload.at(0) == 1, readLE32(payload, 5));
    emit sendAcknowledged();
}

void CompanionProtocol::handleError(const QByteArray &payload)
{
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
}

// ---------------------------------------------------------------------------
// Message path: RX log decoding and correlation
// ---------------------------------------------------------------------------

void CompanionProtocol::handleRxLog(const QByteArray &payload)
{
    recordRxLog(payload);
    if (payload.size() < 2)
        return;
    const int snrQuarters = static_cast<qint8>(payload.at(0));
    const int rssi = static_cast<qint8>(payload.at(1));
    emit traffic(QStringLiteral("RX_LOG  %1 bytes  SNR %2 dB  RSSI %3 dBm")
                     .arg(payload.size() - 2)
                     .arg(snrQuarters / 4.0, 0, 'f', 1)
                     .arg(rssi));
}

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
    const qint64 cutoff = p.receivedMs - kRxLogKeepMs;
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

    const int best = findRxPacket(channelIdx, pathLenByte, textBytes);
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

int CompanionProtocol::findRxPacket(int channelIdx, quint8 pathLenByte, int textBytes) const
{
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
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - kRxLogKeepMs;

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
    return best;
}

#include "companionprotocol.h"

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
    case CmdSendSelfAdvert:    return QStringLiteral("SEND_SELF_ADVERT");
    case CmdSyncNextMessage:   return QStringLiteral("SYNC_NEXT_MESSAGE");
    case CmdDeviceQuery:       return QStringLiteral("DEVICE_QUERY");
    case CmdGetChannel:        return QStringLiteral("GET_CHANNEL");
    default:
        return QStringLiteral("CMD(0x%1)").arg(code, 2, 16, QLatin1Char('0'));
    }
}

QString CompanionProtocol::responseName(quint8 code)
{
    switch (code) {
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

void CompanionProtocol::requestContacts()
{
    QByteArray frame;
    frame.append(char(CmdGetContacts));
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

void CompanionProtocol::handleFrame(const QByteArray &frame)
{
    if (frame.isEmpty())
        return;

    const quint8 code = quint8(frame.at(0));
    const QByteArray payload = frame.mid(1);

    // The RX log is a firehose - every LoRa packet the node overhears. Keep it
    // out of the UI, but keep a one-line trace with the radio metadata.
    if (code == PushLogRxData) {
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
        if (!name.isEmpty())
            emit channelInfoReceived(idx, name);
        break;
    }

    case RespChannelMsgRecv:
    case RespChannelMsgRecv2: {
        // <snr:1> <rsvd:2> <channel_idx:1> <path_len:1> <txt_type:1>
        // <sender_timestamp LE32> <text...>
        const int idx = payload.size() > 3 ? int(quint8(payload.at(3))) : 0;
        const quint32 ts = readLE32(payload, 6);
        // The text starts at a fixed offset. Hunting for the trailing
        // printable run instead drags timestamp bytes in: 6a cf b9 cb all
        // pass a naive "is printable" test and show up as garbage in front
        // of the message.
        const QString text = payload.size() > 10
                                 ? QString::fromUtf8(payload.mid(10)).trimmed()
                                 : trailingText(payload, 1);
        qDebug() << "Companion: channel msg, idx" << idx << "text" << text;
        if (!text.isEmpty())
            emit channelMessageReceived(idx, text, ts);
        // Keep draining until the node says there is nothing left.
        syncNextMessage();
        break;
    }

    case RespContactMsgRecv: {
        const quint32 ts = readLE32(payload, 8);
        const QString text = trailingText(payload, 1);
        emit contactMessageReceived(QStringLiteral("contact"), text, ts);
        syncNextMessage();
        break;
    }

    case RespNoMoreMessages:
        break;

    case RespSent:
    case RespOk:
        emit sendAcknowledged();
        break;

    case RespErr:
        emit errorReported(QStringLiteral("Node rejected the last command"));
        break;

    case PushMsgWaiting:
        // The node is holding a message for us - go fetch it.
        syncNextMessage();
        break;

    case PushSendConfirmed:
        emit sendAcknowledged();
        break;

    default:
        break;
    }
}

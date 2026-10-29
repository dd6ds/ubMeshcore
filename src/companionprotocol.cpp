// CompanionProtocol, part 1: the commands the app sends to the node, and the
// names used for commands and replies in the traffic log.
//
// Frames coming from the node are parsed in companionprotocol_rx.cpp.

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
    case CmdGetBattAndStorage: return QStringLiteral("GET_BATT_AND_STORAGE");
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
    case RespChannelDataRecv: return QStringLiteral("CHANNEL_DATA_RECV");
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
    case RespBattAndStorage: return QStringLiteral("BATT_AND_STORAGE");
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


void CompanionProtocol::emitFrame(const QByteArray &frame)
{
    emit traffic(QStringLiteral("TX %1: %2")
                     .arg(commandName(quint8(frame.at(0))))
                     .arg(QString::fromLatin1(frame.toHex(' '))));
    qDebug() << "CompanionProtocol: TX" << commandName(quint8(frame.at(0))) << frame.toHex(' ');
    emit writeRequested(frame);
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void CompanionProtocol::sendAppStart(const QString &appName)
{
    // 01 <app_ver> <6 reserved bytes> <app name>
    m_syncsInFlight = 0;                // new session, nothing outstanding
    QByteArray frame;
    frame.append(char(CmdAppStart));
    frame.append(char(1));              // app protocol version
    frame.append(6, char(0));           // reserved
    frame.append(appName.left(16).toUtf8());
    emitFrame(frame);
}

void CompanionProtocol::requestChannel(int channelIdx)
{
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

void CompanionProtocol::requestBattery()
{
    QByteArray frame;
    frame.append(char(CmdGetBattAndStorage));
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
    qDebug() << "CompanionProtocol: TX SET_CHANNEL" << channelIdx << name;
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
    ++m_syncsInFlight;
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

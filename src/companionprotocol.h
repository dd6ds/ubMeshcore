#ifndef COMPANIONPROTOCOL_H
#define COMPANIONPROTOCOL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

/// MeshCore "companion radio" protocol, as spoken over the Nordic UART link.
///
/// Frame layout: one leading code byte, then payload. There is no length
/// prefix - each BLE write/notification carries exactly one frame (confirmed
/// by the 1-byte 0x83 frames seen on the wire).
///
/// Codes >= 0x80 are unsolicited pushes from the node, codes < 0x80 are
/// replies to a command we sent.
class CompanionProtocol : public QObject
{
    Q_OBJECT

public:
    enum Cmd : quint8 {
        CmdAppStart          = 1,
        CmdSendTxtMsg        = 2,
        CmdSendChannelTxtMsg = 3,
        CmdGetContacts       = 4,
        CmdGetDeviceTime     = 5,
        CmdSendSelfAdvert    = 7,
        CmdSyncNextMessage   = 10,
        CmdDeviceQuery       = 22,
        CmdGetChannel        = 31
    };

    enum Resp : quint8 {
        RespOk             = 0,
        RespErr            = 1,
        RespContactsStart  = 2,
        RespContact        = 3,
        RespEndOfContacts  = 4,
        RespSelfInfo       = 5,
        RespSent           = 6,
        RespContactMsgRecv = 7,
        RespChannelMsgRecv = 8,
        RespCurrTime       = 9,
        RespNoMoreMessages = 10,
        // Verified on a live DD6DS-Mobile node: channel messages come back as
        // 0x11 and the channel table as 0x12, not as the lower numbers the
        // rest of this enum would suggest.
        RespChannelMsgRecv2 = 0x11,
        RespChannelInfo     = 0x12
    };

    enum Push : quint8 {
        PushAdvert         = 0x80,
        PushPathUpdated    = 0x81,
        PushSendConfirmed  = 0x82,
        PushMsgWaiting     = 0x83,
        PushRawData        = 0x84,
        PushLoginSuccess   = 0x85,
        PushLoginFail      = 0x86,
        PushStatusResponse = 0x87,
        PushLogRxData      = 0x88,
        PushTraceData      = 0x89,
        PushNewAdvert      = 0x8A
    };

    explicit CompanionProtocol(QObject *parent = nullptr);

    // --- commands (phone -> node) -----------------------------------------
    void sendAppStart(const QString &appName);
    void requestChannel(int channelIdx);
    void requestContacts();
    void syncNextMessage();
    void sendChannelText(int channelIdx, const QString &text);

    /// Feeds one NUS notification into the parser.
    void handleFrame(const QByteArray &frame);

    /// Command names (phone -> node). Kept apart from responseName() because
    /// the two numbering spaces overlap - 1 is APP_START outbound but ERR
    /// inbound, 10 is SYNC_NEXT_MESSAGE outbound but NO_MORE_MESSAGES inbound.
    static QString commandName(quint8 code);
    static QString responseName(quint8 code);

signals:
    /// Emitted with a complete frame that must go out on the NUS RX
    /// characteristic.
    void writeRequested(const QByteArray &data);

    void selfInfoReceived(const QString &name);
    void channelInfoReceived(int channelIdx, const QString &name);
    void channelMessageReceived(int channelIdx, const QString &text, quint32 timestamp);
    void contactMessageReceived(const QString &sender, const QString &text, quint32 timestamp);
    void sendAcknowledged();
    void errorReported(const QString &what);
    /// One human-readable line per frame, for the in-app diagnostics view.
    void traffic(const QString &line);

private:
    void emitFrame(const QByteArray &frame);
    /// Pulls the trailing printable run out of a payload. The exact offset of
    /// the text field differs between firmware revisions, so this is
    /// deliberately tolerant - the raw hex is logged alongside it.
    static QString trailingText(const QByteArray &payload, int minLength = 1);

    int m_pendingChannelQuery = -1;
};

#endif // COMPANIONPROTOCOL_H

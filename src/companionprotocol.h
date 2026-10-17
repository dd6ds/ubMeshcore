#ifndef COMPANIONPROTOCOL_H
#define COMPANIONPROTOCOL_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

#include <QHash>
#include <QList>
#include <QVector>

#include "contactmodel.h"

/// How a received message travelled through the mesh.
///
/// The companion message frame only carries the raw path_len byte
/// (0bSSCCCCCC: hop count + hash size - 1, or 0xFF for direct). The hop
/// hashes themselves are taken from the matching raw packet in the node's RX
/// log (0x88), when one was seen - not for messages from the offline cache.
struct MessagePath {
    bool direct = false;          // path_len 0xFF: sent direct, not flooded
    int hops = 0;                 // repeaters the message passed through
    int hashSize = 1;             // bytes per hop hash (1..3)
    QList<QByteArray> hashes;     // hop hashes, first repeater first
    bool pathKnown = false;       // hashes filled from the RX log
    double snr = 0.0;             // of the packet as our node received it
    int rssi = 0;
    bool radioKnown = false;
};

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
        CmdSetDeviceTime     = 6,
        CmdSendSelfAdvert    = 7,
        CmdSyncNextMessage   = 10,
        CmdDeviceQuery       = 22,
        CmdSendLogin         = 26,
        CmdSendStatusReq     = 27,
        CmdGetChannel        = 31
    };

    enum TxtType : quint8 {
        TxtPlain       = 0,
        TxtCliData     = 1,   // remote CLI command / reply
        TxtSignedPlain = 2    // room server post, 4-byte author prefix first
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
        RespContactMsgRecv3 = 0x10,
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
    /// since = most_recent_lastmod of the previous sync, 0 for a full sync.
    void requestContacts(quint32 since = 0);
    void syncNextMessage();
    void sendChannelText(int channelIdx, const QString &text);
    /// Sets the node clock to the phone's. Contact "last heard" times are
    /// stamped with the node clock, so this keeps them meaningful.
    void setDeviceTime();
    void sendSelfAdvert(bool flood);
    /// Login to a repeater / room server (password max 15 bytes).
    void sendLogin(const QByteArray &publicKey, const QString &password);
    void sendStatusRequest(const QByteArray &publicKey);
    /// Direct message; txtType TxtCliData sends a remote CLI command.
    void sendDirectText(const QByteArray &publicKey, const QString &text,
                        quint8 txtType = TxtPlain);

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
    void channelMessageReceived(int channelIdx, const QString &text, quint32 timestamp,
                                const MessagePath &path);
    /// Direct message from a contact. keyPrefix = first 6 bytes of the
    /// sender's public key; txtType tells CLI replies apart from chat.
    void contactMessageReceived(const QByteArray &keyPrefix, quint8 txtType,
                                const QString &text, quint32 timestamp);

    // Contact table sync (CMD_GET_CONTACTS) and advert pushes.
    void contactsSyncStarted(int total);
    void contactReceived(const MeshContact &contact);
    void contactsSyncFinished(quint32 mostRecentLastMod);
    /// PUSH_ADVERT / PUSH_PATH_UPDATED: something in the table changed.
    void contactChanged(const QByteArray &publicKey);

    // Repeater / room server administration.
    void loginResult(const QByteArray &keyPrefix, bool success, bool isAdmin);
    void statusResponse(const QByteArray &keyPrefix, const QByteArray &statusData);
    /// RESP_SENT: the node accepted a send; timeout is its RTT estimate.
    void sendAccepted(quint32 suggestedTimeoutMs);
    void sendAcknowledged();
    /// The node's offline message queue is empty (RESP_NO_MORE_MESSAGES).
    void noMoreMessages();
    void errorReported(const QString &what);
    /// One human-readable line per frame, for the in-app diagnostics view.
    void traffic(const QString &line);

private:
    void emitFrame(const QByteArray &frame);
    /// Pulls the trailing printable run out of a payload. The exact offset of
    /// the text field differs between firmware revisions, so this is
    /// deliberately tolerant - the raw hex is logged alongside it.
    static QString trailingText(const QByteArray &payload, int minLength = 1);
    /// Parses the RESP_CODE_CONTACT / PUSH_NEW_ADVERT body.
    static bool parseContact(const QByteArray &payload, MeshContact *out);
    void handleDirectMessage(const QByteArray &payload, int prefixOffset);

    // --- message path correlation -------------------------------------------
    /// A group-text packet (payload type GRP_TXT) seen in the RX log.
    struct RxGroupPacket {
        qint64 receivedMs = 0;
        quint8 channelHash = 0;
        quint8 pathLenByte = 0;
        QByteArray path;
        int hashSize = 1;
        int cipherLen = 0;        // encrypted payload length (AES blocks)
        double snr = 0.0;
        int rssi = 0;
        bool used = false;
    };
    void recordRxLog(const QByteArray &payload);
    /// Builds the path for a channel message, consuming the matching RX log
    /// entry if there is one.
    MessagePath resolvePath(int channelIdx, quint8 pathLenByte, int textBytes);

    QVector<RxGroupPacket> m_rxGroupPackets;    // last few seconds only
    QHash<int, quint8> m_channelHashes;         // channel idx -> wire hash

    int m_pendingChannelQuery = -1;
};

#endif // COMPANIONPROTOCOL_H

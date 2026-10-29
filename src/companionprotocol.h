// The MeshCore companion protocol: frame layouts, commands and parsed replies.

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
///
/// The implementation is split by direction:
///   companionprotocol.cpp     - commands sent to the node
///   companionprotocol_rx.cpp  - frames from the node, message path correlation
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
        CmdAddUpdateContact  = 9,
        CmdRemoveContact     = 15,
        CmdSyncNextMessage   = 10,
        CmdGetBattAndStorage = 20,
        CmdDeviceQuery       = 22,
        CmdSendLogin         = 26,
        CmdSendStatusReq     = 27,
        CmdGetChannel        = 31,
        CmdSetChannel        = 32
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
        RespBattAndStorage = 12,
        RespDeviceInfo     = 13,
        // Verified on a live DD6DS-Mobile node: channel messages come back as
        // 0x11 and the channel table as 0x12, not as the lower numbers the
        // rest of this enum would suggest.
        RespContactMsgRecv3 = 0x10,
        RespChannelMsgRecv2 = 0x11,
        RespChannelInfo     = 0x12,
        // A binary datagram on a channel (image chunks, app data - not text).
        // Firmware 1.15+ queues it with the messages, so it answers
        // SYNC_NEXT_MESSAGE like one.
        RespChannelDataRecv = 0x1B
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
    /// CMD_DEVICE_QUERY (app protocol v3) -> RESP_DEVICE_INFO with max_channels.
    void sendDeviceQuery();
    /// CMD_GET_BATT_AND_STORAGE -> RESP_BATT_AND_STORAGE (battery voltage).
    void requestBattery();
    /// CMD_ADD_UPDATE_CONTACT: store a contact in the node's table.
    void addUpdateContact(const MeshContact &contact);
    /// CMD_REMOVE_CONTACT: delete a contact from the node's table.
    void removeContact(const QByteArray &publicKey);
    /// CMD_SET_CHANNEL: <idx> <name:32, NUL-padded> <secret:16>. An empty name
    /// with an all-zero secret deletes the slot.
    void setChannel(int channelIdx, const QString &name, const QByteArray &secret16);
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
    void deviceInfoReceived(int firmwareVersion, int maxChannels, int maxContacts);
    /// Battery voltage of the node in millivolts.
    void batteryReceived(int milliVolts);
    /// Emitted for empty slots too (name empty), so free slots are known.
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
    /// RESP_SENT with its details: ack = expected ACK code (DMs) or tag,
    /// flood = sent flood instead of via a known path.
    void sendQueued(const QByteArray &ack, bool flood, quint32 suggestedTimeoutMs);
    /// PUSH_SEND_CONFIRMED: the recipient acknowledged the message with `ack`.
    void sendConfirmed(const QByteArray &ack, quint32 roundTripMs);
    void sendAcknowledged();
    /// Plain RESP_OK (not SENT/confirmations) - the node accepted the command.
    void okReceived();
    /// The node's offline message queue is empty (RESP_NO_MORE_MESSAGES).
    void noMoreMessages();
    void errorReported(const QString &what);
    /// One human-readable line per frame, for the in-app diagnostics view.
    void traffic(const QString &line);

private:
    void emitFrame(const QByteArray &frame);
    void handleReply(quint8 code, const QByteArray &payload);
    void handlePush(quint8 code, const QByteArray &payload);
    void handleChannelInfo(const QByteArray &payload);
    void handleDeviceInfo(const QByteArray &payload);
    void handleChannelMessage(quint8 code, const QByteArray &payload);
    void handleDirectMessage(const QByteArray &payload, int prefixOffset);
    void handleContact(const QByteArray &payload, bool onNode);
    void handleSent(const QByteArray &payload);
    void handleError(const QByteArray &payload);
    /// The node answered one SYNC_NEXT_MESSAGE with a queued item; ask for
    /// the next one so the drain runs on until NO_MORE_MESSAGES.
    void continueDrain();
    /// Pulls the trailing printable run out of a payload. The exact offset of
    /// the text field differs between firmware revisions, so this is
    /// deliberately tolerant - the raw hex is logged alongside it.
    static QString trailingText(const QByteArray &payload, int minLength = 1);
    /// Parses the RESP_CODE_CONTACT / PUSH_NEW_ADVERT body.
    static bool parseContact(const QByteArray &payload, MeshContact *out);

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
    /// How long an RX log packet can still be matched to a message.
    static constexpr qint64 kRxLogKeepMs = 30000;
    void handleRxLog(const QByteArray &payload);
    void recordRxLog(const QByteArray &payload);
    /// Builds the path for a channel message, consuming the matching RX log
    /// entry if there is one.
    MessagePath resolvePath(int channelIdx, quint8 pathLenByte, int textBytes);
    /// Index of the RX log packet that best matches a channel message, or -1.
    int findRxPacket(int channelIdx, quint8 pathLenByte, int textBytes) const;

    QVector<RxGroupPacket> m_rxGroupPackets;    // last few seconds only
    QHash<int, quint8> m_channelHashes;         // channel idx -> wire hash
    /// SYNC_NEXT_MESSAGE requests not answered yet. A reply the app does not
    /// know arriving while one is open is a queued item of a newer type; the
    /// drain must go past it instead of stopping there.
    int m_syncsInFlight = 0;
};

#endif // COMPANIONPROTOCOL_H

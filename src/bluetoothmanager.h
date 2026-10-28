// The app's backend: Bluetooth scanning, the BLE link and the MeshCore session.

#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QHash>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

#include "companionprotocol.h"
#include "devicelistmodel.h"

/// The app's backend, exposed to QML as "BluetoothManager".
///
/// The implementation is split by topic:
///   bluetoothmanager.cpp       - scanning, BLE connection, Nordic UART I/O
///   bluetoothmanager_mesh.cpp  - the MeshCore session on top of it: channels,
///                                messages, contacts, repeater administration
class BluetoothManager : public QObject
{
    Q_OBJECT

    // --- scanning ---------------------------------------------------------
    Q_PROPERTY(bool scanActive READ scanActive NOTIFY scanActiveChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool adapterReady READ adapterReady NOTIFY statusChanged)
    Q_PROPERTY(DeviceListModel *devicesModel READ devicesModel CONSTANT)
    // Selection lives here rather than in a QML page property, so it cannot
    // get lost between pages of the StackLayout.
    Q_PROPERTY(QString selectedAddress READ selectedAddress NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

    // --- connection -------------------------------------------------------
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectionChanged)
    Q_PROPERTY(QString connectedDevice READ connectedDevice NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(bool uartReady READ uartReady NOTIFY connectionChanged)
    Q_PROPERTY(QStringList trafficLog READ trafficLog NOTIFY trafficLogChanged)

    // --- node session -----------------------------------------------------
    Q_PROPERTY(QString nodeName READ nodeName NOTIFY nodeInfoChanged)
    Q_PROPERTY(bool sessionReady READ sessionReady NOTIFY nodeInfoChanged)
    /// Battery of the connected node; 0 / -1 while unknown.
    Q_PROPERTY(int batteryMilliVolts READ batteryMilliVolts NOTIFY batteryChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryChanged)
    Q_PROPERTY(QStringList channelNames READ channelNames NOTIFY channelsChanged)
    Q_PROPERTY(int maxChannels READ maxChannels NOTIFY channelsChanged)
    /// e.g. "3 message(s) fetched from node at 08:31" - last offline-queue drain.
    Q_PROPERTY(QString lastSyncInfo READ lastSyncInfo NOTIFY syncInfoChanged)

    // --- contacts ---------------------------------------------------------
    /// Contact table of the connected node, filled from received adverts.
    Q_PROPERTY(ContactListModel *contactsModel READ contactsModel CONSTANT)
    Q_PROPERTY(bool contactsSyncing READ contactsSyncing NOTIFY contactsSyncingChanged)
    Q_PROPERTY(int maxContacts READ maxContacts NOTIFY contactLimitChanged)
    /// Result of the last "Advert" tap, e.g. "✓ Flood advert sent at 09:41:07".
    Q_PROPERTY(QString lastAdvertInfo READ lastAdvertInfo NOTIFY advertInfoChanged)
    Q_PROPERTY(bool lastAdvertOk READ lastAdvertOk NOTIFY advertInfoChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager() override;

    bool scanActive() const { return m_scanActive; }
    QString status() const { return m_status; }
    bool adapterReady() const { return m_adapterReady; }
    DeviceListModel *devicesModel() { return &m_devices; }
    QString selectedAddress() const { return m_selectedAddress; }
    QString selectedName() const { return m_selectedName; }
    bool hasSelection() const { return !m_selectedAddress.isEmpty(); }

    bool connected() const { return m_connected; }
    bool connecting() const { return m_connecting; }
    QString connectedDevice() const { return m_connectedDevice; }
    QString connectionState() const { return m_connectionState; }
    bool uartReady() const { return m_uartReady; }
    QStringList trafficLog() const { return m_trafficLog; }

    QString nodeName() const { return m_nodeName; }
    bool sessionReady() const { return m_sessionReady; }
    int batteryMilliVolts() const { return m_batteryMv; }
    /// Estimated from the voltage (single LiPo cell), -1 = unknown.
    int batteryPercent() const;
    QStringList channelNames() const { return m_channelNames; }
    int maxChannels() const { return m_maxChannels; }
    QString lastSyncInfo() const { return m_lastSyncInfo; }

    ContactListModel *contactsModel() { return &m_contacts; }
    bool contactsSyncing() const { return m_contactsSyncing; }
    /// Size of the node's contact table (from DEVICE_INFO), 0 = unknown.
    int maxContacts() const { return m_maxContacts; }
    QString lastAdvertInfo() const { return m_lastAdvertInfo; }
    bool lastAdvertOk() const { return m_advertOk; }

    // --- scanning and connecting ------------------------------------------
    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    /// Called from the device list; the connect page then uses the selection.
    Q_INVOKABLE void selectDevice(const QString &address, const QString &name);
    Q_INVOKABLE bool connectToSelected();
    Q_INVOKABLE void disconnect();

    // --- channels and messages --------------------------------------------
    /// Sends a text message on a channel via the companion protocol.
    Q_INVOKABLE bool sendChannelMessage(int channelIdx, const QString &text);
    /// Fetches messages the node has queued (also done automatically).
    Q_INVOKABLE void syncNextMessage();
    /// Adds a hashtag channel ("#z46" or "z46") in the first free slot; the
    /// secret is derived from the name like every MeshCore client does
    /// (first 16 bytes of SHA-256("#name")). Returns the slot index, the
    /// existing index if the channel is already there, or -1 on error
    /// (reason via sendFailed).
    Q_INVOKABLE int addChannel(const QString &name);
    /// Clears a channel slot on the node (empty name, all-zero secret).
    Q_INVOKABLE bool removeChannel(int channelIdx);
    /// Slot index of a channel by name (case-sensitive), -1 if not present.
    Q_INVOKABLE int channelIndexOf(const QString &name) const;
    /// "#name" form of user input, or "" if it is not a usable channel name.
    Q_INVOKABLE QString normalizeChannelName(const QString &name) const;

    // --- contacts ---------------------------------------------------------
    /// Full re-sync of the node's contact table.
    Q_INVOKABLE void refreshContacts();
    /// Send our own advert so other nodes (and repeaters) learn about us.
    Q_INVOKABLE void sendAdvert(bool flood);
    Q_INVOKABLE bool sendDirectMessage(const QString &keyHex, const QString &text);
    /// Stores a contact (known only from an advert) in the node's table.
    Q_INVOKABLE bool saveContactToNode(const QString &keyHex);
    /// Deletes a contact: from the node's table (if stored there) and from
    /// the list. It comes back when its advert is heard again.
    Q_INVOKABLE bool removeContactFromNode(const QString &keyHex);
    /// Deletes several contacts; returns how many were deleted. Commands are
    /// paced by the write queue, so large batches are fine.
    Q_INVOKABLE int deleteContacts(const QStringList &keyHexes);

    // --- repeater / room server administration -----------------------------
    // keyHex = full public key from contactsModel.publicKey.
    Q_INVOKABLE bool loginToNode(const QString &keyHex, const QString &password);
    Q_INVOKABLE bool requestNodeStatus(const QString &keyHex);
    /// Remote CLI command, e.g. "ver", "clock", "neighbors", "set name X".
    /// Requires a successful admin login first.
    Q_INVOKABLE bool sendNodeCommand(const QString &keyHex, const QString &command);

signals:
    void scanActiveChanged();
    void statusChanged();
    void selectionChanged();
    void connectionChanged();
    void trafficLogChanged();
    void nodeInfoChanged();
    void batteryChanged();
    void channelsChanged();
    void syncInfoChanged();
    void contactsSyncingChanged();
    void contactLimitChanged();
    void advertInfoChanged();

    /// timestamp = sender's send time (epoch secs), sanity-checked.
    /// route = { hops, route (one-line summary), routeDetail (multi-line) }.
    void channelMessage(int channelIdx, const QString &text, const QString &time,
                        double timestamp, const QVariantMap &route);
    /// A channel slot was added or removed: any stored history and unread
    /// count for that index belong to the previous channel and must go.
    void channelSlotCleared(int channelIdx);
    /// Something the user asked for did not work; reason is ready to show.
    void sendFailed(const QString &reason);

    /// Direct message from a contact; timestamp = send time, epoch secs.
    void directMessage(const QString &keyHex, const QString &name,
                       const QString &text, double timestamp);
    /// Our DM was queued by the node; delivery is confirmed by dmDelivered
    /// with the same ack code (hex) - typically within timeoutMs.
    void dmSent(const QString &keyHex, const QString &ackHex, bool flood, int timeoutMs);
    void dmDelivered(const QString &ackHex, int roundTripMs);

    void nodeLoginResult(const QString &keyHex, bool success, bool isAdmin);
    /// status = list of { label, value } maps, ready to show.
    void nodeStatusReceived(const QString &keyHex, const QVariantList &status);
    void nodeCliReply(const QString &keyHex, const QString &text);

private:
    // --- bluetoothmanager.cpp: scanning -----------------------------------
    void ensureLocalDevice();
    void createDiscoveryAgent();
    void refreshAdapterState();
    void resolveNamesViaBlueZ();
    DiscoveredDevice toDiscoveredDevice(const QBluetoothDeviceInfo &info) const;
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onDeviceUpdated(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void setStatus(const QString &status);
    void setScanActive(bool active);

    // --- bluetoothmanager.cpp: connection and UART ------------------------
    bool connectToDevice(const QString &address);
    void startController(const QBluetoothDeviceInfo &info,
                         QLowEnergyController::RemoteAddressType addressType);
    void teardownConnection();
    void onControllerConnected();
    void onControllerDisconnected();
    void onControllerError(QLowEnergyController::Error error);
    void onServiceDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onDetailsTimeout();
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                 const QByteArray &value);
    void onServiceError(QLowEnergyService::ServiceError error);
    void setConnectionState(const QString &state);
    void writeToUart(const QByteArray &data);
    void flushWriteQueue();
    void appendTraffic(const QString &line);

    // --- bluetoothmanager_mesh.cpp: reactions to the companion protocol ---
    void connectCompanionSignals();
    void setupSessionTimers();
    void onSessionStarted(const QString &nodeName);
    void onDeviceInfo(int firmwareVersion, int maxChannels, int maxContacts);
    void onChannelInfo(int idx, const QString &name);
    void onChannelMessage(int idx, const QString &text, quint32 ts, const MessagePath &path);
    void onContactMessage(const QByteArray &prefix, quint8 txtType,
                          const QString &text, quint32 ts);
    void onSendQueued(const QByteArray &ack, bool flood, quint32 timeoutMs);
    void onNoMoreMessages();
    void onNodeOk();
    void onNodeError(const QString &what);
    void onAdvertTimeout();
    void onContactsSyncStarted(int total);
    void onContactsSyncFinished(quint32 lastMod);

    // --- bluetoothmanager_mesh.cpp: helpers -------------------------------
    bool checkSession();
    void requestAllChannels(int count);
    bool storeContactOnNode(const MeshContact &c);
    QString keyHexForPrefix(const QByteArray &prefix) const;
    void setBattery(int milliVolts);
    void setAdvertInfo(bool ok, const QString &text);
    QVariantMap routeInfo(const MessagePath &path) const;
    QString repeaterNames(const QByteArray &hopHash) const;
    static QVariantList parseNodeStatus(const QByteArray &data);

    // --- scanning state ---------------------------------------------------
    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;     // created on first scan
    QBluetoothLocalDevice *m_localDevice = nullptr;        // created after startup
    DeviceListModel m_devices;
    QTimer m_nameResolveTimer;
    bool m_scanActive = false;
    bool m_adapterReady = false;
    QString m_status;
    QString m_selectedAddress;
    QString m_selectedName;
    /// Needed to open a GATT connection - QLowEnergyController wants the full
    /// QBluetoothDeviceInfo, not just an address.
    QHash<QString, QBluetoothDeviceInfo> m_deviceInfos;

    // --- connection state -------------------------------------------------
    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_uartService = nullptr;
    QLowEnergyCharacteristic m_rxCharacteristic;   // phone -> node (write)
    /// Reading the UART characteristics normally takes a second or two. An
    /// unpaired node stalls it until the link drops, see onDetailsTimeout().
    QTimer m_detailsTimeout;
    static constexpr int kDetailsTimeoutMs = 15000;
    bool m_triedRandomAddress = false;
    bool m_connected = false;
    bool m_connecting = false;
    bool m_uartReady = false;
    QString m_connectionState = QStringLiteral("Not connected");
    QString m_connectedDevice;
    QStringList m_trafficLog;
    QList<QByteArray> m_writeQueue;     // paced outgoing frames
    QTimer m_writeTimer;
    static constexpr int kWriteGapMs = 40;

    // --- node session -----------------------------------------------------
    CompanionProtocol m_companion;
    bool m_sessionReady = false;
    QString m_nodeName;
    int m_batteryMv = 0;                // node battery, 0 = unknown
    QStringList m_channelNames;
    int m_maxChannels = 0;              // from DEVICE_INFO; 0 = unknown
    QStringList m_sentFifo;             // per pending RESP_SENT: DM key or ""
    int m_drainCount = 0;               // messages fetched in the current drain
    bool m_reportDrain = false;         // drain started by us (connect/resume/poll)
    QString m_lastSyncInfo;
    QTimer m_messagePollTimer;          // safety poll of the node's queue

    // --- contacts ---------------------------------------------------------
    ContactListModel m_contacts;
    bool m_contactsSyncing = false;
    quint32 m_contactsLastMod = 0;      // 'since' for incremental syncs
    int m_maxContacts = 0;              // node contact table size, 0 = unknown
    QTimer m_contactRefreshDebounce;    // coalesces bursts of advert pushes
    QTimer m_contactAgeTimer;           // keeps "x min ago" texts current
    bool m_advertPending = false;       // waiting for OK/ERR to SEND_SELF_ADVERT
    bool m_advertFlood = true;
    bool m_advertOk = true;
    QString m_lastAdvertInfo;
    QTimer m_advertTimeout;
};

#endif // BLUETOOTHMANAGER_H

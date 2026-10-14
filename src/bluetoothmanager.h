#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QAbstractListModel>
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
#include <QVector>

#include "companionprotocol.h"

struct DiscoveredDevice {
    QString name;              // empty while no real name is known
    QString address;
    int rssi = 0;
    bool lowEnergy = false;
    bool nusService = false;   // Nordic UART Service seen in the advertisement
    bool hasRealName = false;

    bool isMeshCore() const;
    QString displayName() const;
};

/// List model of discovered Bluetooth devices, consumed directly by QML.
class DeviceListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AddressRole,
        RssiRole,
        LowEnergyRole,
        IsMeshCoreRole,
        HasNameRole
    };

    explicit DeviceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_devices.size(); }

    void clearDevices();
    void addOrUpdate(DiscoveredDevice device);
    /// Fills in a name resolved later (via BlueZ). Returns true if it changed.
    bool applyName(const QString &address, const QString &name, int rssi);

    Q_INVOKABLE QString addressAt(int row) const;

signals:
    void countChanged();

private:
    int indexOfAddress(const QString &address) const;

    QVector<DiscoveredDevice> m_devices;
};

/// Bluetooth discovery + BLE (GATT/Nordic UART) connection, exposed to QML
/// as "BluetoothManager".
class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool scanActive READ scanActive NOTIFY scanActiveChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool adapterReady READ adapterReady NOTIFY statusChanged)
    Q_PROPERTY(DeviceListModel *devicesModel READ devicesModel CONSTANT)
    Q_PROPERTY(bool connected READ connected NOTIFY connectionChanged)
    Q_PROPERTY(bool connecting READ connecting NOTIFY connectionChanged)
    Q_PROPERTY(QString connectedDevice READ connectedDevice NOTIFY connectionChanged)
    Q_PROPERTY(QString connectionState READ connectionState NOTIFY connectionChanged)
    Q_PROPERTY(bool uartReady READ uartReady NOTIFY connectionChanged)
    Q_PROPERTY(QString lastData READ lastData NOTIFY dataReceived)
    // Selection lives here rather than in a QML page property, so it cannot
    // get lost between pages of the StackLayout.
    Q_PROPERTY(QString selectedAddress READ selectedAddress NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QString nodeName READ nodeName NOTIFY nodeInfoChanged)
    Q_PROPERTY(bool sessionReady READ sessionReady NOTIFY nodeInfoChanged)
    Q_PROPERTY(QStringList channelNames READ channelNames NOTIFY channelsChanged)
    Q_PROPERTY(QStringList trafficLog READ trafficLog NOTIFY trafficLogChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);
    ~BluetoothManager() override;

    bool scanActive() const { return m_scanActive; }
    QString status() const { return m_status; }
    bool adapterReady() const { return m_adapterReady; }
    DeviceListModel *devicesModel() { return &m_devices; }
    bool connected() const { return m_connected; }
    bool connecting() const { return m_connecting; }
    QString connectedDevice() const { return m_connectedDevice; }
    QString connectionState() const { return m_connectionState; }
    bool uartReady() const { return m_uartReady; }
    QString lastData() const { return m_lastData; }
    QString selectedAddress() const { return m_selectedAddress; }
    QString selectedName() const { return m_selectedName; }
    bool hasSelection() const { return !m_selectedAddress.isEmpty(); }

    /// Called from the device list. Also usable without arguments from the
    /// connect page, which then reuses the current selection.
    QString nodeName() const { return m_nodeName; }
    bool sessionReady() const { return m_sessionReady; }
    QStringList channelNames() const { return m_channelNames; }
    QStringList trafficLog() const { return m_trafficLog; }

    Q_INVOKABLE void selectDevice(const QString &address, const QString &name);
    Q_INVOKABLE bool connectToSelected();
    /// Sends a text message on a channel via the companion protocol.
    Q_INVOKABLE bool sendChannelMessage(int channelIdx, const QString &text);

    Q_INVOKABLE void startScan();
    Q_INVOKABLE void stopScan();
    Q_INVOKABLE bool connectToDevice(const QString &address);
    Q_INVOKABLE void disconnect();
    /// Writes raw bytes to the Nordic UART RX characteristic (phone -> node).
    Q_INVOKABLE bool sendText(const QString &text);

signals:
    void scanActiveChanged();
    void statusChanged();
    void connectionChanged();
    void scanComplete();
    void selectionChanged();
    void dataReceived(const QString &text);
    void nodeInfoChanged();
    void channelsChanged();
    void trafficLogChanged();
    void channelMessage(int channelIdx, const QString &text, const QString &time);
    void sendFailed(const QString &reason);

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onDeviceUpdated(const QBluetoothDeviceInfo &info,
                         QBluetoothDeviceInfo::Fields fields);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);

    void onControllerConnected();
    void onControllerDisconnected();
    void onControllerError(QLowEnergyController::Error error);
    void onServiceDiscoveryFinished();
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                 const QByteArray &value);
    void onServiceError(QLowEnergyService::ServiceError error);

private:
    void setStatus(const QString &status);
    void setScanActive(bool active);
    void setConnectionState(const QString &state);
    void refreshAdapterState();
    DiscoveredDevice toDiscoveredDevice(const QBluetoothDeviceInfo &info) const;
    void resolveNamesViaBlueZ();

    void startController(const QBluetoothDeviceInfo &info,
                         QLowEnergyController::RemoteAddressType addressType);
    void teardownConnection();

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QBluetoothLocalDevice m_localDevice;
    DeviceListModel m_devices;
    QTimer m_nameResolveTimer;

    /// Needed to open a GATT connection - QLowEnergyController wants the full
    /// QBluetoothDeviceInfo, not just an address.
    QHash<QString, QBluetoothDeviceInfo> m_deviceInfos;

    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService *m_uartService = nullptr;
    QLowEnergyCharacteristic m_rxCharacteristic;   // phone -> node (write)
    bool m_triedRandomAddress = false;

    bool m_scanActive = false;
    bool m_adapterReady = false;
    bool m_connected = false;
    bool m_connecting = false;
    bool m_uartReady = false;
    QString m_status;
    QString m_connectionState = QStringLiteral("Not connected");
    QString m_connectedDevice;
    QString m_lastData;
    QString m_selectedAddress;
    QString m_selectedName;

    CompanionProtocol m_companion;
    bool m_sessionReady = false;
    QString m_nodeName;
    QStringList m_channelNames;
    QStringList m_trafficLog;

    void writeToUart(const QByteArray &data);
    void appendTraffic(const QString &line);
};

#endif // BLUETOOTHMANAGER_H

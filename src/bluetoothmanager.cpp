#include "bluetoothmanager.h"

#include <QBluetoothUuid>
#include <QDateTime>
#include <QLowEnergyDescriptor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDebug>

// org.freedesktop.DBus.ObjectManager.GetManagedObjects() return type
typedef QMap<QString, QVariantMap> InterfaceList;
typedef QMap<QDBusObjectPath, InterfaceList> ManagedObjectList;
Q_DECLARE_METATYPE(InterfaceList)
Q_DECLARE_METATYPE(ManagedObjectList)

namespace {

// Nordic UART Service - used by the MeshCore companion radio BLE firmware.
const QString kNusServiceUuid = QStringLiteral("{6e400001-b5a3-f393-e0a9-e50e24dcca9e}");

// NUS is defined from the peripheral's point of view: RX is what the node
// receives (we write to it), TX is what the node sends (we subscribe to it).
const QBluetoothUuid kNusService(QStringLiteral("{6e400001-b5a3-f393-e0a9-e50e24dcca9e}"));
const QBluetoothUuid kNusRxChar (QStringLiteral("{6e400002-b5a3-f393-e0a9-e50e24dcca9e}"));
const QBluetoothUuid kNusTxChar (QStringLiteral("{6e400003-b5a3-f393-e0a9-e50e24dcca9e}"));

/// BlueZ falls back to "AA-BB-CC-DD-EE-FF" for the Alias of a device whose
/// name it does not know. Qt hands that to us as the device name, which is why
/// the list used to show the MAC address twice.
bool looksLikeAddress(const QString &name, const QString &address)
{
    if (name.isEmpty())
        return true;
    QString n = name;
    QString a = address;
    n.remove(QLatin1Char(':')).remove(QLatin1Char('-')).remove(QLatin1Char(' '));
    a.remove(QLatin1Char(':')).remove(QLatin1Char('-')).remove(QLatin1Char(' '));
    a.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    return n.compare(a, Qt::CaseInsensitive) == 0;
}

} // namespace

bool DiscoveredDevice::isMeshCore() const
{
    return nusService || name.contains(QStringLiteral("meshcore"), Qt::CaseInsensitive);
}

QString DiscoveredDevice::displayName() const
{
    return hasRealName ? name : QStringLiteral("(no name)");
}

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DeviceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return QVariant();

    const DiscoveredDevice &device = m_devices.at(index.row());
    switch (role) {
    case NameRole:       return device.displayName();
    case AddressRole:    return device.address;
    case RssiRole:       return device.rssi;
    case LowEnergyRole:  return device.lowEnergy;
    case IsMeshCoreRole: return device.isMeshCore();
    case HasNameRole:    return device.hasRealName;
    default:             return QVariant();
    }
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        { NameRole,       "name" },
        { AddressRole,    "address" },
        { RssiRole,       "rssi" },
        { LowEnergyRole,  "lowEnergy" },
        { IsMeshCoreRole, "isMeshCore" },
        { HasNameRole,    "hasName" }
    };
}

int DeviceListModel::indexOfAddress(const QString &address) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i).address.compare(address, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void DeviceListModel::clearDevices()
{
    if (m_devices.isEmpty())
        return;
    beginResetModel();
    m_devices.clear();
    endResetModel();
    emit countChanged();
}

void DeviceListModel::addOrUpdate(DiscoveredDevice device)
{
    const int row = indexOfAddress(device.address);
    if (row >= 0) {
        const DiscoveredDevice &existing = m_devices.at(row);
        // Never let a later, nameless update wipe a name we already resolved.
        if (!device.hasRealName && existing.hasRealName) {
            device.name = existing.name;
            device.hasRealName = true;
        }
        device.nusService = device.nusService || existing.nusService;
        m_devices[row] = device;
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx);
        return;
    }

    beginInsertRows(QModelIndex(), m_devices.size(), m_devices.size());
    m_devices.append(device);
    endInsertRows();
    emit countChanged();
}

bool DeviceListModel::applyName(const QString &address, const QString &name, int rssi)
{
    const int row = indexOfAddress(address);
    if (row < 0)
        return false;

    DiscoveredDevice &device = m_devices[row];
    bool changed = false;
    if (!name.isEmpty() && device.name != name) {
        device.name = name;
        device.hasRealName = true;
        changed = true;
    }
    if (rssi != 0 && device.rssi != rssi) {
        device.rssi = rssi;
        changed = true;
    }
    if (changed) {
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx);
    }
    return changed;
}

QString DeviceListModel::addressAt(int row) const
{
    if (row < 0 || row >= m_devices.size())
        return QString();
    return m_devices.at(row).address;
}

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
    , m_devices(this)
{
    qDBusRegisterMetaType<InterfaceList>();
    qDBusRegisterMetaType<ManagedObjectList>();

    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    // BLE advertisements need an explicit timeout; 0 would scan forever.
    m_agent->setLowEnergyDiscoveryTimeout(15000);

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BluetoothManager::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated,
            this, &BluetoothManager::onDeviceUpdated);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BluetoothManager::onScanFinished);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::canceled,
            this, &BluetoothManager::onScanFinished);
    connect(m_agent,
            QOverload<QBluetoothDeviceDiscoveryAgent::Error>::of(&QBluetoothDeviceDiscoveryAgent::error),
            this, &BluetoothManager::onScanError);

    connect(&m_localDevice, &QBluetoothLocalDevice::hostModeStateChanged,
            this, [this](QBluetoothLocalDevice::HostMode) { refreshAdapterState(); });

    m_nameResolveTimer.setInterval(2000);
    connect(&m_nameResolveTimer, &QTimer::timeout,
            this, &BluetoothManager::resolveNamesViaBlueZ);

    // --- companion protocol wiring ---------------------------------------
    connect(&m_companion, &CompanionProtocol::writeRequested,
            this, &BluetoothManager::writeToUart);
    connect(&m_companion, &CompanionProtocol::traffic,
            this, &BluetoothManager::appendTraffic);

    connect(&m_companion, &CompanionProtocol::selfInfoReceived,
            this, [this](const QString &name) {
                m_nodeName = name;
                m_sessionReady = true;
                emit nodeInfoChanged();
                setConnectionState(name.isEmpty()
                                       ? QStringLiteral("Session started")
                                       : QStringLiteral("Session started - ") + name);
                emit connectionChanged();
                // Ask for the channel table and drain anything queued.
                for (int i = 0; i < 4; ++i)
                    m_companion.requestChannel(i);
                m_companion.syncNextMessage();
            });

    connect(&m_companion, &CompanionProtocol::channelInfoReceived,
            this, [this](int idx, const QString &name) {
                while (m_channelNames.size() <= idx)
                    m_channelNames.append(QString());
                m_channelNames[idx] = name;
                emit channelsChanged();
            });

    connect(&m_companion, &CompanionProtocol::channelMessageReceived,
            this, [this](int idx, const QString &text, quint32 ts) {
                const QString time = ts > 0
                    ? QDateTime::fromSecsSinceEpoch(ts).toString(QStringLiteral("HH:mm"))
                    : QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
                emit channelMessage(idx, text, time);
            });

    connect(&m_companion, &CompanionProtocol::errorReported,
            this, [this](const QString &what) { emit sendFailed(what); });

    refreshAdapterState();
}

void BluetoothManager::resolveNamesViaBlueZ()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected())
        return;

    QDBusInterface manager(QStringLiteral("org.bluez"), QStringLiteral("/"),
                           QStringLiteral("org.freedesktop.DBus.ObjectManager"), bus);
    if (!manager.isValid())
        return;

    QDBusReply<ManagedObjectList> reply = manager.call(QStringLiteral("GetManagedObjects"));
    if (!reply.isValid()) {
        qWarning() << "BluetoothManager: GetManagedObjects failed:"
                   << reply.error().name() << reply.error().message();
        return;
    }

    int resolved = 0;
    const ManagedObjectList objects = reply.value();
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const InterfaceList &interfaces = it.value();
        if (!interfaces.contains(QStringLiteral("org.bluez.Device1")))
            continue;

        const QVariantMap props = interfaces.value(QStringLiteral("org.bluez.Device1"));
        const QString address = props.value(QStringLiteral("Address")).toString();
        if (address.isEmpty())
            continue;

        // Prefer the advertised Name; Alias is only useful when the user
        // renamed the device, and falls back to the MAC otherwise.
        QString name = props.value(QStringLiteral("Name")).toString();
        if (name.isEmpty())
            name = props.value(QStringLiteral("Alias")).toString();
        if (looksLikeAddress(name, address))
            continue;

        const int rssi = props.value(QStringLiteral("RSSI")).toInt();
        if (m_devices.applyName(address, name, rssi)) {
            ++resolved;
            qDebug() << "BluetoothManager: resolved name" << address << "->" << name;
        }
    }

    if (resolved > 0 && !m_scanActive)
        setStatus(QStringLiteral("Scan finished - %1 device(s)").arg(m_devices.count()));
}

void BluetoothManager::refreshAdapterState()
{
    if (!m_localDevice.isValid()) {
        m_adapterReady = false;
        setStatus(QStringLiteral("No Bluetooth adapter found"));
        return;
    }

    const QBluetoothLocalDevice::HostMode mode = m_localDevice.hostMode();
    m_adapterReady = (mode != QBluetoothLocalDevice::HostPoweredOff);
    if (!m_adapterReady)
        setStatus(QStringLiteral("Bluetooth is switched off"));
    else if (m_status.isEmpty() || m_status.startsWith(QStringLiteral("Bluetooth is switched off")))
        setStatus(QStringLiteral("Ready"));
    emit statusChanged();
}

DiscoveredDevice BluetoothManager::toDiscoveredDevice(const QBluetoothDeviceInfo &info) const
{
    DiscoveredDevice device;
    device.address = info.address().isNull() ? info.deviceUuid().toString()
                                             : info.address().toString();
    device.rssi = info.rssi();
    device.lowEnergy = info.coreConfigurations()
                       & QBluetoothDeviceInfo::LowEnergyCoreConfiguration;

    const QString reported = info.name();
    device.hasRealName = !looksLikeAddress(reported, device.address);
    device.name = device.hasRealName ? reported : QString();

    const QList<QBluetoothUuid> uuids = info.serviceUuids();
    for (const QBluetoothUuid &uuid : uuids) {
        if (uuid.toString().compare(kNusServiceUuid, Qt::CaseInsensitive) == 0) {
            device.nusService = true;
            break;
        }
    }
    return device;
}

void BluetoothManager::startScan()
{
    if (m_scanActive) {
        qDebug() << "BluetoothManager: scan already running";
        return;
    }

    if (m_localDevice.isValid()
        && m_localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        qDebug() << "BluetoothManager: adapter powered off, powering on";
        m_localDevice.powerOn();
    }

    if (!m_localDevice.isValid()) {
        setStatus(QStringLiteral("No Bluetooth adapter found"));
        emit scanComplete();
        return;
    }

    m_devices.clearDevices();
    m_deviceInfos.clear();

    QBluetoothDeviceDiscoveryAgent::DiscoveryMethods methods =
        m_agent->supportedDiscoveryMethods();
    if (methods == QBluetoothDeviceDiscoveryAgent::NoMethod) {
        setStatus(QStringLiteral("Discovery not supported on this device"));
        emit scanComplete();
        return;
    }

    qDebug() << "BluetoothManager: starting discovery, methods =" << int(methods);
    setScanActive(true);
    setStatus(QStringLiteral("Scanning..."));
    m_agent->start(methods);
    m_nameResolveTimer.start();
}

void BluetoothManager::stopScan()
{
    if (m_agent->isActive())
        m_agent->stop();
    m_nameResolveTimer.stop();
    resolveNamesViaBlueZ();
    setScanActive(false);
    setStatus(QStringLiteral("Scan stopped"));
    emit scanComplete();
}

void BluetoothManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    const DiscoveredDevice device = toDiscoveredDevice(info);
    qDebug() << "BluetoothManager: discovered"
             << (device.hasRealName ? device.name : QStringLiteral("<no name>"))
             << device.address << "LE:" << device.lowEnergy << "RSSI:" << device.rssi;
    m_deviceInfos.insert(device.address.toUpper(), info);
    m_devices.addOrUpdate(device);

    // A freshly seen device often gets its name from BlueZ a moment later.
    if (!device.hasRealName)
        QTimer::singleShot(600, this, &BluetoothManager::resolveNamesViaBlueZ);
}

void BluetoothManager::onDeviceUpdated(const QBluetoothDeviceInfo &info,
                                       QBluetoothDeviceInfo::Fields fields)
{
    Q_UNUSED(fields)
    const DiscoveredDevice device = toDiscoveredDevice(info);
    m_deviceInfos.insert(device.address.toUpper(), info);
    m_devices.addOrUpdate(device);
}

void BluetoothManager::onScanFinished()
{
    m_nameResolveTimer.stop();
    resolveNamesViaBlueZ();

    qDebug() << "BluetoothManager: scan finished, found" << m_devices.count() << "devices";
    setScanActive(false);
    setStatus(m_devices.count() == 0
                  ? QStringLiteral("Scan finished - no devices found")
                  : QStringLiteral("Scan finished - %1 device(s)").arg(m_devices.count()));
    emit scanComplete();

    // BlueZ can still be resolving names right after discovery stops.
    QTimer::singleShot(1500, this, &BluetoothManager::resolveNamesViaBlueZ);
}

void BluetoothManager::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    QString message;
    switch (error) {
    case QBluetoothDeviceDiscoveryAgent::PoweredOffError:
        message = QStringLiteral("Bluetooth is switched off");
        break;
    case QBluetoothDeviceDiscoveryAgent::InputOutputError:
        message = QStringLiteral("Bluetooth I/O error");
        break;
    case QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError:
        message = QStringLiteral("Invalid Bluetooth adapter");
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError:
        message = QStringLiteral("Discovery unsupported on this platform");
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod:
        message = QStringLiteral("Requested discovery method unsupported");
        break;
    default:
        message = m_agent->errorString();
        break;
    }

    qWarning() << "BluetoothManager: discovery error:" << error << message;
    m_nameResolveTimer.stop();
    setScanActive(false);
    setStatus(QStringLiteral("Error: ") + message);
    emit scanComplete();
}

// ---------------------------------------------------------------------------
// BLE connection (GATT / Nordic UART Service)
// ---------------------------------------------------------------------------

BluetoothManager::~BluetoothManager()
{
    teardownConnection();
}

void BluetoothManager::teardownConnection()
{
    m_rxCharacteristic = QLowEnergyCharacteristic();

    if (m_uartService) {
        m_uartService->deleteLater();
        m_uartService = nullptr;
    }
    if (m_controller) {
        // Drop our slots first, otherwise the disconnectFromDevice() below
        // calls back into onControllerDisconnected() and the state is logged
        // and emitted twice.
        m_controller->disconnect(this);
        m_controller->disconnectFromDevice();
        m_controller->deleteLater();
        m_controller = nullptr;
    }

    m_uartReady = false;
    m_connected = false;
    m_connecting = false;
}

void BluetoothManager::selectDevice(const QString &address, const QString &name)
{
    qDebug() << "BluetoothManager: selectDevice" << address << name;
    m_selectedAddress = address;
    m_selectedName = name;
    emit selectionChanged();
}

bool BluetoothManager::connectToSelected()
{
    return connectToDevice(m_selectedAddress);
}

bool BluetoothManager::connectToDevice(const QString &address)
{
    qDebug() << "BluetoothManager: connectToDevice" << address;
    if (address.isEmpty()) {
        setConnectionState(QStringLiteral("No device selected"));
        emit connectionChanged();
        return false;
    }

    const QBluetoothDeviceInfo info = m_deviceInfos.value(address.toUpper());
    if (!info.isValid()) {
        qWarning() << "BluetoothManager: no QBluetoothDeviceInfo cached for" << address;
        setConnectionState(QStringLiteral("Unknown device - please scan again"));
        emit connectionChanged();
        return false;
    }

    // Scanning and connecting at the same time makes BlueZ unhappy.
    if (m_agent->isActive())
        stopScan();

    const bool hadConnection = (m_controller != nullptr);
    teardownConnection();
    m_triedRandomAddress = false;
    m_connectedDevice = address;

    m_connecting = true;
    setConnectionState(QStringLiteral("Connecting..."));
    emit connectionChanged();

    if (hadConnection) {
        // BlueZ needs a moment to tear the old link down, otherwise the new
        // attempt fails with "Cannot connect due to pending active LE
        // connections" and only recovers after a retry.
        QTimer::singleShot(400, this, [this, info]() {
            startController(info, QLowEnergyController::PublicAddress);
        });
    } else {
        startController(info, QLowEnergyController::PublicAddress);
    }
    return true;
}

void BluetoothManager::startController(const QBluetoothDeviceInfo &info,
                                       QLowEnergyController::RemoteAddressType addressType)
{
    m_controller = QLowEnergyController::createCentral(info, this);
    m_controller->setRemoteAddressType(addressType);

    connect(m_controller, &QLowEnergyController::connected,
            this, &BluetoothManager::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected,
            this, &BluetoothManager::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &BluetoothManager::onServiceDiscoveryFinished);
    connect(m_controller,
            QOverload<QLowEnergyController::Error>::of(&QLowEnergyController::error),
            this, &BluetoothManager::onControllerError);

    m_connecting = true;
    m_connected = false;
    setConnectionState(QStringLiteral("Connecting..."));
    emit connectionChanged();

    qDebug() << "BluetoothManager: connecting to" << m_connectedDevice
             << "address type" << int(addressType);
    m_controller->connectToDevice();
}

void BluetoothManager::onControllerConnected()
{
    qDebug() << "BluetoothManager: connected, discovering services";
    setConnectionState(QStringLiteral("Connected - discovering services..."));
    m_connected = true;
    m_connecting = false;
    emit connectionChanged();
    m_controller->discoverServices();
}

void BluetoothManager::onControllerDisconnected()
{
    qDebug() << "BluetoothManager: disconnected";
    m_rxCharacteristic = QLowEnergyCharacteristic();
    if (m_uartService) {
        m_uartService->deleteLater();
        m_uartService = nullptr;
    }
    m_connected = false;
    m_connecting = false;
    m_uartReady = false;
    m_sessionReady = false;
    m_nodeName.clear();
    emit nodeInfoChanged();
    setConnectionState(QStringLiteral("Not connected"));
    emit connectionChanged();
}

void BluetoothManager::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void BluetoothManager::setScanActive(bool active)
{
    if (m_scanActive == active)
        return;
    m_scanActive = active;
    emit scanActiveChanged();
}

void BluetoothManager::onServiceDiscoveryFinished()
{
    const QList<QBluetoothUuid> services = m_controller->services();
    qDebug() << "BluetoothManager: services found:" << services;

    if (!services.contains(kNusService)) {
        setConnectionState(QStringLiteral("Connected, but no Nordic UART service"));
        emit connectionChanged();
        return;
    }

    m_uartService = m_controller->createServiceObject(kNusService, this);
    if (!m_uartService) {
        setConnectionState(QStringLiteral("Could not open UART service"));
        emit connectionChanged();
        return;
    }

    connect(m_uartService, &QLowEnergyService::stateChanged,
            this, &BluetoothManager::onServiceStateChanged);
    connect(m_uartService, &QLowEnergyService::characteristicChanged,
            this, &BluetoothManager::onCharacteristicChanged);
    connect(m_uartService,
            QOverload<QLowEnergyService::ServiceError>::of(&QLowEnergyService::error),
            this, &BluetoothManager::onServiceError);

    setConnectionState(QStringLiteral("Reading UART characteristics..."));
    emit connectionChanged();
    m_uartService->discoverDetails();
}

void BluetoothManager::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    if (state != QLowEnergyService::ServiceDiscovered)
        return;

    m_rxCharacteristic = m_uartService->characteristic(kNusRxChar);
    const QLowEnergyCharacteristic tx = m_uartService->characteristic(kNusTxChar);

    if (!tx.isValid() || !m_rxCharacteristic.isValid()) {
        setConnectionState(QStringLiteral("UART characteristics missing"));
        emit connectionChanged();
        return;
    }

    // Subscribe to notifications by writing 0x0100 to the Client Characteristic
    // Configuration descriptor - without this the node can talk but we hear it.
    const QLowEnergyDescriptor ccc =
        tx.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration);
    if (ccc.isValid()) {
        m_uartService->writeDescriptor(ccc, QByteArray::fromHex("0100"));
    } else {
        qWarning() << "BluetoothManager: no CCC descriptor on TX characteristic";
    }

    m_uartReady = true;
    setConnectionState(QStringLiteral("UART ready - starting session..."));
    emit connectionChanged();
    qDebug() << "BluetoothManager: Nordic UART ready on" << m_connectedDevice;

    // Until the node has seen an APP_START it treats us as a passive listener
    // and only pushes its RX log; contacts, channels and messages stay silent.
    // Give the notification subscription a moment to take effect first.
    QTimer::singleShot(400, this, [this]() {
        if (m_uartReady)
            m_companion.sendAppStart(QStringLiteral("ubMeshcore"));
    });
}

void BluetoothManager::writeToUart(const QByteArray &data)
{
    if (!m_uartService || !m_rxCharacteristic.isValid()) {
        qWarning() << "BluetoothManager: UART not ready, dropping"
                   << data.size() << "bytes";
        emit sendFailed(QStringLiteral("Not connected"));
        return;
    }

    const QLowEnergyService::WriteMode mode =
        (m_rxCharacteristic.properties() & QLowEnergyCharacteristic::WriteNoResponse)
            ? QLowEnergyService::WriteWithoutResponse
            : QLowEnergyService::WriteWithResponse;

    m_uartService->writeCharacteristic(m_rxCharacteristic, data, mode);
}

void BluetoothManager::appendTraffic(const QString &line)
{
    m_trafficLog.append(line);
    while (m_trafficLog.size() > 60)
        m_trafficLog.removeFirst();
    emit trafficLogChanged();
}

bool BluetoothManager::sendChannelMessage(int channelIdx, const QString &text)
{
    if (text.isEmpty())
        return false;
    if (!m_uartReady) {
        emit sendFailed(QStringLiteral("Not connected to a node"));
        return false;
    }
    if (!m_sessionReady) {
        emit sendFailed(QStringLiteral("Node session not started yet"));
        return false;
    }

    qDebug() << "BluetoothManager: sending on channel" << channelIdx << ":" << text;
    m_companion.sendChannelText(channelIdx, text);
    return true;
}

void BluetoothManager::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                               const QByteArray &value)
{
    if (characteristic.uuid() != kNusTxChar)
        return;

    m_lastData = QString::fromLatin1(value.toHex(' '));
    emit dataReceived(m_lastData);

    // Hand the frame to the companion-protocol decoder.
    m_companion.handleFrame(value);
}

void BluetoothManager::onServiceError(QLowEnergyService::ServiceError error)
{
    qWarning() << "BluetoothManager: service error:" << error;
    if (error == QLowEnergyService::DescriptorWriteError)
        setConnectionState(QStringLiteral("Could not enable notifications"));
    emit connectionChanged();
}

void BluetoothManager::onControllerError(QLowEnergyController::Error error)
{
    qWarning() << "BluetoothManager: controller error:" << error
               << m_controller->errorString();

    // nRF52-based nodes (MeshCore) usually advertise with a random static
    // address. If the public-address attempt fails, retry once as random.
    if (!m_triedRandomAddress
        && (error == QLowEnergyController::ConnectionError
            || error == QLowEnergyController::UnknownRemoteDeviceError)) {
        const QBluetoothDeviceInfo info = m_deviceInfos.value(m_connectedDevice.toUpper());
        if (info.isValid()) {
            qDebug() << "BluetoothManager: retrying with random address type";
            m_triedRandomAddress = true;
            teardownConnection();
            startController(info, QLowEnergyController::RandomAddress);
            return;
        }
    }

    m_connected = false;
    m_connecting = false;
    m_uartReady = false;
    setConnectionState(QStringLiteral("Connection failed: ") + m_controller->errorString());
    emit connectionChanged();
}

void BluetoothManager::disconnect()
{
    qDebug() << "BluetoothManager: disconnect requested";
    teardownConnection();
    m_connectedDevice.clear();
    setConnectionState(QStringLiteral("Not connected"));
    emit connectionChanged();
}

bool BluetoothManager::sendText(const QString &text)
{
    if (!m_uartService || !m_rxCharacteristic.isValid()) {
        qWarning() << "BluetoothManager: sendText without a ready UART";
        return false;
    }

    const QLowEnergyService::WriteMode mode =
        (m_rxCharacteristic.properties() & QLowEnergyCharacteristic::WriteNoResponse)
            ? QLowEnergyService::WriteWithoutResponse
            : QLowEnergyService::WriteWithResponse;

    m_uartService->writeCharacteristic(m_rxCharacteristic, text.toUtf8(), mode);
    return true;
}

void BluetoothManager::setConnectionState(const QString &state)
{
    if (m_connectionState == state)
        return;
    m_connectionState = state;
}

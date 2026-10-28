// BluetoothManager, part 1: finding the node and talking to it over BLE.
//
// Scan for devices (Qt discovery + BlueZ over D-Bus for late names), open a
// GATT connection, set up the Nordic UART Service and move bytes in and out.
// What those bytes mean is handled in bluetoothmanager_mesh.cpp.

#include "bluetoothmanager.h"

#include <QBluetoothUuid>
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


} // namespace

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
    , m_devices(this)
    , m_contacts(this)
{
    // While a scan runs, BlueZ is asked regularly for names Qt has not
    // reported (see resolveNamesViaBlueZ).
    m_nameResolveTimer.setInterval(2000);
    connect(&m_nameResolveTimer, &QTimer::timeout,
            this, &BluetoothManager::resolveNamesViaBlueZ);

    // Outgoing frames are paced, see writeToUart().
    m_writeTimer.setSingleShot(true);
    m_writeTimer.setInterval(kWriteGapMs);
    connect(&m_writeTimer, &QTimer::timeout, this, &BluetoothManager::flushWriteQueue);

    m_detailsTimeout.setSingleShot(true);
    m_detailsTimeout.setInterval(kDetailsTimeoutMs);
    connect(&m_detailsTimeout, &QTimer::timeout, this, &BluetoothManager::onDetailsTimeout);

    connectCompanionSignals();
    setupSessionTimers();

    // No D-Bus or Bluetooth access in the constructor: it runs before the
    // event loop, where AppArmor confinement has made it fail.
    // The adapter must be known BEFORE the first scan, though: the "Start
    // Scan" button is only enabled once adapterReady is true. Deferred to the
    // event loop so QBluetoothLocalDevice's internal timers are created on
    // the GUI thread after QGuiApplication is fully up.
    setStatus(tr("Checking Bluetooth..."));
    QTimer::singleShot(0, this, &BluetoothManager::ensureLocalDevice);
}

BluetoothManager::~BluetoothManager()
{
    teardownConnection();
    if (m_localDevice) {
        m_localDevice->deleteLater();
        m_localDevice = nullptr;
    }
    if (m_agent) {
        m_agent->deleteLater();
        m_agent = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------

void BluetoothManager::ensureLocalDevice()
{
    if (!m_localDevice) {
        qDebug() << "BluetoothManager: initializing local device";
        m_localDevice = new QBluetoothLocalDevice(this);
        connect(m_localDevice, &QBluetoothLocalDevice::hostModeStateChanged,
                this, [this](QBluetoothLocalDevice::HostMode) { refreshAdapterState(); });
    }
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
        setStatus(tr("Scan finished - %1 device(s)").arg(m_devices.count()));
}

void BluetoothManager::refreshAdapterState()
{
    if (!m_localDevice || !m_localDevice->isValid()) {
        m_adapterReady = false;
        setStatus(tr("No Bluetooth adapter found"));
        return;
    }

    const QBluetoothLocalDevice::HostMode mode = m_localDevice->hostMode();
    m_adapterReady = (mode != QBluetoothLocalDevice::HostPoweredOff);
    if (!m_adapterReady)
        setStatus(tr("Bluetooth is switched off"));
    else if (m_status.isEmpty() || m_status.startsWith(tr("Bluetooth is switched off")))
        setStatus(tr("Ready"));
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

    ensureLocalDevice();
    if (!m_agent)
        createDiscoveryAgent();

    if (m_localDevice && m_localDevice->isValid()
        && m_localDevice->hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        qDebug() << "BluetoothManager: adapter powered off, powering on";
        m_localDevice->powerOn();
    }

    if (!m_localDevice || !m_localDevice->isValid()) {
        setStatus(tr("No Bluetooth adapter found"));
        return;
    }

    m_devices.clearDevices();
    m_deviceInfos.clear();

    QBluetoothDeviceDiscoveryAgent::DiscoveryMethods methods =
        m_agent->supportedDiscoveryMethods();
    if (methods == QBluetoothDeviceDiscoveryAgent::NoMethod) {
        setStatus(tr("Discovery not supported on this device"));
        return;
    }

    qDebug() << "BluetoothManager: starting discovery, methods =" << int(methods);
    setScanActive(true);
    setStatus(tr("Scanning..."));
    m_agent->start(methods);
    m_nameResolveTimer.start();
}

void BluetoothManager::createDiscoveryAgent()
{
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(15000);

    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BluetoothManager::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceUpdated,
            this, &BluetoothManager::onDeviceUpdated);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BluetoothManager::onScanFinished);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::canceled,
            this, &BluetoothManager::onScanFinished);
    using AgentError = QBluetoothDeviceDiscoveryAgent::Error;
    connect(m_agent, QOverload<AgentError>::of(&QBluetoothDeviceDiscoveryAgent::error),
            this, &BluetoothManager::onScanError);
}

void BluetoothManager::stopScan()
{
    if (m_agent && m_agent->isActive())
        m_agent->stop();
    m_nameResolveTimer.stop();
    resolveNamesViaBlueZ();
    setScanActive(false);
    setStatus(tr("Scan stopped"));
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

void BluetoothManager::onDeviceUpdated(const QBluetoothDeviceInfo &info)
{
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
                  ? tr("Scan finished - no devices found")
                  : tr("Scan finished - %1 device(s)").arg(m_devices.count()));

    // BlueZ can still be resolving names right after discovery stops.
    QTimer::singleShot(1500, this, &BluetoothManager::resolveNamesViaBlueZ);
}

void BluetoothManager::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    QString message;
    switch (error) {
    case QBluetoothDeviceDiscoveryAgent::PoweredOffError:
        message = tr("Bluetooth is switched off");
        break;
    case QBluetoothDeviceDiscoveryAgent::InputOutputError:
        message = tr("Bluetooth I/O error");
        break;
    case QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError:
        message = tr("Invalid Bluetooth adapter");
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError:
        message = tr("Discovery unsupported on this platform");
        break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod:
        message = tr("Requested discovery method unsupported");
        break;
    default:
        message = m_agent->errorString();
        break;
    }

    qWarning() << "BluetoothManager: discovery error:" << error << message;
    m_nameResolveTimer.stop();
    setScanActive(false);
    setStatus(tr("Error: ") + message);
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

void BluetoothManager::selectDevice(const QString &address, const QString &name)
{
    qDebug() << "BluetoothManager: selectDevice" << address << name;
    m_selectedAddress = address;
    m_selectedName = name;
    emit selectionChanged();
}

// ---------------------------------------------------------------------------
// BLE connection (GATT / Nordic UART Service)
// ---------------------------------------------------------------------------

bool BluetoothManager::connectToSelected()
{
    return connectToDevice(m_selectedAddress);
}

bool BluetoothManager::connectToDevice(const QString &address)
{
    qDebug() << "BluetoothManager: connectToDevice" << address;
    if (address.isEmpty()) {
        setConnectionState(tr("No device selected"));
        emit connectionChanged();
        return false;
    }

    const QBluetoothDeviceInfo info = m_deviceInfos.value(address.toUpper());
    if (!info.isValid()) {
        qWarning() << "BluetoothManager: no QBluetoothDeviceInfo cached for" << address;
        setConnectionState(tr("Unknown device - please scan again"));
        emit connectionChanged();
        return false;
    }

    // Scanning and connecting at the same time makes BlueZ unhappy.
    if (m_agent && m_agent->isActive())
        stopScan();

    const bool hadConnection = (m_controller != nullptr);
    teardownConnection();
    m_triedRandomAddress = false;
    m_connectedDevice = address;

    m_connecting = true;
    setConnectionState(tr("Connecting..."));
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
    setConnectionState(tr("Connecting..."));
    emit connectionChanged();

    qDebug() << "BluetoothManager: connecting to" << m_connectedDevice
             << "address type" << int(addressType);
    m_controller->connectToDevice();
}

void BluetoothManager::teardownConnection()
{
    m_detailsTimeout.stop();
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
    setBattery(0);
}

void BluetoothManager::disconnect()
{
    qDebug() << "BluetoothManager: disconnect requested";
    teardownConnection();
    m_connectedDevice.clear();
    setConnectionState(tr("Not connected"));
    emit connectionChanged();
}

void BluetoothManager::onControllerConnected()
{
    qDebug() << "BluetoothManager: connected, discovering services";
    setConnectionState(tr("Connected - discovering services..."));
    m_connected = true;
    m_connecting = false;
    emit connectionChanged();
    m_controller->discoverServices();
}

void BluetoothManager::onControllerDisconnected()
{
    qDebug() << "BluetoothManager: disconnected";
    m_detailsTimeout.stop();
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
    setBattery(0);
    m_contactRefreshDebounce.stop();
    m_writeQueue.clear();
    m_writeTimer.stop();
    m_sentFifo.clear();
    if (m_advertPending) {
        m_advertPending = false;
        m_advertTimeout.stop();
        setAdvertInfo(false, tr("Connection lost before the node confirmed the advert"));
    }
    if (m_contactsSyncing) {
        m_contactsSyncing = false;
        emit contactsSyncingChanged();
    }
    emit nodeInfoChanged();
    setConnectionState(tr("Not connected"));
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
    setConnectionState(tr("Connection failed: ") + m_controller->errorString());
    emit connectionChanged();
}

void BluetoothManager::onServiceDiscoveryFinished()
{
    const QList<QBluetoothUuid> services = m_controller->services();
    qDebug() << "BluetoothManager: services found:" << services;

    if (!services.contains(kNusService)) {
        setConnectionState(tr("Connected, but no Nordic UART service"));
        emit connectionChanged();
        return;
    }

    m_uartService = m_controller->createServiceObject(kNusService, this);
    if (!m_uartService) {
        setConnectionState(tr("Could not open UART service"));
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

    setConnectionState(tr("Reading UART characteristics..."));
    emit connectionChanged();
    m_detailsTimeout.start();
    m_uartService->discoverDetails();
}

void BluetoothManager::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    if (state != QLowEnergyService::ServiceDiscovered)
        return;
    m_detailsTimeout.stop();

    m_rxCharacteristic = m_uartService->characteristic(kNusRxChar);
    const QLowEnergyCharacteristic tx = m_uartService->characteristic(kNusTxChar);

    if (!tx.isValid() || !m_rxCharacteristic.isValid()) {
        setConnectionState(tr("UART characteristics missing"));
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
    setConnectionState(tr("UART ready - starting session..."));
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

// MeshCore companion firmware only lets a bonded phone read the UART
// characteristics. Without a bond, Qt waits for an encryption change that
// never comes ("Encryption change event blocking further GATT requests")
// until the link drops after about 30 s. The system Bluetooth settings do not
// ask for the node's PIN, so the user has to pair once with bluetoothctl.
void BluetoothManager::onDetailsTimeout()
{
    qWarning() << "BluetoothManager: no answer while reading UART characteristics"
               << "- node probably not paired";
    teardownConnection();
    setConnectionState(tr("The node did not answer. It is probably not paired with this "
                          "phone yet - see \"Pairing\" in the README."));
    emit connectionChanged();
}

void BluetoothManager::onServiceError(QLowEnergyService::ServiceError error)
{
    qWarning() << "BluetoothManager: service error:" << error;
    if (error == QLowEnergyService::DescriptorWriteError)
        setConnectionState(tr("Could not enable notifications"));
    emit connectionChanged();
}

void BluetoothManager::setConnectionState(const QString &state)
{
    if (m_connectionState == state)
        return;
    m_connectionState = state;
}

// ---------------------------------------------------------------------------
// Moving bytes
// ---------------------------------------------------------------------------

void BluetoothManager::writeToUart(const QByteArray &data)
{
    if (!m_uartService || !m_rxCharacteristic.isValid()) {
        qWarning() << "BluetoothManager: UART not ready, dropping"
                   << data.size() << "bytes";
        emit sendFailed(tr("Not connected"));
        return;
    }

    // Pace the writes: the companion firmware handles one command at a time,
    // and a burst (e.g. reading 40 channel slots) written back to back can
    // overrun it. Frames go out in order, at least kWriteGapMs apart.
    m_writeQueue.append(data);
    if (!m_writeTimer.isActive())
        flushWriteQueue();
}

void BluetoothManager::flushWriteQueue()
{
    if (m_writeQueue.isEmpty())
        return;
    if (!m_uartService || !m_rxCharacteristic.isValid()) {
        m_writeQueue.clear();
        return;
    }
    const QByteArray data = m_writeQueue.takeFirst();
    const QLowEnergyService::WriteMode mode =
        (m_rxCharacteristic.properties() & QLowEnergyCharacteristic::WriteNoResponse)
            ? QLowEnergyService::WriteWithoutResponse
            : QLowEnergyService::WriteWithResponse;
    m_uartService->writeCharacteristic(m_rxCharacteristic, data, mode);
    m_writeTimer.start();   // single shot; fires flushWriteQueue for the next
}

void BluetoothManager::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                               const QByteArray &value)
{
    if (characteristic.uuid() != kNusTxChar)
        return;
    // Hand the frame to the companion-protocol decoder.
    m_companion.handleFrame(value);
}

void BluetoothManager::appendTraffic(const QString &line)
{
    m_trafficLog.append(line);
    while (m_trafficLog.size() > 60)
        m_trafficLog.removeFirst();
    emit trafficLogChanged();
}

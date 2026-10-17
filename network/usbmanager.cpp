#include "usbmanager.h"
#include <QDebug>
#include <QSerialPortInfo>
#include <QSerialPort>

UsbManager::UsbManager(QObject *parent)
    : QObject(parent)
    , m_connected(false)
    , m_autoReconnect(true)
    , m_baudRate(115200)
{
    qDebug() << "UsbManager initialized";
}

UsbManager::~UsbManager()
{
    disconnect();
    qDebug() << "UsbManager destroyed";
}

QList<QVariantMap> UsbManager::scanForDevices()
{
    QList<QVariantMap> devices;
    
    // In a real implementation, this would scan for serial/USB devices
    // For now, return sample devices
    
    // Detect available serial ports
    foreach (const QSerialPortInfo &port, QSerialPortInfo::availablePorts()) {
        QVariantMap device;
        device["id"] = port.portName();
        device["name"] = port.description();
        device["manufacturer"] = port.manufacturer();
        device["vendorId"] = port.vendorIdentifier();
        device["productId"] = port.productIdentifier();
        device["type"] = "serial";
        devices.append(device);
    }
    
    return devices;
}

bool UsbManager::connectToDevice(const QString &deviceId, bool autoReconnect)
{
    m_deviceId = deviceId;
    m_autoReconnect = autoReconnect;
    
    qDebug() << "Connecting to USB device:" << deviceId;
    
    // Stub: In a real implementation, open serial port and establish connection
    m_connected = true;
    emit deviceConnected(deviceId);
    emit connectionStatusChanged("Connected via USB to " + deviceId);
    
    return true;
}

void UsbManager::disconnect()
{
    if (m_connected) {
        qDebug() << "Disconnecting from USB device:" << m_deviceId;
        m_connected = false;
        emit deviceDisconnected();
        emit connectionStatusChanged("Disconnected");
    }
    m_deviceId.clear();
}

bool UsbManager::isConnected() const
{
    return m_connected;
}

QString UsbManager::getConnectionStatus() const
{
    if (m_connected) {
        return "Connected to " + m_deviceId + " via USB";
    }
    return "Disconnected";
}

bool UsbManager::sendData(const QByteArray &data)
{
    if (!m_connected) {
        emit errorOccurred("Not connected to USB device");
        return false;
    }
    
    qDebug() << "Sending" << data.size() << "bytes via USB";
    // Stub: In a real implementation, write to serial port
    return true;
}

QVariantMap UsbManager::getDeviceInfo()
{
    QVariantMap info;
    info["id"] = m_deviceId;
    info["type"] = "USB";
    info["connected"] = m_connected;
    info["baudRate"] = m_baudRate;
    info["autoReconnect"] = m_autoReconnect;
    return info;
}

QVariantMap UsbManager::fetchDeviceInfo(int timeout)
{
    QVariantMap deviceInfo;
    
    if (!m_connected) {
        emit errorOccurred("Cannot fetch device info: Not connected");
        return deviceInfo;
    }
    
    // Stub: In a real implementation, send a command to fetch device info
    qDebug() << "Fetching device info (timeout:" << timeout << "ms)";
    
    // Sample device info
    deviceInfo["firmwareVersion"] = "1.50.0";
    deviceInfo["deviceType"] = "ESP32";
    deviceInfo["nodeId"] = "0x12345678";
    deviceInfo["protocolVersion"] = "1.0";
    
    return deviceInfo;
}

void UsbManager::setAutoReconnect(bool enable)
{
    m_autoReconnect = enable;
    qDebug() << "Auto-reconnect set to:" << enable;
}

QList<int> UsbManager::getSupportedBaudRates()
{
    return {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
}

bool UsbManager::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
    qDebug() << "Baud rate set to:" << baudRate;
    return true;
}

QList<QVariantMap> UsbManager::scanForDevicesLinux()
{
    // Platform-specific implementation for Linux
    return scanForDevices();
}

QList<QVariantMap> UsbManager::scanForDevicesWindows()
{
    // Platform-specific implementation for Windows
    return scanForDevices();
}

QList<QVariantMap> UsbManager::scanForDevicesMacOS()
{
    // Platform-specific implementation for macOS
    return scanForDevices();
}

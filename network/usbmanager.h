#ifndef USBMANAGER_H
#define USBMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * @brief Manages USB connections for MeshCore
 *
 * Features:
 * - USB connection establishment and management
 * - Auto-reconnect functionality
 * - Platform-specific USB handling (Windows, macOS, Linux)
 * - Device information retrieval
 * - Data transfer over USB
 */
class UsbManager : public QObject
{
    Q_OBJECT

public:
    explicit UsbManager(QObject *parent = nullptr);
    ~UsbManager() override;

    /**
     * @brief Scan for available USB devices
     * @return List of available USB devices with their properties
     */
    QList<QVariantMap> scanForDevices();

    /**
     * @brief Connect to a USB device
     * @param deviceId ID of the device to connect to
     * @param autoReconnect Whether to auto-reconnect if connection is lost
     * @return true if connection was successful
     */
    bool connectToDevice(const QString &deviceId, bool autoReconnect = true);

    /**
     * @brief Disconnect from current USB device
     */
    void disconnect();

    /**
     * @brief Check if currently connected via USB
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current connection status
     * @return Connection status string
     */
    QString getConnectionStatus() const;

    /**
     * @brief Send data over USB connection
     * @param data Data to send
     * @return true if data was sent successfully
     */
    bool sendData(const QByteArray &data);

    /**
     * @brief Get information about the connected device
     * @return Device information map
     */
    QVariantMap getDeviceInfo();

    /**
     * @brief Fetch device info (ESP32 specific)
     * @param timeout Timeout in milliseconds
     * @return Device info or empty map on failure
     */
    QVariantMap fetchDeviceInfo(int timeout = 5000);

    /**
     * @brief Set auto-reconnect preference
     * @param enable Whether to enable auto-reconnect
     */
    void setAutoReconnect(bool enable);

    /**
     * @brief Get the list of supported baud rates
     * @return List of supported baud rates
     */
    QList<int> getSupportedBaudRates();

    /**
     * @brief Set custom baud rate
     * @param baudRate Baud rate to use
     * @return true if baud rate was set successfully
     */
    bool setBaudRate(int baudRate);

signals:
    /**
     * @brief Emitted when a USB device is connected
     */
    void deviceConnected(const QString &deviceId);

    /**
     * @brief Emitted when a USB device is disconnected
     */
    void deviceDisconnected();

    /**
     * @brief Emitted when data is received from the device
     */
    void dataReceived(const QByteArray &data);

    /**
     * @brief Emitted when connection status changes
     */
    void connectionStatusChanged(const QString &status);

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString &errorMessage);

private:
    bool m_connected;
    QString m_deviceId;
    bool m_autoReconnect;
    int m_baudRate;
    
    // Platform-specific implementation methods
    QList<QVariantMap> scanForDevicesLinux();
    QList<QVariantMap> scanForDevicesWindows();
    QList<QVariantMap> scanForDevicesMacOS();
};

#endif // USBMANAGER_H

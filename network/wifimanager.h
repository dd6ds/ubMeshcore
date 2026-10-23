#ifndef WIFIMANAGER_H
#define WIFIMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * @brief Manages WiFi connections for MeshCore
 *
 * Features:
 * - WiFi network scanning
 * - Connection to WiFi networks with SSID and password
 * - Auto-reconnect functionality
 * - Connection status monitoring
 * - Signal strength monitoring
 */
class WiFiManager : public QObject
{
    Q_OBJECT

public:
    explicit WiFiManager(QObject *parent = nullptr);
    ~WiFiManager() override;

    /**
     * @brief Scan for available WiFi networks
     * @return List of available networks with their properties
     */
    QList<QVariantMap> scanForNetworks();

    /**
     * @brief Connect to a WiFi network
     * @param ssid SSID of the network to connect to
     * @param password Password for the network (empty for open networks)
     * @param autoReconnect Whether to auto-reconnect if connection is lost
     * @return true if connection was successful
     */
    bool connectToNetwork(const QString &ssid, const QString &password = {}, bool autoReconnect = true);

    /**
     * @brief Disconnect from current WiFi network
     */
    void disconnect();

    /**
     * @brief Check if currently connected to WiFi
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current connection status
     * @return Connection status string
     */
    QString getConnectionStatus() const;

    /**
     * @brief Get current signal strength (RSSI)
     * @return Signal strength in dBm
     */
    int getSignalStrength() const;

    /**
     * @brief Get information about the current connection
     * @return Connection information map
     */
    QVariantMap getConnectionInfo() const;

    /**
     * @brief Set auto-reconnect preference
     * @param enable Whether to enable auto-reconnect
     */
    void setAutoReconnect(bool enable);

    /**
     * @brief Forget a saved network
     * @param ssid SSID of the network to forget
     * @return true if network was forgotten
     */
    bool forgetNetwork(const QString &ssid);

    /**
     * @brief Get list of saved networks
     * @return List of saved network SSIDs
     */
    QStringList getSavedNetworks();

signals:
    /**
     * @brief Emitted when a WiFi network is connected
     */
    void networkConnected(const QString &ssid);

    /**
     * @brief Emitted when disconnected from WiFi
     */
    void networkDisconnected();

    /**
     * @brief Emitted when scan results are available
     */
    void scanResultsAvailable(const QList<QVariantMap> &networks);

    /**
     * @brief Emitted when signal strength changes
     */
    void signalStrengthChanged(int rssi);

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
    QString m_ssid;
    QString m_password;
    bool m_autoReconnect;
    int m_signalStrength;
    
    // Internal helper methods
    bool isNetworkSecured(const QVariantMap &network) const;
};

#endif // WIFIMANAGER_H

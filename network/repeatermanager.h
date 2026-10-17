#ifndef REPEATERMANAGER_H
#define REPEATERMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QList>

/**
 * @brief Manages repeater connections and operations for MeshCore
 *
 * Features:
 * - Repeater discovery and connection
 * - Remote management of repeaters
 * - Path tracing to/from repeaters
 * - Repeater region management
 * - Repeater status monitoring
 * - Path hash size configuration
 * - Repeater neighbors discovery
 */
class RepeaterManager : public QObject
{
    Q_OBJECT

public:
    explicit RepeaterManager(QObject *parent = nullptr);
    ~RepeaterManager() override;

    /**
     * @brief Scan for nearby repeaters
     * @return List of discovered repeaters
     */
    QList<QVariantMap> scanForRepeaters();

    /**
     * @brief Connect to a repeater
     * @param repeaterId ID of the repeater to connect to
     * @param credentials Optional credentials for authentication
     * @return true if connection was successful
     */
    bool connectToRepeater(const QString &repeaterId, const QVariantMap &credentials = {});

    /**
     * @brief Disconnect from current repeater
     */
    void disconnect();

    /**
     * @brief Check if currently connected to a repeater
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current repeater connection information
     * @return Repeater connection info map
     */
    QVariantMap getConnectionInfo() const;

    /**
     * @brief Trace path to a repeater
     * @param repeaterId ID of the target repeater
     * @return List of hops in the path
     */
    QList<QVariantMap> tracePathToRepeater(const QString &repeaterId);

    /**
     * @brief Trace path from a repeater
     * @param repeaterId ID of the source repeater
     * @return List of hops in the path
     */
    QList<QVariantMap> tracePathFromRepeater(const QString &repeaterId);

    /**
     * @brief Get repeater status information
     * @return Repeater status map
     */
    QVariantMap getRepeaterStatus();

    /**
     * @brief Get list of repeater neighbors
     * @return List of neighbor repeaters
     */
    QList<QVariantMap> getNeighbors();

    /**
     * @brief Discover nearby neighbors
     * @return List of discovered neighbors
     */
    QList<QVariantMap> discoverNeighbors();

    /**
     * @brief Configure path hash size
     * @param size Path hash size (1, 2, or 3 bytes)
     * @return true if configuration was successful
     */
    bool configurePathHashSize(int size);

    /**
     * @brief Get current path hash size
     * @return Current path hash size
     */
    int getPathHashSize() const;

    /**
     * @brief Manage repeater regions
     * @param regionName Name of the region
     * @param action Action to perform ("add", "remove", "update")
     * @param config Region configuration
     * @return true if operation was successful
     */
    bool manageRegion(const QString &regionName, const QString &action, const QVariantMap &config = {});

    /**
     * @brief Get list of configured regions
     * @return List of regions
     */
    QList<QVariantMap> getRegions();

    /**
     * @brief Get region information from a repeater
     * @param repeaterId ID of the repeater
     * @return Region information map
     */
    QVariantMap getRegionInfo(const QString &repeaterId);

    /**
     * @brief Enable or disable repeat mode
     * @param enable Whether to enable repeat mode
     * @param frequency Optional frequency to use when enabling
     * @return true if operation was successful
     */
    bool setRepeatMode(bool enable, double frequency = 0.0);

    /**
     * @brief Resume existing session
     * @return true if session was resumed
     */
    bool resumeSession();

    /**
     * @brief Save current regions configuration
     * @return true if save was successful
     */
    bool saveRegions();

    /**
     * @brief Get owner info for a repeater
     * @param repeaterId ID of the repeater
     * @return Owner information map
     */
    QVariantMap getOwnerInfo(const QString &repeaterId);

    /**
     * @brief Request name from a repeater
     * @param repeaterId ID of the repeater
     * @return Repeater name or empty string
     */
    QString requestRepeaterName(const QString &repeaterId);

    /**
     * @brief Change repeater identity keys
     * @param newPrefix New prefix for the repeater
     * @return true if change was successful
     */
    bool changeRepeaterIdentity(const QString &newPrefix);

    /**
     * @brief Configure default region for repeater
     * @param regionName Name of the default region
     * @return true if configuration was successful
     */
    bool configureDefaultRegion(const QString &regionName);

signals:
    /**
     * @brief Emitted when a repeater is connected
     */
    void repeaterConnected(const QString &repeaterId);

    /**
     * @brief Emitted when disconnected from a repeater
     */
    void repeaterDisconnected();

    /**
     * @brief Emitted when repeater status changes
     */
    void repeaterStatusChanged(const QVariantMap &status);

    /**
     * @brief Emitted when neighbors list changes
     */
    void neighborsChanged(const QList<QVariantMap> &neighbors);

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString &errorMessage);

private:
    bool m_connected;
    QString m_repeaterId;
    int m_pathHashSize;
    QList<QVariantMap> m_neighbors;
    
    // Internal helper methods
    bool validateRepeaterId(const QString &repeaterId) const;
    bool isValidPathHashSize(int size) const;
};

#endif // REPEATERMANAGER_H

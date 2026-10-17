#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>

/**
 * @brief Manages configuration import and export for MeshCore
 *
 * Features:
 * - Export SQLite database to file
 * - Import SQLite database from file
 * - Export companion configuration as JSON
 * - Import companion configuration from JSON
 * - Export discovered contacts as JSON
 * - Import discovered contacts from JSON
 * - Export/import region scopes
 * - Export/import channel configurations
 */
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() override;

    /**
     * @brief Export the application SQLite database
     * @param filePath Path to save the database file
     * @return true if export was successful
     */
    bool exportDatabase(const QString &filePath);

    /**
     * @brief Import an SQLite database
     * @param filePath Path to the database file to import
     * @return true if import was successful
     */
    bool importDatabase(const QString &filePath);

    /**
     * @brief Export companion configuration as JSON
     * @param config Companion configuration map
     * @param filePath Path to save the JSON file
     * @param includeRegionScopes Whether to include region scopes in export
     * @param includeChannelSettings Whether to include channel settings
     * @return true if export was successful
     */
    bool exportCompanionConfig(const QVariantMap &config, const QString &filePath,
                               bool includeRegionScopes = true, bool includeChannelSettings = true);

    /**
     * @brief Import companion configuration from JSON
     * @param filePath Path to the JSON file to import
     * @return QVariantMap containing the imported configuration
     */
    QVariantMap importCompanionConfig(const QString &filePath);

    /**
     * @brief Export discovered contacts as JSON
     * @param contacts List of discovered contacts
     * @param filePath Path to save the JSON file
     * @return true if export was successful
     */
    bool exportDiscoveredContacts(const QList<QVariantMap> &contacts, const QString &filePath);

    /**
     * @brief Import discovered contacts from JSON
     * @param filePath Path to the JSON file to import
     * @return List of imported contacts
     */
    QList<QVariantMap> importDiscoveredContacts(const QString &filePath);

    /**
     * @brief Export region scopes configuration
     * @param regionScopes Map of region name to scope configuration
     * @param filePath Path to save the JSON file
     * @return true if export was successful
     */
    bool exportRegionScopes(const QMap<QString, QVariantMap> &regionScopes, const QString &filePath);

    /**
     * @brief Import region scopes configuration
     * @param filePath Path to the JSON file to import
     * @return Map of region name to scope configuration
     */
    QMap<QString, QVariantMap> importRegionScopes(const QString &filePath);

    /**
     * @brief Export channel configurations
     * @param channels List of channel configurations
     * @param filePath Path to save the JSON file
     * @return true if export was successful
     */
    bool exportChannels(const QList<QVariantMap> &channels, const QString &filePath);

    /**
     * @brief Import channel configurations
     * @param filePath Path to the JSON file to import
     * @return List of imported channel configurations
     */
    QList<QVariantMap> importChannels(const QString &filePath);

    /**
     * @brief Validate a configuration file before import
     * @param filePath Path to the configuration file
     * @return true if the file is valid
     */
    bool validateConfigFile(const QString &filePath);

    /**
     * @brief Get the default configuration directory path
     * @return Path to the configuration directory
     */
    QString getConfigDirectory();

signals:
    /**
     * @brief Emitted when a configuration export completes
     */
    void exportCompleted(const QString &filePath, bool success);

    /**
     * @brief Emitted when a configuration import completes
     */
    void importCompleted(const QString &filePath, bool success);

    /**
     * @brief Emitted when an error occurs during config processing
     */
    void errorOccurred(const QString &errorMessage);

private:
    // Internal helper methods
    bool writeJsonToFile(const QVariant &data, const QString &filePath);
    QVariant readJsonFromFile(const QString &filePath);
};

#endif // CONFIGMANAGER_H

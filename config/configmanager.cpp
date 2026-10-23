#include "configmanager.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "ConfigManager initialized";
}

ConfigManager::~ConfigManager()
{
    qDebug() << "ConfigManager destroyed";
}

bool ConfigManager::exportDatabase(const QString &filePath)
{
    qDebug() << "Exporting database to:" << filePath;
    // Stub: In a real implementation, copy the SQLite database file
    emit exportCompleted(filePath, true);
    return true;
}

bool ConfigManager::importDatabase(const QString &filePath)
{
    qDebug() << "Importing database from:" << filePath;
    // Stub: In a real implementation, copy and replace the SQLite database file
    emit importCompleted(filePath, true);
    return true;
}

bool ConfigManager::exportCompanionConfig(const QVariantMap &config, const QString &filePath,
                                         bool includeRegionScopes, bool includeChannelSettings)
{
    QVariantMap exportConfig = config;
    
    if (!includeRegionScopes) {
        exportConfig.remove("regionScopes");
    }
    
    if (!includeChannelSettings) {
        exportConfig.remove("channels");
    }
    
    return writeJsonToFile(exportConfig, filePath);
}

QVariantMap ConfigManager::importCompanionConfig(const QString &filePath)
{
    QVariant data = readJsonFromFile(filePath);
    if (data.isValid() && data.canConvert<QVariantMap>()) {
        return data.value<QVariantMap>();
    }
    return QVariantMap();
}

bool ConfigManager::exportDiscoveredContacts(const QList<QVariantMap> &contacts, const QString &filePath)
{
    QJsonArray contactsArray;
    for (const QVariantMap &contact : contacts) {
        QJsonObject contactObj = QJsonObject::fromVariantMap(contact);
        contactsArray.append(contactObj);
    }
    
    QJsonDocument doc(contactsArray);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QList<QVariantMap> ConfigManager::importDiscoveredContacts(const QString &filePath)
{
    QList<QVariantMap> contacts;
    QVariant data = readJsonFromFile(filePath);
    
    if (data.isValid() && data.canConvert<QJsonArray>()) {
        QJsonArray array = data.value<QJsonArray>();
        for (const QJsonValue &value : array) {
            if (value.isObject()) {
                contacts.append(value.toObject().toVariantMap());
            }
        }
    }
    
    return contacts;
}

bool ConfigManager::exportRegionScopes(const QMap<QString, QVariantMap> &regionScopes, const QString &filePath)
{
    QJsonObject scopesObj;
    for (auto it = regionScopes.constBegin(); it != regionScopes.constEnd(); ++it) {
        scopesObj[it.key()] = QJsonObject::fromVariantMap(it.value());
    }
    
    QJsonDocument doc(scopesObj);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QMap<QString, QVariantMap> ConfigManager::importRegionScopes(const QString &filePath)
{
    QMap<QString, QVariantMap> regionScopes;
    QVariant data = readJsonFromFile(filePath);
    
    if (data.isValid() && data.canConvert<QJsonObject>()) {
        QJsonObject obj = data.value<QJsonObject>();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            if (it.value().isObject()) {
                regionScopes[it.key()] = it.value().toObject().toVariantMap();
            }
        }
    }
    
    return regionScopes;
}

bool ConfigManager::exportChannels(const QList<QVariantMap> &channels, const QString &filePath)
{
    QJsonArray channelsArray;
    for (const QVariantMap &channel : channels) {
        QJsonObject channelObj = QJsonObject::fromVariantMap(channel);
        channelsArray.append(channelObj);
    }
    
    QJsonDocument doc(channelsArray);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QList<QVariantMap> ConfigManager::importChannels(const QString &filePath)
{
    QList<QVariantMap> channels;
    QVariant data = readJsonFromFile(filePath);
    
    if (data.isValid() && data.canConvert<QJsonArray>()) {
        QJsonArray array = data.value<QJsonArray>();
        for (const QJsonValue &value : array) {
            if (value.isObject()) {
                channels.append(value.toObject().toVariantMap());
            }
        }
    }
    
    return channels;
}

bool ConfigManager::validateConfigFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.exists()) {
        return false;
    }
    
    QVariant data = readJsonFromFile(filePath);
    return data.isValid();
}

QString ConfigManager::getConfigDirectory()
{
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
    if (!locations.isEmpty()) {
        QString dir = locations.first() + "/MeshCore";
        QDir().mkpath(dir);
        return dir;
    }
    return QDir::homePath() + "/.meshcore";
}

bool ConfigManager::writeJsonToFile(const QVariant &data, const QString &filePath)
{
    QJsonDocument doc;
    
    if (data.canConvert<QVariantMap>()) {
        doc = QJsonDocument(QJsonObject::fromVariantMap(data.value<QVariantMap>()));
    } else if (data.canConvert<QList<QVariant>>()) {
        doc = QJsonDocument(QJsonArray::fromVariantList(data.value<QList<QVariant>>()));
    } else {
        return false;
    }
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    return false;
}

QVariant ConfigManager::readJsonFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QVariant();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString();
        return QVariant();
    }
    
    if (doc.isObject()) {
        return doc.object().toVariantMap();
    } else if (doc.isArray()) {
        return doc.array().toVariantList();
    }
    
    return QVariant();
}

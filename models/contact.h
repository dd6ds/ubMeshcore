#ifndef CONTACT_H
#define CONTACT_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief Represents a contact in MeshCore
 *
 * A contact represents another node that can communicate via the mesh network.
 * Contacts can be discovered, added manually, or imported from various sources.
 */
class Contact : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString id READ id NOTIFY idChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString longName READ longName WRITE setLongName NOTIFY longNameChanged)
    Q_PROPERTY(QString nodeId READ nodeId WRITE setNodeId NOTIFY nodeIdChanged)
    Q_PROPERTY(QStringList path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int pathHashSize READ pathHashSize WRITE setPathHashSize NOTIFY pathHashSizeChanged)
    Q_PROPERTY(QString hardwareId READ hardwareId WRITE setHardwareId NOTIFY hardwareIdChanged)
    Q_PROPERTY(QDateTime lastSeen READ lastSeen WRITE setLastSeen NOTIFY lastSeenChanged)
    Q_PROPERTY(QDateTime lastAdvert READ lastAdvert WRITE setLastAdvert NOTIFY lastAdvertChanged)
    Q_PROPERTY(QDateTime lastMessage READ lastMessage WRITE setLastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(bool isFavorite READ isFavorite WRITE setFavorite NOTIFY favoriteChanged)
    Q_PROPERTY(bool isMuted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QVariantMap metadata READ metadata WRITE setMetadata NOTIFY metadataChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities WRITE setCapabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(QStringList regions READ regions WRITE setRegions NOTIFY regionsChanged)
    Q_PROPERTY(double latitude READ latitude WRITE setLatitude NOTIFY positionChanged)
    Q_PROPERTY(double longitude READ longitude WRITE setLongitude NOTIFY positionChanged)
    Q_PROPERTY(double elevation READ elevation WRITE setElevation NOTIFY positionChanged)
    Q_PROPERTY(QDateTime positionTimestamp READ positionTimestamp WRITE setPositionTimestamp NOTIFY positionChanged)

public:
    explicit Contact(QObject *parent = nullptr);
    explicit Contact(const QString &id, QObject *parent = nullptr);
    ~Contact() override;

    // Getters
    QString id() const;
    QString name() const;
    QString longName() const;
    QString nodeId() const;
    QStringList path() const;
    int pathHashSize() const;
    QString hardwareId() const;
    QDateTime lastSeen() const;
    QDateTime lastAdvert() const;
    QDateTime lastMessage() const;
    bool isFavorite() const;
    bool isMuted() const;
    QString color() const;
    QVariantMap metadata() const;
    QVariantMap capabilities() const;
    QStringList regions() const;
    double latitude() const;
    double longitude() const;
    double elevation() const;
    QDateTime positionTimestamp() const;

    // Setters
    void setName(const QString &name);
    void setLongName(const QString &longName);
    void setNodeId(const QString &nodeId);
    void setPath(const QStringList &path);
    void setPathHashSize(int size);
    void setHardwareId(const QString &hardwareId);
    void setLastSeen(const QDateTime &timestamp);
    void setLastAdvert(const QDateTime &timestamp);
    void setLastMessage(const QDateTime &timestamp);
    void setFavorite(bool favorite);
    void setMuted(bool muted);
    void setColor(const QString &color);
    void setMetadata(const QVariantMap &metadata);
    void setCapabilities(const QVariantMap &capabilities);
    void setRegions(const QStringList &regions);
    void setLatitude(double latitude);
    void setLongitude(double longitude);
    void setElevation(double elevation);
    void setPositionTimestamp(const QDateTime &timestamp);

    /**
     * @brief Check if the contact has a valid position
     * @return true if position is valid (non-zero coordinates)
     */
    bool hasValidPosition() const;

    /**
     * @brief Get the full path as a formatted string
     * @return Formatted path string
     */
    QString getFormattedPath() const;

    /**
     * @brief Get contact info as a variant map
     * @return Contact data as QVariantMap
     */
    QVariantMap toVariantMap() const;

    /**
     * @brief Update contact from variant map
     * @param data Contact data as QVariantMap
     */
    void fromVariantMap(const QVariantMap &data);

    /**
     * @brief Check if contact is online (recently seen)
     * @param thresholdMinutes Maximum minutes to consider online
     * @return true if contact is online
     */
    bool isOnline(int thresholdMinutes = 15) const;

    /**
     * @brief Check if contact has a specific capability
     * @param capability Capability to check for
     * @return true if contact has the capability
     */
    bool hasCapability(const QString &capability) const;

    /**
     * @brief Get the display name (name or longName or nodeId)
     * @return Display name
     */
    QString getDisplayName() const;

    /**
     * @brief Compare contacts by last seen timestamp
     * @param other Other contact to compare with
     * @return true if this contact was seen more recently
     */
    bool isMoreRecentThan(const Contact *other) const;

signals:
    void idChanged(const QString &id);
    void nameChanged(const QString &name);
    void longNameChanged(const QString &longName);
    void nodeIdChanged(const QString &nodeId);
    void pathChanged(const QStringList &path);
    void pathHashSizeChanged(int size);
    void hardwareIdChanged(const QString &hardwareId);
    void lastSeenChanged(const QDateTime &timestamp);
    void lastAdvertChanged(const QDateTime &timestamp);
    void lastMessageChanged(const QDateTime &timestamp);
    void favoriteChanged(bool favorite);
    void mutedChanged(bool muted);
    void colorChanged(const QString &color);
    void metadataChanged(const QVariantMap &metadata);
    void capabilitiesChanged(const QVariantMap &capabilities);
    void regionsChanged(const QStringList &regions);
    void positionChanged();

private:
    QString m_id;
    QString m_name;
    QString m_longName;
    QString m_nodeId;
    QStringList m_path;
    int m_pathHashSize;
    QString m_hardwareId;
    QDateTime m_lastSeen;
    QDateTime m_lastAdvert;
    QDateTime m_lastMessage;
    bool m_isFavorite;
    bool m_isMuted;
    QString m_color;
    QVariantMap m_metadata;
    QVariantMap m_capabilities;
    QStringList m_regions;
    double m_latitude;
    double m_longitude;
    double m_elevation;
    QDateTime m_positionTimestamp;
};

#endif // CONTACT_H

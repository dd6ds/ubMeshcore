#include "contact.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>

Contact::Contact(QObject *parent)
    : QObject(parent)
    , m_id(QUuid::createUuid().toString())
    , m_pathHashSize(0)
    , m_isFavorite(false)
    , m_isMuted(false)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_elevation(0.0)
{
}

Contact::Contact(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_pathHashSize(0)
    , m_isFavorite(false)
    , m_isMuted(false)
    , m_latitude(0.0)
    , m_longitude(0.0)
    , m_elevation(0.0)
{
}

Contact::~Contact()
{
}

// Getters
QString Contact::id() const { return m_id; }
QString Contact::name() const { return m_name; }
QString Contact::longName() const { return m_longName; }
QString Contact::nodeId() const { return m_nodeId; }
QStringList Contact::path() const { return m_path; }
int Contact::pathHashSize() const { return m_pathHashSize; }
QString Contact::hardwareId() const { return m_hardwareId; }
QDateTime Contact::lastSeen() const { return m_lastSeen; }
QDateTime Contact::lastAdvert() const { return m_lastAdvert; }
QDateTime Contact::lastMessage() const { return m_lastMessage; }
bool Contact::isFavorite() const { return m_isFavorite; }
bool Contact::isMuted() const { return m_isMuted; }
QString Contact::color() const { return m_color; }
QVariantMap Contact::metadata() const { return m_metadata; }
QVariantMap Contact::capabilities() const { return m_capabilities; }
QStringList Contact::regions() const { return m_regions; }
double Contact::latitude() const { return m_latitude; }
double Contact::longitude() const { return m_longitude; }
double Contact::elevation() const { return m_elevation; }
QDateTime Contact::positionTimestamp() const { return m_positionTimestamp; }

// Setters
void Contact::setName(const QString &name)
{
    if (m_name != name) {
        m_name = name;
        emit nameChanged(m_name);
    }
}

void Contact::setLongName(const QString &longName)
{
    if (m_longName != longName) {
        m_longName = longName;
        emit longNameChanged(m_longName);
    }
}

void Contact::setNodeId(const QString &nodeId)
{
    if (m_nodeId != nodeId) {
        m_nodeId = nodeId;
        emit nodeIdChanged(m_nodeId);
    }
}

void Contact::setPath(const QStringList &path)
{
    if (m_path != path) {
        m_path = path;
        emit pathChanged(m_path);
    }
}

void Contact::setPathHashSize(int size)
{
    if (m_pathHashSize != size) {
        m_pathHashSize = size;
        emit pathHashSizeChanged(m_pathHashSize);
    }
}

void Contact::setHardwareId(const QString &hardwareId)
{
    if (m_hardwareId != hardwareId) {
        m_hardwareId = hardwareId;
        emit hardwareIdChanged(m_hardwareId);
    }
}

void Contact::setLastSeen(const QDateTime &timestamp)
{
    if (m_lastSeen != timestamp) {
        m_lastSeen = timestamp;
        emit lastSeenChanged(m_lastSeen);
    }
}

void Contact::setLastAdvert(const QDateTime &timestamp)
{
    if (m_lastAdvert != timestamp) {
        m_lastAdvert = timestamp;
        emit lastAdvertChanged(m_lastAdvert);
    }
}

void Contact::setLastMessage(const QDateTime &timestamp)
{
    if (m_lastMessage != timestamp) {
        m_lastMessage = timestamp;
        emit lastMessageChanged(m_lastMessage);
    }
}

void Contact::setFavorite(bool favorite)
{
    if (m_isFavorite != favorite) {
        m_isFavorite = favorite;
        emit favoriteChanged(m_isFavorite);
    }
}

void Contact::setMuted(bool muted)
{
    if (m_isMuted != muted) {
        m_isMuted = muted;
        emit mutedChanged(m_isMuted);
    }
}

void Contact::setColor(const QString &color)
{
    if (m_color != color) {
        m_color = color;
        emit colorChanged(m_color);
    }
}

void Contact::setMetadata(const QVariantMap &metadata)
{
    if (m_metadata != metadata) {
        m_metadata = metadata;
        emit metadataChanged(m_metadata);
    }
}

void Contact::setCapabilities(const QVariantMap &capabilities)
{
    if (m_capabilities != capabilities) {
        m_capabilities = capabilities;
        emit capabilitiesChanged(m_capabilities);
    }
}

void Contact::setRegions(const QStringList &regions)
{
    if (m_regions != regions) {
        m_regions = regions;
        emit regionsChanged(m_regions);
    }
}

void Contact::setLatitude(double latitude)
{
    if (m_latitude != latitude) {
        m_latitude = latitude;
        emit positionChanged();
    }
}

void Contact::setLongitude(double longitude)
{
    if (m_longitude != longitude) {
        m_longitude = longitude;
        emit positionChanged();
    }
}

void Contact::setElevation(double elevation)
{
    if (m_elevation != elevation) {
        m_elevation = elevation;
        emit positionChanged();
    }
}

void Contact::setPositionTimestamp(const QDateTime &timestamp)
{
    if (m_positionTimestamp != timestamp) {
        m_positionTimestamp = timestamp;
        emit positionChanged();
    }
}

bool Contact::hasValidPosition() const
{
    return m_latitude != 0.0 && m_longitude != 0.0;
}

QString Contact::getFormattedPath() const
{
    if (m_path.isEmpty()) {
        return QString();
    }
    return m_path.join(" -> ");
}

QVariantMap Contact::toVariantMap() const
{
    QVariantMap data;
    data["id"] = m_id;
    data["name"] = m_name;
    data["longName"] = m_longName;
    data["nodeId"] = m_nodeId;
    data["path"] = m_path;
    data["pathHashSize"] = m_pathHashSize;
    data["hardwareId"] = m_hardwareId;
    data["lastSeen"] = m_lastSeen.toString(Qt::ISODate);
    data["lastAdvert"] = m_lastAdvert.toString(Qt::ISODate);
    data["lastMessage"] = m_lastMessage.toString(Qt::ISODate);
    data["isFavorite"] = m_isFavorite;
    data["isMuted"] = m_isMuted;
    data["color"] = m_color;
    data["metadata"] = m_metadata;
    data["capabilities"] = m_capabilities;
    data["regions"] = m_regions;
    data["latitude"] = m_latitude;
    data["longitude"] = m_longitude;
    data["elevation"] = m_elevation;
    data["positionTimestamp"] = m_positionTimestamp.toString(Qt::ISODate);
    return data;
}

void Contact::fromVariantMap(const QVariantMap &data)
{
    setName(data.value("name").toString());
    setLongName(data.value("longName").toString());
    setNodeId(data.value("nodeId").toString());
    setPath(data.value("path").toStringList());
    setPathHashSize(data.value("pathHashSize", 0).toInt());
    setHardwareId(data.value("hardwareId").toString());
    setLastSeen(QDateTime::fromString(data.value("lastSeen").toString(), Qt::ISODate));
    setLastAdvert(QDateTime::fromString(data.value("lastAdvert").toString(), Qt::ISODate));
    setLastMessage(QDateTime::fromString(data.value("lastMessage").toString(), Qt::ISODate));
    setFavorite(data.value("isFavorite", false).toBool());
    setMuted(data.value("isMuted", false).toBool());
    setColor(data.value("color").toString());
    setMetadata(data.value("metadata").toMap());
    setCapabilities(data.value("capabilities").toMap());
    setRegions(data.value("regions").toStringList());
    setLatitude(data.value("latitude", 0.0).toDouble());
    setLongitude(data.value("longitude", 0.0).toDouble());
    setElevation(data.value("elevation", 0.0).toDouble());
    setPositionTimestamp(QDateTime::fromString(data.value("positionTimestamp").toString(), Qt::ISODate));
}

bool Contact::isOnline(int thresholdMinutes) const
{
    if (!m_lastSeen.isValid()) {
        return false;
    }
    QDateTime now = QDateTime::currentDateTime();
    return m_lastSeen.secsTo(now) <= thresholdMinutes * 60;
}

bool Contact::hasCapability(const QString &capability) const
{
    return m_capabilities.contains(capability) && 
           m_capabilities.value(capability).toBool();
}

QString Contact::getDisplayName() const
{
    if (!m_longName.isEmpty()) {
        return m_longName;
    }
    if (!m_name.isEmpty()) {
        return m_name;
    }
    if (!m_nodeId.isEmpty()) {
        return m_nodeId;
    }
    return m_id;
}

bool Contact::isMoreRecentThan(const Contact *other) const
{
    if (!other) {
        return true;
    }
    if (!m_lastSeen.isValid()) {
        return false;
    }
    if (!other->lastSeen().isValid()) {
        return true;
    }
    return m_lastSeen > other->lastSeen();
}

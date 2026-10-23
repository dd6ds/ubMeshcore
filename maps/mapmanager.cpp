#include "mapmanager.h"
#include <QDebug>
#include <QUuid>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

MapManager::MapManager(QObject *parent)
    : QObject(parent)
    , m_initialized(false)
    , m_isInternetMap(true)
    , m_clusteringEnabled(true)
    , m_repeaterClusteringEnabled(false)
    , m_zoomLevel(12)
    , m_centerLatitude(0.0)
    , m_centerLongitude(0.0)
{
    qDebug() << "MapManager initialized";
}

MapManager::~MapManager()
{
    clearMarkers();
    clearPaths();
    qDebug() << "MapManager destroyed";
}

bool MapManager::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    qDebug() << "Initializing MapManager";
    m_initialized = true;
    emit markersChanged();
    return true;
}

void MapManager::setMapCenter(double latitude, double longitude, int zoom)
{
    m_centerLatitude = latitude;
    m_centerLongitude = longitude;
    if (zoom >= 0 && zoom <= 20) {
        m_zoomLevel = zoom;
    }
    emit mapCenterChanged(latitude, longitude, m_zoomLevel);
}

void MapManager::addNodeMarker(const QString &nodeId, double latitude, double longitude,
                              const QString &label, const QString &icon)
{
    QVariantMap marker;
    marker["id"] = nodeId;
    marker["type"] = "node";
    marker["latitude"] = latitude;
    marker["longitude"] = longitude;
    marker["label"] = label.isEmpty() ? nodeId : label;
    marker["icon"] = icon;
    
    m_markers[nodeId] = marker;
    emit markersChanged();
}

void MapManager::addRepeaterMarker(const QString &repeaterId, double latitude, double longitude,
                                  const QString &name, int signalStrength)
{
    QVariantMap marker;
    marker["id"] = repeaterId;
    marker["type"] = "repeater";
    marker["latitude"] = latitude;
    marker["longitude"] = longitude;
    marker["label"] = name.isEmpty() ? repeaterId : name;
    marker["signalStrength"] = signalStrength;
    marker["icon"] = "repeater";
    
    m_markers[repeaterId] = marker;
    emit markersChanged();
}

void MapManager::removeMarker(const QString &markerId)
{
    if (m_markers.remove(markerId) > 0) {
        emit markersChanged();
    }
}

void MapManager::clearMarkers()
{
    m_markers.clear();
    emit markersChanged();
}

void MapManager::updateNodePosition(const QString &nodeId, double latitude, double longitude)
{
    if (m_markers.contains(nodeId)) {
        QVariantMap &marker = m_markers[nodeId];
        marker["latitude"] = latitude;
        marker["longitude"] = longitude;
        emit markersChanged();
    }
}

QVariantMap MapManager::calculateLineOfSight(const QGeoCoordinate &start, const QGeoCoordinate &end)
{
    QVariantMap result;
    
    // Simple line of sight calculation (stub)
    // In a real implementation, this would use terrain elevation data
    
    double distance = start.distanceTo(end); // in meters
    double bearing = start.azimuthTo(end); // in degrees
    
    result["distance"] = distance;
    result["bearing"] = bearing;
    result["lineOfSight"] = true; // Simplified: assume LOS for now
    result["elevationProfile"] = QVariantList();
    
    return result;
}

QList<QGeoCoordinate> MapManager::calculateAntennaCoverage(const QGeoCoordinate &position,
                                                      double antennaHeight, double frequency,
                                                      double txPower)
{
    QList<QGeoCoordinate> coveragePolygon;
    
    // Stub: Create a circular coverage area
    // In a real implementation, this would use proper RF propagation models
    
    double radiusKm = 5.0; // 5km radius (simplified)
    int numPoints = 36;
    
    for (int i = 0; i < numPoints; ++i) {
        double angle = i * (360.0 / numPoints);
        QGeoCoordinate coord = position.atDistanceAndAzimuth(radiusKm * 1000, angle);
        coveragePolygon.append(coord);
    }
    
    return coveragePolygon;
}

void MapManager::fitToMarkers()
{
    if (m_markers.isEmpty()) {
        return;
    }
    
    // Calculate bounds
    double minLat = 90.0, maxLat = -90.0;
    double minLon = 180.0, maxLon = -180.0;
    
    for (const QVariantMap &marker : m_markers.values()) {
        double lat = marker.value("latitude").toDouble();
        double lon = marker.value("longitude").toDouble();
        
        minLat = qMin(minLat, lat);
        maxLat = qMax(maxLat, lat);
        minLon = qMin(minLon, lon);
        maxLon = qMax(maxLon, lon);
    }
    
    // Center the map
    double centerLat = (minLat + maxLat) / 2.0;
    double centerLon = (minLon + maxLon) / 2.0;
    
    // Calculate zoom based on bounds
    double latSpan = maxLat - minLat;
    double lonSpan = maxLon - minLon;
    int zoom = qMax(0, qMin(20, 15 - static_cast<int>(qMax(latSpan, lonSpan) * 10)));
    
    setMapCenter(centerLat, centerLon, zoom);
}

void MapManager::setClustering(bool enable)
{
    m_clusteringEnabled = enable;
    qDebug() << "Map clustering set to:" << enable;
    emit markersChanged();
}

void MapManager::setRepeaterClustering(bool enable)
{
    m_repeaterClusteringEnabled = enable;
    qDebug() << "Repeater clustering set to:" << enable;
    emit markersChanged();
}

QVariantMap MapManager::getMapBounds() const
{
    QVariantMap bounds;
    // In a real implementation, return actual bounds
    bounds["north"] = m_centerLatitude + 0.1;
    bounds["south"] = m_centerLatitude - 0.1;
    bounds["east"] = m_centerLongitude + 0.1;
    bounds["west"] = m_centerLongitude - 0.1;
    return bounds;
}

int MapManager::getZoomLevel() const
{
    return m_zoomLevel;
}

void MapManager::setMapType(bool isInternetMap)
{
    m_isInternetMap = isInternetMap;
    qDebug() << "Map type set to:" << (isInternetMap ? "Internet" : "Local");
}

bool MapManager::importMapLayers(const QString &filePath)
{
    qDebug() << "Importing map layers from:" << filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open map layers file");
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit errorOccurred("JSON parse error: " + error.errorString());
        return false;
    }
    
    if (doc.isArray()) {
        QJsonArray layers = doc.array();
        for (const QJsonValue &value : layers) {
            if (value.isObject()) {
                QJsonObject layerObj = value.toObject();
                QString name = layerObj.value("name").toString();
                QJsonArray coordsArray = layerObj.value("coordinates").toArray();
                
                QList<QGeoCoordinate> coords;
                for (const QJsonValue &coordValue : coordsArray) {
                    if (coordValue.isObject()) {
                        QJsonObject coordObj = coordValue.toObject();
                        double lat = coordObj.value("latitude").toDouble();
                        double lon = coordObj.value("longitude").toDouble();
                        coords.append(QGeoCoordinate(lat, lon));
                    }
                }
                
                if (!name.isEmpty() && !coords.isEmpty()) {
                    addCoverageLayer(name, coords);
                }
            }
        }
        emit coverageLayersChanged();
        return true;
    }
    
    return false;
}

bool MapManager::exportMapView(const QString &filePath)
{
    qDebug() << "Exporting map view to:" << filePath;
    
    QJsonArray layersArray;
    for (auto it = m_coverageLayers.constBegin(); it != m_coverageLayers.constEnd(); ++it) {
        QJsonObject layerObj;
        layerObj["name"] = it.key();
        
        QJsonArray coordsArray;
        for (const QGeoCoordinate &coord : it.value()) {
            QJsonObject coordObj;
            coordObj["latitude"] = coord.latitude();
            coordObj["longitude"] = coord.longitude();
            coordsArray.append(coordObj);
        }
        
        layerObj["coordinates"] = coordsArray;
        layersArray.append(layerObj);
    }
    
    QJsonDocument doc(layersArray);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    
    return false;
}

QString MapManager::addPath(const QList<QGeoCoordinate> &path, const QString &color)
{
    QString pathId = generateMarkerId();
    m_paths[pathId] = path;
    emit pathsChanged();
    return pathId;
}

void MapManager::removePath(const QString &pathId)
{
    if (m_paths.remove(pathId) > 0) {
        emit pathsChanged();
    }
}

void MapManager::clearPaths()
{
    m_paths.clear();
    emit pathsChanged();
}

void MapManager::addCoverageLayer(const QString &layerName, const QList<QGeoCoordinate> &coordinates,
                                 const QString &color)
{
    m_coverageLayers[layerName] = coordinates;
    emit coverageLayersChanged();
}

void MapManager::removeCoverageLayer(const QString &layerName)
{
    if (m_coverageLayers.remove(layerName) > 0) {
        emit coverageLayersChanged();
    }
}

double MapManager::getElevation(const QGeoCoordinate &coordinate)
{
    // Stub: In a real implementation, query elevation data
    // For now, return a random elevation
    return 100.0 + qrand() % 1000;
}

bool MapManager::hasElevationData(const QGeoCoordinate &coordinate)
{
    // Stub: In a real implementation, check if elevation data is available
    return true;
}

QList<QVariantMap> MapManager::getAllMarkers() const
{
    return m_markers.values();
}

QString MapManager::generateMarkerId() const
{
    return QUuid::createUuid().toString();
}

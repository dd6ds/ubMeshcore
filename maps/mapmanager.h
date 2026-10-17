#ifndef MAPMANAGER_H
#define MAPMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QList>
#include <QGeoCoordinate>

/**
 * @brief Manages map-related functionality for MeshCore
 *
 * Features:
 * - Local map display and management
 * - Internet map integration
 * - Node position tracking on maps
 * - Coverage area visualization
 * - Path tracing visualization
 * - Antenna coverage calculation
 * - Line of sight calculation
 * - Map clustering
 */
class MapManager : public QObject
{
    Q_OBJECT

public:
    explicit MapManager(QObject *parent = nullptr);
    ~MapManager() override;

    /**
     * @brief Initialize the map system
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Set the center position for the map
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param zoom Zoom level
     */
    void setMapCenter(double latitude, double longitude, int zoom = 12);

    /**
     * @brief Add a node marker to the map
     * @param nodeId ID of the node
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param label Label for the marker
     * @param icon Icon to use for the marker
     */
    void addNodeMarker(const QString &nodeId, double latitude, double longitude,
                      const QString &label = {}, const QString &icon = "node");

    /**
     * @brief Add a repeater marker to the map
     * @param repeaterId ID of the repeater
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param name Name of the repeater
     * @param signalStrength Signal strength in dBm
     */
    void addRepeaterMarker(const QString &repeaterId, double latitude, double longitude,
                          const QString &name = {}, int signalStrength = -70);

    /**
     * @brief Remove a marker from the map
     * @param markerId ID of the marker to remove
     */
    void removeMarker(const QString &markerId);

    /**
     * @brief Clear all markers from the map
     */
    void clearMarkers();

    /**
     * @brief Update a node's position on the map
     * @param nodeId ID of the node
     * @param latitude New latitude
     * @param longitude New longitude
     */
    void updateNodePosition(const QString &nodeId, double latitude, double longitude);

    /**
     * @brief Calculate line of sight between two points
     * @param start Start position (latitude, longitude, elevation in meters)
     * @param end End position (latitude, longitude, elevation in meters)
     * @return true if line of sight exists, with details in the result map
     */
    QVariantMap calculateLineOfSight(const QGeoCoordinate &start, const QGeoCoordinate &end);

    /**
     * @brief Calculate antenna coverage area
     * @param position Position of the antenna (latitude, longitude, elevation)
     * @param antennaHeight Height of antenna in meters
     * @param frequency Frequency in MHz
     * @param txPower Transmit power in dBm
     * @return Coverage area as a polygon
     */
    QList<QGeoCoordinate> calculateAntennaCoverage(const QGeoCoordinate &position,
                                                  double antennaHeight, double frequency,
                                                  double txPower);

    /**
     * @brief Fit map to show all markers
     */
    void fitToMarkers();

    /**
     * @brief Cluster markers on the map
     * @param enable Whether to enable clustering
     */
    void setClustering(bool enable);

    /**
     * @brief Cluster repeater markers specifically
     * @param enable Whether to cluster repeater markers
     */
    void setRepeaterClustering(bool enable);

    /**
     * @brief Get the current map bounds
     * @return Map bounds as (north, south, east, west)
     */
    QVariantMap getMapBounds() const;

    /**
     * @brief Get the current zoom level
     * @return Current zoom level
     */
    int getZoomLevel() const;

    /**
     * @brief Set map type (local or internet)
     * @param isInternetMap true for internet map, false for local map
     */
    void setMapType(bool isInternetMap);

    /**
     * @brief Import map layers from JSON
     * @param filePath Path to the JSON file
     * @return true if import was successful
     */
    bool importMapLayers(const QString &filePath);

    /**
     * @brief Export current map view as JSON
     * @param filePath Path to save the JSON file
     * @return true if export was successful
     */
    bool exportMapView(const QString &filePath);

    /**
     * @brief Add a path to visualize on the map
     * @param path List of coordinates representing the path
     * @param color Color for the path line
     * @return Path ID
     */
    QString addPath(const QList<QGeoCoordinate> &path, const QString &color = "blue");

    /**
     * @brief Remove a path from the map
     * @param pathId ID of the path to remove
     */
    void removePath(const QString &pathId);

    /**
     * @brief Clear all paths from the map
     */
    void clearPaths();

    /**
     * @brief Add a coverage layer to the map
     * @param layerName Name of the coverage layer
     * @param coordinates List of coordinates defining the layer
     * @param color Color for the layer
     */
    void addCoverageLayer(const QString &layerName, const QList<QGeoCoordinate> &coordinates,
                         const QString &color = "green");

    /**
     * @brief Remove a coverage layer from the map
     * @param layerName Name of the layer to remove
     */
    void removeCoverageLayer(const QString &layerName);

    /**
     * @brief Get elevation data for a coordinate
     * @param coordinate Coordinate to get elevation for
     * @return Elevation in meters, or NaN if not available
     */
    double getElevation(const QGeoCoordinate &coordinate);

    /**
     * @brief Check if elevation data is available for a point
     * @param coordinate Coordinate to check
     * @return true if elevation data is available
     */
    bool hasElevationData(const QGeoCoordinate &coordinate);

    /**
     * @brief Get all markers on the map
     * @return List of all markers
     */
    QList<QVariantMap> getAllMarkers() const;

signals:
    /**
     * @brief Emitted when the map center changes
     */
    void mapCenterChanged(double latitude, double longitude, int zoom);

    /**
     * @brief Emitted when markers are added or updated
     */
    void markersChanged();

    /**
     * @brief Emitted when paths are added or removed
     */
    void pathsChanged();

    /**
     * @brief Emitted when coverage layers change
     */
    void coverageLayersChanged();

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString &errorMessage);

private:
    bool m_initialized;
    bool m_isInternetMap;
    bool m_clusteringEnabled;
    bool m_repeaterClusteringEnabled;
    int m_zoomLevel;
    double m_centerLatitude;
    double m_centerLongitude;
    
    // Internal data structures
    QMap<QString, QVariantMap> m_markers;
    QMap<QString, QList<QGeoCoordinate>> m_paths;
    QMap<QString, QList<QGeoCoordinate>> m_coverageLayers;
    
    // Helper methods
    QString generateMarkerId() const;
};

#endif // MAPMANAGER_H

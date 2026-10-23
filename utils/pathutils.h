#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Utility functions for path handling in MeshCore
 *
 * Features:
 * - Path validation
 * - Path encoding/decoding
 * - Path hash size conversion
 * - Path comparison
 * - Path manipulation
 */
class PathUtils : public QObject
{
    Q_OBJECT

public:
    explicit PathUtils(QObject *parent = nullptr);
    ~PathUtils() override;

    /**
     * @brief Validate a path string
     * @param path Path to validate
     * @return true if path is valid
     */
    static bool validatePath(const QString &path);

    /**
     * @brief Validate path hash size
     * @param size Size to validate
     * @return true if size is valid (1, 2, or 3)
     */
    static bool validatePathHashSize(int size);

    /**
     * @brief Parse a path string into segments
     * @param path Path string to parse
     * @return List of path segments
     */
    static QStringList parsePath(const QString &path);

    /**
     * @brief Convert path segments to string
     * @param segments List of path segments
     * @return Path string
     */
    static QString pathToString(const QStringList &segments);

    /**
     * @brief Check if a path is multi-byte
     * @param path Path to check
     * @return true if path contains multi-byte segments
     */
    static bool isMultiBytePath(const QString &path);

    /**
     * @brief Convert path to specific hash size
     * @param path Path to convert
     * @param targetSize Target hash size (1, 2, or 3)
     * @return Converted path
     */
    static QString convertPathHashSize(const QString &path, int targetSize);

    /**
     * @brief Get path hash size
     * @param path Path to check
     * @return Hash size (1, 2, or 3)
     */
    static int getPathHashSize(const QString &path);

    /**
     * @brief Compare two paths
     * @param path1 First path
     * @param path2 Second path
     * @return true if paths are equal
     */
    static bool comparePaths(const QString &path1, const QString &path2);

    /**
     * @brief Check if path is valid for a specific hash size
     * @param path Path to check
     * @param size Hash size to validate against
     * @return true if path is valid for the specified size
     */
    static bool isPathValidForSize(const QString &path, int size);

    /**
     * @brief Convert 3-byte path to 2-byte (for compatibility)
     * @param path 3-byte path
     * @return 2-byte path
     */
    static QString convert3ByteTo2Byte(const QString &path);

    /**
     * @brief Get the default path hash size from settings
     * @return Default path hash size
     */
    static int getDefaultPathHashSize();

    /**
     * @brief Set the default path hash size
     * @param size Default path hash size to set
     */
    static void setDefaultPathHashSize(int size);

    /**
     * @brief Generate a path hash from node IDs
     * @param nodeIds List of node IDs
     * @param hashSize Hash size to use
     * @return Generated path hash
     */
    static QString generatePathHash(const QStringList &nodeIds, int hashSize);

    /**
     * @brief Get the reverse path
     * @param path Path to reverse
     * @return Reversed path
     */
    static QString getReversePath(const QString &path);

    /**
     * @brief Check if path contains a specific node
     * @param path Path to check
     * @param nodeId Node ID to find
     * @return true if node is in path
     */
    static bool pathContainsNode(const QString &path, const QString &nodeId);

signals:
    /**
     * @brief Emitted when default path hash size changes
     */
    void defaultPathHashSizeChanged(int size);

private:
    static int s_defaultPathHashSize;
};

#endif // PATHUTILS_H

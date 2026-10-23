#include "pathutils.h"
#include <QDebug>

int PathUtils::s_defaultPathHashSize = 2;

PathUtils::PathUtils(QObject *parent)
    : QObject(parent)
{
    qDebug() << "PathUtils initialized";
}

PathUtils::~PathUtils()
{
    qDebug() << "PathUtils destroyed";
}

bool PathUtils::validatePath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    
    // Check for valid characters
    // In MeshCore, paths typically contain printable ASCII characters
    for (QChar c : path) {
        if (c.unicode() < 33 || c.unicode() > 126) {
            return false;
        }
    }
    
    return true;
}

bool PathUtils::validatePathHashSize(int size)
{
    return size == 1 || size == 2 || size == 3;
}

QStringList PathUtils::parsePath(const QString &path)
{
    QStringList segments;
    
    if (path.isEmpty()) {
        return segments;
    }
    
    // Split path by separator (typically '!', '!', or custom delimiters)
    // For simplicity, split by individual characters or segments
    for (QChar c : path) {
        if (c != '!' && c != ' ' && c != ',') {
            segments.append(QString(c));
        }
    }
    
    return segments;
}

QString PathUtils::pathToString(const QStringList &segments)
{
    return segments.join("");
}

bool PathUtils::isMultiBytePath(const QString &path)
{
    // Check if path contains non-ASCII or multi-byte characters
    for (QChar c : path) {
        if (c.unicode() > 127) {
            return true;
        }
    }
    return false;
}

QString PathUtils::convertPathHashSize(const QString &path, int targetSize)
{
    if (!validatePathHashSize(targetSize)) {
        qWarning() << "Invalid target hash size:" << targetSize;
        return path;
    }
    
    // Stub: In a real implementation, convert the path
    // For now, just return the path as-is
    return path;
}

int PathUtils::getPathHashSize(const QString &path)
{
    // Determine hash size based on path length and content
    if (path.length() <= 2) {
        return 1;
    } else if (path.length() <= 4) {
        return 2;
    } else {
        return 3;
    }
}

bool PathUtils::comparePaths(const QString &path1, const QString &path2)
{
    if (path1.length() != path2.length()) {
        return false;
    }
    return path1 == path2;
}

bool PathUtils::isPathValidForSize(const QString &path, int size)
{
    int pathSize = getPathHashSize(path);
    return pathSize == size;
}

QString PathUtils::convert3ByteTo2Byte(const QString &path)
{
    if (getPathHashSize(path) != 3) {
        return path;
    }
    
    // Stub: In a real implementation, convert 3-byte to 2-byte
    // For now, just truncate
    return path.left(path.length() - 1);
}

int PathUtils::getDefaultPathHashSize()
{
    return s_defaultPathHashSize;
}

void PathUtils::setDefaultPathHashSize(int size)
{
    if (validatePathHashSize(size)) {
        s_defaultPathHashSize = size;
        emit defaultPathHashSizeChanged(size);
    }
}

QString PathUtils::generatePathHash(const QStringList &nodeIds, int hashSize)
{
    if (!validatePathHashSize(hashSize)) {
        return QString();
    }
    
    // Stub: Generate a simple hash from node IDs
    QString hash;
    for (const QString &nodeId : nodeIds) {
        if (!nodeId.isEmpty()) {
            hash += nodeId.left(hashSize);
        }
    }
    
    return hash;
}

QString PathUtils::getReversePath(const QString &path)
{
    QStringList segments = parsePath(path);
    std::reverse(segments.begin(), segments.end());
    return pathToString(segments);
}

bool PathUtils::pathContainsNode(const QString &path, const QString &nodeId)
{
    if (nodeId.isEmpty() || path.isEmpty()) {
        return false;
    }
    
    // Simple check: if nodeId appears in path
    return path.contains(nodeId);
}

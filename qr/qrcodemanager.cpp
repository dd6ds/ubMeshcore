#include "qrcodemanager.h"
#include <QDebug>
#include <QImage>
#include <QPainter>

QRCodeManager::QRCodeManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "QRCodeManager initialized";
}

QRCodeManager::~QRCodeManager()
{
    qDebug() << "QRCodeManager destroyed";
}

QByteArray QRCodeManager::generateContactQRCode(const QString &contactId)
{
    QString url = generateContactUrl(contactId);
    return generateQRCodeImage(url);
}

QByteArray QRCodeManager::generateChannelQRCode(const QString &channelId, const QStringList &regionScopes)
{
    QString url = generateChannelUrl(channelId, regionScopes);
    return generateQRCodeImage(url);
}

QByteArray QRCodeManager::generateConfigQRCode(const QString &nodeInfo)
{
    // For now, just encode the JSON directly
    // In a real implementation, this would use proper QR encoding
    return generateQRCodeImage(nodeInfo);
}

QString QRCodeManager::decodeQRCode(const QByteArray &imageData)
{
    // Stub: In a real implementation, use a QR code decoder library
    // For now, return a placeholder
    qDebug() << "Decoding QR code (stub)";
    return QString();
}

QMap<QString, QString> QRCodeManager::parseMeshCoreUrl(const QString &url)
{
    QMap<QString, QString> result;
    
    if (!url.startsWith("meshcore://")) {
        return result;
    }
    
    QString path = url.mid(12); // Remove "meshcore://"
    QStringList parts = path.split('/');
    
    if (parts.isEmpty()) {
        return result;
    }
    
    // First part is the type
    if (parts[0] == "contact" && parts.size() > 1) {
        result["type"] = "contact";
        result["id"] = parts[1];
    } else if (parts[0] == "channel" && parts.size() > 1) {
        result["type"] = "channel";
        result["id"] = parts[1];
        if (parts.size() > 2) {
            result["scopes"] = parts[2];
        }
    } else if (parts[0] == "node" && parts.size() > 1) {
        result["type"] = "node";
        result["id"] = parts[1];
    }
    
    // Parse query parameters if present
    if (parts.size() > 0) {
        QString lastPart = parts.last();
        if (lastPart.contains('?')) {
            QStringList queryParts = lastPart.split('?');
            if (queryParts.size() > 1) {
                QString query = queryParts[1];
                QStringList params = query.split('&');
                for (const QString &param : params) {
                    QStringList keyValue = param.split('=');
                    if (keyValue.size() == 2) {
                        result[keyValue[0]] = keyValue[1];
                    }
                }
            }
        }
    }
    
    return result;
}

QString QRCodeManager::generateContactUrl(const QString &contactId, const QString &name)
{
    QString url = "meshcore://contact/" + contactId;
    if (!name.isEmpty()) {
        url += "?name=" + QUrl::toPercentEncoding(name);
    }
    return url;
}

QString QRCodeManager::generateChannelUrl(const QString &channelId, const QStringList &regionScopes)
{
    QString url = "meshcore://channel/" + channelId;
    if (!regionScopes.isEmpty()) {
        url += "?scopes=" + QUrl::toPercentEncoding(regionScopes.join(","));
    }
    return url;
}

QByteArray QRCodeManager::generateQRCodeImage(const QString &data, int size)
{
    // Stub implementation: In a real app, use a QR code generation library
    // For now, return a placeholder image
    
    QImage image(size, size, QImage::Format_RGB888);
    image.fill(Qt::white);
    
    QPainter painter(&image);
    painter.setPen(Qt::black);
    painter.setBrush(Qt::black);
    
    // Draw a simple pattern to represent a QR code
    int cellSize = size / 10;
    for (int i = 0; i < 10; i += 2) {
        for (int j = 0; j < 10; j += 2) {
            painter.drawRect(i * cellSize, j * cellSize, cellSize, cellSize);
        }
    }
    
    painter.end();
    
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    buffer.close();
    
    return byteArray;
}

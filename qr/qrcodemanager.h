#ifndef QRCODEMANAGER_H
#define QRCODEMANAGER_H

#include <QObject>
#include <QString>
#include <QByteArray>

/**
 * @brief Manages QR code generation and scanning for MeshCore
 *
 * Features:
 * - Generate QR codes for contact sharing, channel sharing, node info
 * - Scan QR codes to extract MeshCore URLs and data
 * - Support for meshcore:// URLs
 * - Configurable QR code appearance (black on white background)
 */
class QRCodeManager : public QObject
{
    Q_OBJECT

public:
    explicit QRCodeManager(QObject *parent = nullptr);
    ~QRCodeManager() override;

    /**
     * @brief Generate a QR code image for sharing a contact
     * @param contactId The contact ID to encode
     * @return QByteArray containing the QR code image data
     */
    QByteArray generateContactQRCode(const QString &contactId);

    /**
     * @brief Generate a QR code image for sharing a channel
     * @param channelId The channel ID to encode
     * @param regionScopes Optional region scopes to attach
     * @return QByteArray containing the QR code image data
     */
    QByteArray generateChannelQRCode(const QString &channelId, const QStringList &regionScopes = {});

    /**
     * @brief Generate a QR code for node configuration
     * @param nodeInfo JSON string containing node configuration
     * @return QByteArray containing the QR code image data
     */
    QByteArray generateConfigQRCode(const QString &nodeInfo);

    /**
     * @brief Decode a QR code image
     * @param imageData The QR code image data to decode
     * @return Decoded string (meshcore:// URL or other data)
     */
    QString decodeQRCode(const QByteArray &imageData);

    /**
     * @brief Extract data from a meshcore:// URL
     * @param url The URL to parse
     * @return Map of extracted parameters (type, id, scopes, etc.)
     */
    QMap<QString, QString> parseMeshCoreUrl(const QString &url);

    /**
     * @brief Generate a meshcore:// URL for a contact
     * @param contactId The contact ID
     * @param name Optional contact name
     * @return Formatted meshcore:// URL
     */
    QString generateContactUrl(const QString &contactId, const QString &name = {});

    /**
     * @brief Generate a meshcore:// URL for a channel
     * @param channelId The channel ID
     * @param regionScopes Optional region scopes
     * @return Formatted meshcore:// URL
     */
    QString generateChannelUrl(const QString &channelId, const QStringList &regionScopes = {});

signals:
    /**
     * @brief Emitted when a QR code is successfully generated
     */
    void qrCodeGenerated(const QByteArray &imageData);

    /**
     * @brief Emitted when a QR code is successfully decoded
     */
    void qrCodeDecoded(const QString &data);

    /**
     * @brief Emitted when an error occurs during QR code processing
     */
    void errorOccurred(const QString &errorMessage);

private:
    // Internal helper methods
    QByteArray generateQRCodeImage(const QString &data, int size = 512);
};

#endif // QRCODEMANAGER_H

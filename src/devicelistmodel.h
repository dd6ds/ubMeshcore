// Bluetooth devices found during a scan, as a list model for QML.

#ifndef DEVICELISTMODEL_H
#define DEVICELISTMODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVector>

/// One Bluetooth device seen during a scan.
struct DiscoveredDevice {
    QString name;              // empty while no real name is known
    QString address;
    int rssi = 0;
    bool lowEnergy = false;
    bool nusService = false;   // Nordic UART Service seen in the advertisement
    bool hasRealName = false;

    bool isMeshCore() const;
    QString displayName() const;
};

/// BlueZ falls back to "AA-BB-CC-DD-EE-FF" for the Alias of a device whose
/// name it does not know, and Qt hands that to us as the device name. Such a
/// "name" is treated as no name, so the list does not show the address twice.
bool looksLikeAddress(const QString &name, const QString &address);

/// List model of discovered Bluetooth devices, consumed directly by QML.
class DeviceListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        AddressRole,
        RssiRole,
        LowEnergyRole,
        IsMeshCoreRole,
        HasNameRole
    };

    explicit DeviceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_devices.size(); }

    void clearDevices();
    void addOrUpdate(DiscoveredDevice device);
    /// Fills in a name resolved later (via BlueZ). Returns true if it changed.
    bool applyName(const QString &address, const QString &name, int rssi);

signals:
    void countChanged();

private:
    int indexOfAddress(const QString &address) const;

    QVector<DiscoveredDevice> m_devices;
};

#endif // DEVICELISTMODEL_H

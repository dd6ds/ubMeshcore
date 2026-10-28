// DeviceListModel: the devices found during a scan, MeshCore nodes marked.

#include "devicelistmodel.h"

bool looksLikeAddress(const QString &name, const QString &address)
{
    if (name.isEmpty())
        return true;
    QString n = name;
    QString a = address;
    n.remove(QLatin1Char(':')).remove(QLatin1Char('-')).remove(QLatin1Char(' '));
    a.remove(QLatin1Char(':')).remove(QLatin1Char('-')).remove(QLatin1Char(' '));
    a.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    return n.compare(a, Qt::CaseInsensitive) == 0;
}

bool DiscoveredDevice::isMeshCore() const
{
    return nusService || name.contains(QStringLiteral("meshcore"), Qt::CaseInsensitive);
}

QString DiscoveredDevice::displayName() const
{
    return hasRealName ? name : QStringLiteral("(no name)");
}

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DeviceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return QVariant();

    const DiscoveredDevice &device = m_devices.at(index.row());
    switch (role) {
    case NameRole:       return device.displayName();
    case AddressRole:    return device.address;
    case RssiRole:       return device.rssi;
    case LowEnergyRole:  return device.lowEnergy;
    case IsMeshCoreRole: return device.isMeshCore();
    case HasNameRole:    return device.hasRealName;
    default:             return QVariant();
    }
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        { NameRole,       "name" },
        { AddressRole,    "address" },
        { RssiRole,       "rssi" },
        { LowEnergyRole,  "lowEnergy" },
        { IsMeshCoreRole, "isMeshCore" },
        { HasNameRole,    "hasName" }
    };
}

int DeviceListModel::indexOfAddress(const QString &address) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices.at(i).address.compare(address, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void DeviceListModel::clearDevices()
{
    if (m_devices.isEmpty())
        return;
    beginResetModel();
    m_devices.clear();
    endResetModel();
    emit countChanged();
}

void DeviceListModel::addOrUpdate(DiscoveredDevice device)
{
    const int row = indexOfAddress(device.address);
    if (row >= 0) {
        const DiscoveredDevice &existing = m_devices.at(row);
        // Never let a later, nameless update wipe a name we already resolved.
        if (!device.hasRealName && existing.hasRealName) {
            device.name = existing.name;
            device.hasRealName = true;
        }
        device.nusService = device.nusService || existing.nusService;
        m_devices[row] = device;
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx);
        return;
    }

    beginInsertRows(QModelIndex(), m_devices.size(), m_devices.size());
    m_devices.append(device);
    endInsertRows();
    emit countChanged();
}

bool DeviceListModel::applyName(const QString &address, const QString &name, int rssi)
{
    const int row = indexOfAddress(address);
    if (row < 0)
        return false;

    DiscoveredDevice &device = m_devices[row];
    bool changed = false;
    if (!name.isEmpty() && device.name != name) {
        device.name = name;
        device.hasRealName = true;
        changed = true;
    }
    if (rssi != 0 && device.rssi != rssi) {
        device.rssi = rssi;
        changed = true;
    }
    if (changed) {
        const QModelIndex idx = index(row, 0);
        emit dataChanged(idx, idx);
    }
    return changed;
}

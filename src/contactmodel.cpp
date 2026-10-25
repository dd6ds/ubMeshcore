// ContactListModel: the node's contacts, favourites and "last heard" ages.

#include "contactmodel.h"

#include <QDateTime>
#include <QSettings>
#include <QStringList>
#include <algorithm>

// QSettings uses the organisation/application names set in main.cpp, i.e.
// ~/.config/ubmeshcore/ubMeshcore.conf - inside the app's AppArmor-writable
// config directory.
static const char kFavoritesKey[] = "contacts/favorites";

ContactListModel::ContactListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    loadFavorites();
}

void ContactListModel::loadFavorites()
{
    QSettings settings;
    const QStringList keys = settings.value(QLatin1String(kFavoritesKey)).toStringList();
    m_favorites.clear();
    for (const QString &k : keys)
        m_favorites.insert(k.toLower());
}

void ContactListModel::saveFavorites() const
{
    QSettings settings;
    settings.setValue(QLatin1String(kFavoritesKey), QStringList(m_favorites.values()));
}

bool ContactListModel::isFavorite(const QString &keyHex) const
{
    return m_favorites.contains(keyHex.toLower());
}

void ContactListModel::setFavorite(const QString &keyHex, bool favorite)
{
    const QString key = keyHex.toLower();
    if (key.isEmpty() || m_favorites.contains(key) == favorite)
        return;
    if (favorite)
        m_favorites.insert(key);
    else
        m_favorites.remove(key);
    saveFavorites();

    // Favourites sort to the top, so re-order the list.
    beginResetModel();
    sortByRecency();
    endResetModel();
    emit favoritesChanged();
}

void ContactListModel::toggleFavorite(const QString &keyHex)
{
    setFavorite(keyHex, !isFavorite(keyHex));
}

int ContactListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_contacts.size();
}

QString ContactListModel::typeName(quint8 type)
{
    switch (type) {
    case 1: return QStringLiteral("chat");
    case 2: return QStringLiteral("repeater");
    case 3: return QStringLiteral("room");
    case 4: return QStringLiteral("sensor");
    default: return QStringLiteral("unknown");
    }
}

static QString ageText(quint32 epoch)
{
    if (epoch == 0)
        return ContactListModel::tr("never");
    const qint64 delta = QDateTime::currentSecsSinceEpoch() - qint64(epoch);
    if (delta < 0)
        return ContactListModel::tr("just now");   // node clock slightly ahead
    if (delta < 60)
        return ContactListModel::tr("%1 s ago").arg(delta);
    if (delta < 3600)
        return ContactListModel::tr("%1 min ago").arg(delta / 60);
    if (delta < 86400)
        return ContactListModel::tr("%1 h ago").arg(delta / 3600);
    return ContactListModel::tr("%1 d ago").arg(delta / 86400);
}

bool ContactListModel::isActive(const MeshContact &c) const
{
    const quint32 heard = c.lastHeard();
    if (heard == 0)
        return false;
    return QDateTime::currentSecsSinceEpoch() - qint64(heard) <= m_activeWindowSecs;
}

QVariant ContactListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_contacts.size())
        return QVariant();
    const MeshContact &c = m_contacts.at(index.row());

    switch (role) {
    case PublicKeyRole:     return QString::fromLatin1(c.publicKey.toHex());
    case KeyPrefixRole:     return QString::fromLatin1(c.publicKey.left(4).toHex());
    case NameRole:          return c.name.isEmpty()
                                ? QString::fromLatin1(c.publicKey.left(4).toHex())
                                : c.name;
    case TypeRole:          return typeName(c.type);
    case HopsRole:          return int(c.outPathLen);
    case LastHeardRole:     return c.lastHeard();
    case LastHeardTextRole: return ageText(c.lastHeard());
    case ActiveRole:        return isActive(c);
    case LatRole:           return c.lat;
    case LonRole:           return c.lon;
    case HasLocationRole:   return !(c.lat == 0.0 && c.lon == 0.0);
    case FavoriteRole:      return m_favorites.contains(QString::fromLatin1(c.publicKey.toHex()));
    case OnNodeRole:        return c.onNode;
    default:                return QVariant();
    }
}

QHash<int, QByteArray> ContactListModel::roleNames() const
{
    return {
        { PublicKeyRole,     "publicKey" },
        { KeyPrefixRole,     "keyPrefix" },
        { NameRole,          "name" },
        { TypeRole,          "type" },
        { HopsRole,          "hops" },
        { LastHeardRole,     "lastHeard" },
        { LastHeardTextRole, "lastHeardText" },
        { ActiveRole,        "isActive" },
        { LatRole,           "lat" },
        { LonRole,           "lon" },
        { HasLocationRole,   "hasLocation" },
        { FavoriteRole,      "isFavorite" },
        { OnNodeRole,        "onNode" }
    };
}

void ContactListModel::setActiveWindowSecs(int secs)
{
    if (secs <= 0 || secs == m_activeWindowSecs)
        return;
    m_activeWindowSecs = secs;
    emit activeWindowChanged();
    refreshAges();
}

void ContactListModel::sortByRecency()
{
    // Favourites first, then most recently heard.
    std::stable_sort(m_contacts.begin(), m_contacts.end(),
                     [this](const MeshContact &a, const MeshContact &b) {
                         const bool fa = m_favorites.contains(
                             QString::fromLatin1(a.publicKey.toHex()));
                         const bool fb = m_favorites.contains(
                             QString::fromLatin1(b.publicKey.toHex()));
                         if (fa != fb)
                             return fa;
                         return a.lastHeard() > b.lastHeard();
                     });
}

void ContactListModel::upsert(const MeshContact &contact)
{
    if (contact.publicKey.size() < 6)
        return;

    // A full reset is fine here: contact tables are at most a few hundred
    // entries and this keeps the recency order without move bookkeeping.
    beginResetModel();
    bool found = false;
    for (MeshContact &existing : m_contacts) {
        if (existing.publicKey == contact.publicKey) {
            existing = contact;
            found = true;
            break;
        }
    }
    if (!found)
        m_contacts.append(contact);
    sortByRecency();
    endResetModel();
    emit countChanged();
}

bool ContactListModel::isOnNode(const QString &keyHex) const
{
    const MeshContact *c = findByKeyHex(keyHex);
    return c ? c->onNode : true;
}

QStringList ContactListModel::inactiveKeys(int days, bool skipFavorites) const
{
    const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - qint64(days) * 86400;
    QStringList keys;
    for (const MeshContact &c : m_contacts) {
        const QString key = QString::fromLatin1(c.publicKey.toHex());
        if (skipFavorites && m_favorites.contains(key))
            continue;
        if (qint64(c.lastHeard()) < cutoff)
            keys << key;
    }
    return keys;
}

QStringList ContactListModel::allKeys(bool skipFavorites) const
{
    QStringList keys;
    for (const MeshContact &c : m_contacts) {
        const QString key = QString::fromLatin1(c.publicKey.toHex());
        if (skipFavorites && m_favorites.contains(key))
            continue;
        keys << key;
    }
    return keys;
}

int ContactListModel::nodeCount() const
{
    int n = 0;
    for (const MeshContact &c : m_contacts)
        if (c.onNode)
            ++n;
    return n;
}

void ContactListModel::setOnNode(const QByteArray &publicKey, bool onNode)
{
    for (int i = 0; i < m_contacts.size(); ++i) {
        if (m_contacts.at(i).publicKey == publicKey) {
            if (m_contacts.at(i).onNode == onNode)
                return;
            m_contacts[i].onNode = onNode;
            emit dataChanged(index(i), index(i), { OnNodeRole });
            emit countChanged();
            return;
        }
    }
}

void ContactListModel::removeContact(const QByteArray &publicKey)
{
    for (int i = 0; i < m_contacts.size(); ++i) {
        if (m_contacts.at(i).publicKey == publicKey) {
            beginRemoveRows(QModelIndex(), i, i);
            m_contacts.removeAt(i);
            endRemoveRows();
            emit countChanged();
            return;
        }
    }
}

void ContactListModel::clearContacts()
{
    if (m_contacts.isEmpty())
        return;
    beginResetModel();
    m_contacts.clear();
    endResetModel();
    emit countChanged();
}

const MeshContact *ContactListModel::findByPrefix(const QByteArray &prefix) const
{
    if (prefix.isEmpty())
        return nullptr;
    for (const MeshContact &c : m_contacts) {
        if (c.publicKey.startsWith(prefix))
            return &c;
    }
    return nullptr;
}

QStringList ContactListModel::namesForHopHash(const QByteArray &hash) const
{
    QStringList forwarders, others;
    if (hash.isEmpty())
        return forwarders;
    for (const MeshContact &c : m_contacts) {
        if (!c.publicKey.startsWith(hash))
            continue;
        const QString n = c.name.isEmpty() ? QString::fromLatin1(c.publicKey.left(4).toHex())
                                           : c.name;
        if (c.type == 2 || c.type == 3)
            forwarders << n;
        else
            others << n;
    }
    // Companions do not repeat; only fall back to them if nothing else fits.
    return forwarders.isEmpty() ? others : forwarders;
}

const MeshContact *ContactListModel::findByKeyHex(const QString &hex) const
{
    return findByPrefix(QByteArray::fromHex(hex.toLatin1()));
}

QString ContactListModel::nameForKey(const QString &keyHex) const
{
    const MeshContact *c = findByKeyHex(keyHex);
    return c ? c->name : QString();
}

QString ContactListModel::typeForKey(const QString &keyHex) const
{
    const MeshContact *c = findByKeyHex(keyHex);
    return c ? typeName(c->type) : QString();
}

void ContactListModel::refreshAges()
{
    if (m_contacts.isEmpty())
        return;
    emit dataChanged(index(0), index(m_contacts.size() - 1),
                     { LastHeardTextRole, ActiveRole });
}

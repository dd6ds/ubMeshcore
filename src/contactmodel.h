// The node's contact table as a list model for QML, with favourites.

#ifndef CONTACTMODEL_H
#define CONTACTMODEL_H

#include <QAbstractListModel>
#include <QByteArray>
#include <QSet>
#include <QString>
#include <QVector>

/// One entry of the node's contact table, as delivered by RESP_CODE_CONTACT
/// (or PUSH_CODE_NEW_ADVERT, which uses the same layout).
struct MeshContact {
    QByteArray publicKey;      // 32 bytes
    quint8 type = 0;           // ADV_TYPE_*: 1 chat, 2 repeater, 3 room, 4 sensor
    quint8 flags = 0;
    qint8 outPathLen = -1;     // -1 = no known path (flood), else hop count
    QByteArray outPath;        // 64 bytes as stored by the node (for ADD_UPDATE)
    /// Stored in the node's own contact table. False for contacts only known
    /// from PUSH_NEW_ADVERT (node did not add them, e.g. its table is full) -
    /// the node refuses to send direct messages to those ("not found").
    bool onNode = true;
    QString name;
    quint32 lastAdvert = 0;    // advertiser's own clock
    double lat = 0.0;
    double lon = 0.0;
    quint32 lastMod = 0;       // companion node clock when last updated

    /// Best guess of when we last heard this node, in epoch seconds.
    /// lastMod is stamped by OUR node when the advert arrives, so it is
    /// independent of a repeater with a wrong clock; fall back to lastAdvert.
    quint32 lastHeard() const { return lastMod ? lastMod : lastAdvert; }
};

class ContactListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    /// Contacts actually stored in the node's table (count includes the ones
    /// only known from PUSH_NEW_ADVERT).
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY countChanged)
    /// Seconds after which a node counts as no longer active.
    Q_PROPERTY(int activeWindowSecs READ activeWindowSecs WRITE setActiveWindowSecs
               NOTIFY activeWindowChanged)

public:
    enum Roles {
        PublicKeyRole = Qt::UserRole + 1,
        KeyPrefixRole,
        NameRole,
        TypeRole,          // "chat" | "repeater" | "room" | "sensor" | "unknown"
        HopsRole,          // -1 = flood / unknown
        LastHeardRole,     // epoch secs
        LastHeardTextRole, // "3 min ago"
        ActiveRole,
        LatRole,
        LonRole,
        HasLocationRole,
        FavoriteRole,
        OnNodeRole         // stored in the node's contact table
    };

    explicit ContactListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_contacts.size(); }
    int nodeCount() const;
    /// Marks a contact as stored / not stored on the node.
    void setOnNode(const QByteArray &publicKey, bool onNode);
    /// Drops a contact from the list (after removing it from the node).
    void removeContact(const QByteArray &publicKey);
    int activeWindowSecs() const { return m_activeWindowSecs; }
    void setActiveWindowSecs(int secs);

    /// Inserts or updates by public key, keeping the list sorted by recency.
    void upsert(const MeshContact &contact);
    void clearContacts();

    /// Looks up a contact by the 6-byte prefix used in pushes/messages.
    const MeshContact *findByPrefix(const QByteArray &prefix) const;
    const MeshContact *findByKeyHex(const QString &hex) const;
    /// Names of the nodes whose public key starts with a path hop hash.
    /// Repeaters/rooms first (only they forward); with 1-byte hashes several
    /// nodes can match, so all candidates are returned.
    QStringList namesForHopHash(const QByteArray &hash) const;

    Q_INVOKABLE QString nameForKey(const QString &keyHex) const;
    Q_INVOKABLE QString typeForKey(const QString &keyHex) const;
    /// True if the contact is stored in the node's table (unknown keys: true,
    /// so no warning is shown for contacts that are not in the list at all).
    Q_INVOKABLE bool isOnNode(const QString &keyHex) const;
    /// Keys of contacts not heard for more than `days` (never heard counts
    /// as inactive); favourites are skipped if skipFavorites.
    Q_INVOKABLE QStringList inactiveKeys(int days, bool skipFavorites) const;
    /// Keys of all contacts, optionally without favourites.
    Q_INVOKABLE QStringList allKeys(bool skipFavorites) const;

    /// Favourites are kept on the phone (QSettings, keyed by public key), so
    /// they survive reconnects, app restarts and a contact re-sync.
    Q_INVOKABLE bool isFavorite(const QString &keyHex) const;
    Q_INVOKABLE void setFavorite(const QString &keyHex, bool favorite);
    Q_INVOKABLE void toggleFavorite(const QString &keyHex);

    static QString typeName(quint8 type);

public slots:
    /// Re-evaluates "x min ago" texts and the active flag.
    void refreshAges();

signals:
    void countChanged();
    void activeWindowChanged();
    void favoritesChanged();

private:
    void loadFavorites();
    void saveFavorites() const;
    QSet<QString> m_favorites;   // lower-case hex public keys
    bool isActive(const MeshContact &c) const;
    void sortByRecency();

    QVector<MeshContact> m_contacts;
    int m_activeWindowSecs = 24 * 3600;
};

#endif // CONTACTMODEL_H

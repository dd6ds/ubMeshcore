#include "contactmanager.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ContactManager::ContactManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "ContactManager initialized";
}

ContactManager::~ContactManager()
{
    // Clean up all contacts
    qDeleteAll(m_contacts);
    qDeleteAll(m_discoveredContacts);
    qDebug() << "ContactManager destroyed";
}

bool ContactManager::initialize()
{
    qDebug() << "ContactManager initialized";
    return true;
}

bool ContactManager::addContact(Contact *contact)
{
    if (!contact) {
        return false;
    }
    
    // Check if contact with same ID already exists
    for (Contact *existing : m_contacts) {
        if (existing->id() == contact->id()) {
            return false;
        }
    }
    
    m_contacts.append(contact);
    emit contactAdded(contact);
    return true;
}

Contact *ContactManager::addContactFromVariantMap(const QVariantMap &contactData)
{
    Contact *contact = new Contact(this);
    contact->fromVariantMap(contactData);
    
    if (addContact(contact)) {
        return contact;
    }
    
    delete contact;
    return nullptr;
}

bool ContactManager::removeContact(const QString &contactId)
{
    for (int i = 0; i < m_contacts.size(); ++i) {
        if (m_contacts[i]->id() == contactId) {
            Contact *contact = m_contacts.takeAt(i);
            emit contactRemoved(contactId);
            cleanupContact(contact);
            return true;
        }
    }
    return false;
}

Contact *ContactManager::getContact(const QString &contactId) const
{
    for (Contact *contact : m_contacts) {
        if (contact->id() == contactId) {
            return contact;
        }
    }
    return nullptr;
}

QList<Contact *> ContactManager::getAllContacts() const
{
    return m_contacts;
}

QList<Contact *> ContactManager::getContactsSortedByLastSeen(bool descending) const
{
    QList<Contact *> sorted = m_contacts;
    
    std::sort(sorted.begin(), sorted.end(), [descending](Contact *a, Contact *b) {
        if (!a->lastSeen().isValid()) {
            return descending ? false : true;
        }
        if (!b->lastSeen().isValid()) {
            return descending ? true : false;
        }
        if (descending) {
            return a->lastSeen() > b->lastSeen();
        }
        return a->lastSeen() < b->lastSeen();
    });
    
    return sorted;
}

QList<Contact *> ContactManager::getContactsSortedByName(bool ascending) const
{
    QList<Contact *> sorted = m_contacts;
    
    std::sort(sorted.begin(), sorted.end(), [ascending](Contact *a, Contact *b) {
        QString aName = a->getDisplayName();
        QString bName = b->getDisplayName();
        if (ascending) {
            return aName < bName;
        }
        return aName > bName;
    });
    
    return sorted;
}

QList<Contact *> ContactManager::getFavoriteContacts() const
{
    QList<Contact *> favorites;
    for (Contact *contact : m_contacts) {
        if (contact->isFavorite()) {
            favorites.append(contact);
        }
    }
    return favorites;
}

QList<Contact *> ContactManager::getOnlineContacts(int thresholdMinutes) const
{
    QList<Contact *> online;
    for (Contact *contact : m_contacts) {
        if (contact->isOnline(thresholdMinutes)) {
            online.append(contact);
        }
    }
    return online;
}

QList<Contact *> ContactManager::searchContacts(const QString &query) const
{
    QList<Contact *> results;
    QString lowerQuery = query.toLower();
    
    for (Contact *contact : m_contacts) {
        if (contact->id().contains(lowerQuery, Qt::CaseInsensitive) ||
            contact->name().contains(lowerQuery, Qt::CaseInsensitive) ||
            contact->longName().contains(lowerQuery, Qt::CaseInsensitive) ||
            contact->nodeId().contains(lowerQuery, Qt::CaseInsensitive)) {
            results.append(contact);
        }
    }
    
    return results;
}

bool ContactManager::updateContact(const QString &contactId, const QVariantMap &data)
{
    Contact *contact = getContact(contactId);
    if (!contact) {
        return false;
    }
    
    contact->fromVariantMap(data);
    emit contactUpdated(contact);
    return true;
}

bool ContactManager::updateContactPosition(const QString &contactId, double latitude, 
                                          double longitude, double elevation)
{
    Contact *contact = getContact(contactId);
    if (!contact) {
        return false;
    }
    
    contact->setLatitude(latitude);
    contact->setLongitude(longitude);
    contact->setElevation(elevation);
    contact->setPositionTimestamp(QDateTime::currentDateTime());
    emit contactUpdated(contact);
    return true;
}

bool ContactManager::updateContactLastSeen(const QString &contactId, const QDateTime &timestamp)
{
    Contact *contact = getContact(contactId);
    if (!contact) {
        return false;
    }
    
    contact->setLastSeen(timestamp);
    emit contactUpdated(contact);
    return true;
}

bool ContactManager::setContactFavorite(const QString &contactId, bool favorite)
{
    Contact *contact = getContact(contactId);
    if (!contact) {
        return false;
    }
    
    contact->setFavorite(favorite);
    emit contactUpdated(contact);
    return true;
}

bool ContactManager::setContactMuted(const QString &contactId, bool muted)
{
    Contact *contact = getContact(contactId);
    if (!contact) {
        return false;
    }
    
    contact->setMuted(muted);
    emit contactUpdated(contact);
    return true;
}

QList<Contact *> ContactManager::discoverContacts()
{
    // Stub: In a real implementation, discover contacts from the mesh network
    QList<Contact *> discovered;
    
    // Create some sample discovered contacts
    for (int i = 0; i < 5; ++i) {
        Contact *contact = new Contact(this);
        contact->setId(QString("discovered_%1").arg(i));
        contact->setName(QString("Discovered Node %1").arg(i));
        contact->setNodeId(QString("!%1%2").arg(char('A' + i)).arg(char('0' + i)));
        contact->setLastSeen(QDateTime::currentDateTime().addSecs(-i * 60));
        contact->setPathHashSize(2);
        discovered.append(contact);
    }
    
    // Cache discovered contacts
    qDeleteAll(m_discoveredContacts);
    m_discoveredContacts = discovered;
    
    emit contactsDiscovered(discovered);
    return discovered;
}

int ContactManager::importContacts(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open contacts file");
        return 0;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit errorOccurred("JSON parse error: " + error.errorString());
        return 0;
    }
    
    int count = 0;
    if (doc.isArray()) {
        QJsonArray contactsArray = doc.array();
        for (const QJsonValue &value : contactsArray) {
            if (value.isObject()) {
                QVariantMap contactData = value.toObject().toVariantMap();
                Contact *contact = addContactFromVariantMap(contactData);
                if (contact) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

bool ContactManager::exportContacts(const QString &filePath, const QList<Contact *> &contacts)
{
    const QList<Contact *> &exportContacts = contacts.isEmpty() ? m_contacts : contacts;
    
    QJsonArray contactsArray;
    for (Contact *contact : exportContacts) {
        contactsArray.append(QJsonObject::fromVariantMap(contact->toVariantMap()));
    }
    
    QJsonDocument doc(contactsArray);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    
    return false;
}

int ContactManager::getContactCount() const
{
    return m_contacts.size();
}

QList<Contact *> ContactManager::getContactsWithUnreadMessages() const
{
    // Stub: In a real implementation, check for unread messages
    QList<Contact *> contactsWithUnread;
    for (Contact *contact : m_contacts) {
        // For now, mark some contacts as having unread messages
        if (contact->id().endsWith("2") || contact->id().endsWith("4")) {
            contactsWithUnread.append(contact);
        }
    }
    return contactsWithUnread;
}

bool ContactManager::contactExists(const QString &contactId) const
{
    return getContact(contactId) != nullptr;
}

int ContactManager::clearAllContacts()
{
    int count = m_contacts.size();
    qDeleteAll(m_contacts);
    m_contacts.clear();
    return count;
}

QList<Contact *> ContactManager::getContactsByRegion(const QString &regionName) const
{
    QList<Contact *> contactsInRegion;
    for (Contact *contact : m_contacts) {
        if (contact->regions().contains(regionName)) {
            contactsInRegion.append(contact);
        }
    }
    return contactsInRegion;
}

bool ContactManager::addDiscoveredContact(Contact *contact)
{
    if (!contact) {
        return false;
    }
    
    // Check if already in discovered contacts
    for (Contact *existing : m_discoveredContacts) {
        if (existing->id() == contact->id() || existing->nodeId() == contact->nodeId()) {
            return false;
        }
    }
    
    m_discoveredContacts.append(contact);
    return true;
}

QList<Contact *> ContactManager::getDiscoveredContacts() const
{
    return m_discoveredContacts;
}

bool ContactManager::removeDiscoveredContact(const QString &contactId)
{
    for (int i = 0; i < m_discoveredContacts.size(); ++i) {
        if (m_discoveredContacts[i]->id() == contactId) {
            Contact *contact = m_discoveredContacts.takeAt(i);
            cleanupContact(contact);
            return true;
        }
    }
    return false;
}

void ContactManager::cleanupContact(Contact *contact)
{
    if (contact) {
        contact->deleteLater();
    }
}

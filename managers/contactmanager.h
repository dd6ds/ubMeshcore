#ifndef CONTACTMANAGER_H
#define CONTACTMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantMap>
#include "models/contact.h"

/**
 * @brief Manages contacts for MeshCore
 *
 * Features:
 * - Add, remove, and update contacts
 * - Discover contacts via mesh network
 * - Search and filter contacts
 * - Import/export contacts from JSON
 * - Track contact online/offline status
 * - Manage contact favorites and mute status
 */
class ContactManager : public QObject
{
    Q_OBJECT

public:
    explicit ContactManager(QObject *parent = nullptr);
    ~ContactManager() override;

    /**
     * @brief Initialize the contact manager
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Add a new contact
     * @param contact Contact to add
     * @return true if contact was added successfully
     */
    bool addContact(Contact *contact);

    /**
     * @brief Add a new contact from variant map
     * @param contactData Contact data as QVariantMap
     * @return The created contact, or nullptr on failure
     */
    Contact *addContactFromVariantMap(const QVariantMap &contactData);

    /**
     * @brief Remove a contact by ID
     * @param contactId ID of the contact to remove
     * @return true if contact was removed
     */
    bool removeContact(const QString &contactId);

    /**
     * @brief Get a contact by ID
     * @param contactId ID of the contact
     * @return Contact pointer, or nullptr if not found
     */
    Contact *getContact(const QString &contactId) const;

    /**
     * @brief Get all contacts
     * @return List of all contacts
     */
    QList<Contact *> getAllContacts() const;

    /**
     * @brief Get contacts sorted by last seen
     * @param descending Sort in descending order (most recent first)
     * @return List of contacts sorted by last seen timestamp
     */
    QList<Contact *> getContactsSortedByLastSeen(bool descending = true) const;

    /**
     * @brief Get contacts sorted by name
     * @param ascending Sort in ascending alphabetical order
     * @return List of contacts sorted by name
     */
    QList<Contact *> getContactsSortedByName(bool ascending = true) const;

    /**
     * @brief Get favorite contacts
     * @return List of favorite contacts
     */
    QList<Contact *> getFavoriteContacts() const;

    /**
     * @brief Get online contacts (recently seen)
     * @param thresholdMinutes Maximum minutes to consider online
     * @return List of online contacts
     */
    QList<Contact *> getOnlineContacts(int thresholdMinutes = 15) const;

    /**
     * @brief Search contacts by name or ID
     * @param query Search query
     * @return List of matching contacts
     */
    QList<Contact *> searchContacts(const QString &query) const;

    /**
     * @brief Update a contact
     * @param contactId ID of the contact to update
     * @param data Updated contact data
     * @return true if contact was updated
     */
    bool updateContact(const QString &contactId, const QVariantMap &data);

    /**
     * @brief Update a contact's position
     * @param contactId ID of the contact
     * @param latitude Latitude
     * @param longitude Longitude
     * @param elevation Elevation in meters
     * @return true if contact was updated
     */
    bool updateContactPosition(const QString &contactId, double latitude, double longitude,
                              double elevation = 0.0);

    /**
     * @brief Update a contact's last seen timestamp
     * @param contactId ID of the contact
     * @param timestamp Timestamp
     * @return true if contact was updated
     */
    bool updateContactLastSeen(const QString &contactId, const QDateTime &timestamp);

    /**
     * @brief Mark a contact as favorite
     * @param contactId ID of the contact
     * @param favorite Whether to mark as favorite
     * @return true if contact was updated
     */
    bool setContactFavorite(const QString &contactId, bool favorite);

    /**
     * @brief Mark a contact as muted
     * @param contactId ID of the contact
     * @param muted Whether to mark as muted
     * @return true if contact was updated
     */
    bool setContactMuted(const QString &contactId, bool muted);

    /**
     * @brief Discover contacts from the mesh network
     * @return List of discovered contacts
     */
    QList<Contact *> discoverContacts();

    /**
     * @brief Import contacts from JSON
     * @param filePath Path to the JSON file
     * @return Number of contacts imported
     */
    int importContacts(const QString &filePath);

    /**
     * @brief Export contacts to JSON
     * @param filePath Path to save the JSON file
     * @param contacts List of contacts to export (empty for all)
     * @return true if export was successful
     */
    bool exportContacts(const QString &filePath, const QList<Contact *> &contacts = {});

    /**
     * @brief Get the number of contacts
     * @return Number of contacts
     */
    int getContactCount() const;

    /**
     * @brief Get contacts with unread messages
     * @return List of contacts with unread messages
     */
    QList<Contact *> getContactsWithUnreadMessages() const;

    /**
     * @brief Check if a contact exists
     * @param contactId ID of the contact
     * @return true if contact exists
     */
    bool contactExists(const QString &contactId) const;

    /**
     * @brief Clear all contacts
     * @return Number of contacts cleared
     */
    int clearAllContacts();

    /**
     * @brief Get contacts by region
     * @param regionName Name of the region
     * @return List of contacts in the region
     */
    QList<Contact *> getContactsByRegion(const QString &regionName) const;

    /**
     * @brief Add a contact to discovered contacts database
     * @param contact Contact to add
     * @return true if contact was added
     */
    bool addDiscoveredContact(Contact *contact);

    /**
     * @brief Get discovered contacts
     * @return List of discovered contacts
     */
    QList<Contact *> getDiscoveredContacts() const;

    /**
     * @brief Remove a discovered contact
     * @param contactId ID of the discovered contact to remove
     * @return true if contact was removed
     */
    bool removeDiscoveredContact(const QString &contactId);

signals:
    /**
     * @brief Emitted when a contact is added
     */
    void contactAdded(Contact *contact);

    /**
     * @brief Emitted when a contact is removed
     */
    void contactRemoved(const QString &contactId);

    /**
     * @brief Emitted when a contact is updated
     */
    void contactUpdated(Contact *contact);

    /**
     * @brief Emitted when contacts are discovered
     */
    void contactsDiscovered(const QList<Contact *> &contacts);

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString &errorMessage);

private:
    QList<Contact *> m_contacts;
    QList<Contact *> m_discoveredContacts;
    
    // Helper methods
    void cleanupContact(Contact *contact);
};

#endif // CONTACTMANAGER_H

#ifndef MESSAGESTORE_H
#define MESSAGESTORE_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief Stores and manages messages for MeshCore
 *
 * Features:
 * - Store and retrieve direct messages
 * - Store and retrieve channel messages
 * - Message search functionality
 * - Message retention and auto-delete
 * - Unread message tracking
 */
class MessageStore : public QObject
{
    Q_OBJECT

public:
    explicit MessageStore(QObject *parent = nullptr);
    ~MessageStore() override;

    /**
     * @brief Initialize the message database
     * @param dbPath Path to the SQLite database file
     * @return true if initialization was successful
     */
    bool initialize(const QString &dbPath);

    /**
     * @brief Store a direct message
     * @param senderId ID of the sender
     * @param receiverId ID of the receiver
     * @param messageContent Content of the message
     * @param timestamp Timestamp when the message was received
     * @param isRead Whether the message has been read
     * @param isOutgoing Whether the message is outgoing (sent by this node)
     * @return ID of the stored message, or -1 on failure
     */
    qint64 storeDirectMessage(const QString &senderId, const QString &receiverId,
                             const QString &messageContent, const QDateTime &timestamp,
                             bool isRead = false, bool isOutgoing = false);

    /**
     * @brief Store a channel message
     * @param channelId ID of the channel
     * @param senderId ID of the sender
     * @param messageContent Content of the message
     * @param timestamp Timestamp when the message was received
     * @param regionScopes Region scopes applied to the message
     * @return ID of the stored message, or -1 on failure
     */
    qint64 storeChannelMessage(const QString &channelId, const QString &senderId,
                              const QString &messageContent, const QDateTime &timestamp,
                              const QStringList &regionScopes = {});

    /**
     * @brief Retrieve messages for a direct conversation
     * @param contactId ID of the contact
     * @param limit Maximum number of messages to retrieve
     * @param offset Offset for pagination
     * @param sortOrder Sort order ("ASC" or "DESC")
     * @return List of messages (as QVariantMap)
     */
    QList<QVariantMap> getDirectMessages(const QString &contactId, int limit = 50,
                                        int offset = 0, const QString &sortOrder = "DESC");

    /**
     * @brief Retrieve messages for a channel
     * @param channelId ID of the channel
     * @param limit Maximum number of messages to retrieve
     * @param offset Offset for pagination
     * @param sortOrder Sort order ("ASC" or "DESC")
     * @return List of messages (as QVariantMap)
     */
    QList<QVariantMap> getChannelMessages(const QString &channelId, int limit = 50,
                                          int offset = 0, const QString &sortOrder = "DESC");

    /**
     * @brief Mark messages as read
     * @param messageIds List of message IDs to mark as read
     * @return Number of messages marked as read
     */
    int markMessagesAsRead(const QList<qint64> &messageIds);

    /**
     * @brief Mark all messages in a conversation as read
     * @param contactId ID of the contact (for DMs) or channelId (for channels)
     * @param isChannel Whether this is a channel or direct message
     * @return Number of messages marked as read
     */
    int markConversationAsRead(const QString &id, bool isChannel = false);

    /**
     * @brief Search messages by content
     * @param query Search query string
     * @param isChannel Search in channels or direct messages
     * @param channelId Optional: specific channel to search in
     * @param limit Maximum number of results
     * @return List of matching messages
     */
    QList<QVariantMap> searchMessages(const QString &query, bool isChannel = false,
                                    const QString &channelId = {}, int limit = 20);

    /**
     * @brief Get unread message count for a conversation
     * @param id Contact ID (for DMs) or channel ID (for channels)
     * @param isChannel Whether this is a channel or direct message
     * @return Number of unread messages
     */
    int getUnreadCount(const QString &id, bool isChannel = false);

    /**
     * @brief Get total unread message count across all conversations
     * @return Total number of unread messages
     */
    int getTotalUnreadCount();

    /**
     * @brief Delete messages older than a certain timestamp
     * @param timestamp Cutoff timestamp
     * @param isChannel Delete from channels or direct messages
     * @param channelId Optional: specific channel to clean up
     * @return Number of messages deleted
     */
    int deleteMessagesOlderThan(const QDateTime &timestamp, bool isChannel = false,
                               const QString &channelId = {});

    /**
     * @brief Delete a specific message
     * @param messageId ID of the message to delete
     * @return true if deletion was successful
     */
    bool deleteMessage(qint64 messageId);

    /**
     * @brief Get the oldest unread message ID in a conversation
     * @param id Contact ID (for DMs) or channel ID (for channels)
     * @param isChannel Whether this is a channel or direct message
     * @return ID of the oldest unread message, or -1 if all messages are read
     */
    qint64 getOldestUnreadMessageId(const QString &id, bool isChannel = false);

    /**
     * @brief Get message by ID
     * @param messageId ID of the message to retrieve
     * @return Message data as QVariantMap, or empty map if not found
     */
    QVariantMap getMessage(qint64 messageId);

    /**
     * @brief Export all messages to a JSON file
     * @param filePath Path to save the JSON file
     * @return true if export was successful
     */
    bool exportMessages(const QString &filePath);

signals:
    /**
     * @brief Emitted when a new message is stored
     */
    void messageStored(qint64 messageId, bool isChannel);

    /**
     * @brief Emitted when messages are deleted
     */
    void messagesDeleted(int count);

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString &errorMessage);

private:
    QString m_dbPath;
    // Database connection would be stored here in a real implementation
    
    // Helper methods
    bool openDatabase();
    bool createTables();
};

#endif // MESSAGESTORE_H

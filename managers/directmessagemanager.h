#ifndef DIRECTMESSAGEMANAGER_H
#define DIRECTMESSAGEMANAGER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QList>

/**
 * @brief Manages direct messages between contacts for MeshCore
 *
 * Features:
 * - Send and receive direct messages
 * - Message queuing and retry logic
 * - Message acknowledgment
 * - Message search
 * - Typing indicators
 * - Read receipts
 */
class DirectMessageManager : public QObject
{
    Q_OBJECT

public:
    explicit DirectMessageManager(QObject *parent = nullptr);
    ~DirectMessageManager() override;

    /**
     * @brief Initialize the direct message manager
     * @return true if initialization was successful
     */
    bool initialize();

    /**
     * @brief Send a direct message to a contact
     * @param contactId ID of the recipient contact
     * @param message Content of the message
     * @param regionScopes Optional region scopes for the message
     * @return Message ID or -1 on failure
     */
    qint64 sendMessage(const QString &contactId, const QString &message,
                      const QStringList &regionScopes = {});

    /**
     * @brief Receive a direct message
     * @param senderId ID of the sender
     * @param message Content of the message
     * @param timestamp When the message was sent/received
     * @param regionScopes Region scopes applied to the message
     * @return Message ID of the received message
     */
    qint64 receiveMessage(const QString &senderId, const QString &message,
                          const QDateTime &timestamp, const QStringList &regionScopes = {});

    /**
     * @brief Get messages for a specific contact (conversation)
     * @param contactId ID of the contact
     * @param limit Maximum number of messages to retrieve
     * @param offset Offset for pagination
     * @param sortOrder Sort order ("ASC" or "DESC")
     * @return List of messages
     */
    QList<QVariantMap> getMessages(const QString &contactId, int limit = 50,
                                   int offset = 0, const QString &sortOrder = "DESC");

    /**
     * @brief Get the latest message for a contact
     * @param contactId ID of the contact
     * @return Latest message or empty map if none
     */
    QVariantMap getLatestMessage(const QString &contactId);

    /**
     * @brief Mark messages as read
     * @param messageIds List of message IDs to mark as read
     * @return Number of messages marked as read
     */
    int markMessagesAsRead(const QList<qint64> &messageIds);

    /**
     * @brief Mark all messages in a conversation as read
     * @param contactId ID of the contact
     * @return Number of messages marked as read
     */
    int markConversationAsRead(const QString &contactId);

    /**
     * @brief Get unread message count for a conversation
     * @param contactId ID of the contact
     * @return Number of unread messages
     */
    int getUnreadCount(const QString &contactId);

    /**
     * @brief Delete a message
     * @param messageId ID of the message to delete
     * @param isOutgoing Whether the message is outgoing (local) or incoming
     * @return true if message was deleted
     */
    bool deleteMessage(qint64 messageId, bool isOutgoing = false);

    /**
     * @brief Search messages by content
     * @param query Search query
     * @param contactId Optional: specific contact to search in
     * @param limit Maximum number of results
     * @return List of matching messages
     */
    QList<QVariantMap> searchMessages(const QString &query, const QString &contactId = {}, int limit = 20);

    /**
     * @brief Get conversations with contacts
     * @param limit Maximum number of conversations
     * @param offset Offset for pagination
     * @return List of conversations (contact info + last message)
     */
    QList<QVariantMap> getConversations(int limit = 20, int offset = 0);

    /**
     * @brief Get conversations sorted by last message time
     * @param descending Most recent first
     * @return List of conversations
     */
    QList<QVariantMap> getConversationsSortedByLastMessage(bool descending = true);

    /**
     * @brief Check if a conversation has unread messages
     * @param contactId ID of the contact
     * @return true if there are unread messages
     */
    bool hasUnreadMessages(const QString &contactId);

    /**
     * @brief Send typing indicator
     * @param contactId ID of the contact
     * @param isTyping Whether the user is typing
     */
    void sendTypingIndicator(const QString &contactId, bool isTyping);

    /**
     * @brief Send read receipt
     * @param contactId ID of the contact
     * @param messageId ID of the message that was read
     */
    void sendReadReceipt(const QString &contactId, qint64 messageId);

    /**
     * @brief Queue a message for retry
     * @param messageId ID of the message to retry
     * @param retryDelay Delay in milliseconds before retry
     */
    void retryMessage(qint64 messageId, int retryDelay = 5000);

    /**
     * @brief Cancel a message retry
     * @param messageId ID of the message
     */
    void cancelRetry(qint64 messageId);

    /**
     * @brief Get message by ID
     * @param messageId ID of the message
     * @return Message data or empty map if not found
     */
    QVariantMap getMessage(qint64 messageId);

    /**
     * @brief Get total message count for a conversation
     * @param contactId ID of the contact
     * @return Number of messages
     */
    int getMessageCount(const QString &contactId);

    /**
     * @brief Get statistics for a conversation
     * @param contactId ID of the contact
     * @return Conversation statistics
     */
    QVariantMap getConversationStats(const QString &contactId);

    /**
     * @brief Get message delivery status
     * @param messageId ID of the message
     * @return Delivery status ("sent", "delivered", "read", "failed")
     */
    QString getMessageStatus(qint64 messageId);

signals:
    /**
     * @brief Emitted when a message is sent
     */
    void messageSent(qint64 messageId, const QString &contactId, const QString &message);

    /**
     * @brief Emitted when a message is received
     */
    void messageReceived(qint64 messageId, const QString &senderId, const QString &message);

    /**
     * @brief Emitted when a message is delivered
     */
    void messageDelivered(qint64 messageId, const QString &contactId);

    /**
     * @brief Emitted when a message is read
     */
    void messageRead(qint64 messageId, const QString &contactId);

    /**
     * @brief Emitted when a message fails to send
     */
    void messageFailed(qint64 messageId, const QString &contactId, const QString &error);

    /**
     * @brief Emitted when typing indicator is received
     */
    void typingIndicatorReceived(const QString &contactId, bool isTyping);

    /**
     * @brief Emitted when an error occurs
     */
    void errorOccurred(const QString &errorMessage);

private:
    // Internal data structures
    // In a real implementation, this would use a database or persistent storage
    
    struct Message {
        qint64 id;
        QString contactId;
        QString senderId;
        QString content;
        QDateTime timestamp;
        bool isOutgoing;
        bool isRead;
        bool isDelivered;
        bool isFailed;
        QStringList regionScopes;
        QString status;
    };
    
    QList<Message> m_messages;
    QMap<QString, QList<qint64>> m_conversationMessages; // contactId -> list of message IDs
    QMap<qint64, Message> m_messageMap; // messageId -> Message
    
    // Helper methods
    Message *findMessage(qint64 messageId);
    qint64 generateMessageId();
};

#endif // DIRECTMESSAGEMANAGER_H

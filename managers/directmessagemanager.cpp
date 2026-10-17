#include "directmessagemanager.h"
#include <QDebug>
#include <QUuid>

DirectMessageManager::DirectMessageManager(QObject *parent)
    : QObject(parent)
{
    qDebug() << "DirectMessageManager initialized";
}

DirectMessageManager::~DirectMessageManager()
{
    qDebug() << "DirectMessageManager destroyed";
}

bool DirectMessageManager::initialize()
{
    qDebug() << "DirectMessageManager initialized";
    return true;
}

qint64 DirectMessageManager::sendMessage(const QString &contactId, const QString &message,
                                        const QStringList &regionScopes)
{
    qint64 messageId = generateMessageId();
    
    Message msg;
    msg.id = messageId;
    msg.contactId = contactId;
    msg.senderId = "self"; // This would be the local node ID in a real implementation
    msg.content = message;
    msg.timestamp = QDateTime::currentDateTime();
    msg.isOutgoing = true;
    msg.isRead = true; // Outgoing messages are always "read" locally
    msg.isDelivered = false;
    msg.isFailed = false;
    msg.regionScopes = regionScopes;
    msg.status = "sent";
    
    m_messages.append(msg);
    m_messageMap[messageId] = msg;
    m_conversationMessages[contactId].append(messageId);
    
    emit messageSent(messageId, contactId, message);
    
    // Simulate delivery after a delay
    QMetaObject::invokeMethod(this, [this, messageId, contactId]() {
        if (m_messageMap.contains(messageId)) {
            m_messageMap[messageId].isDelivered = true;
            m_messageMap[messageId].status = "delivered";
            emit messageDelivered(messageId, contactId);
        }
    }, Qt::QueuedConnection, Q_ARG(int, 1000));
    
    return messageId;
}

qint64 DirectMessageManager::receiveMessage(const QString &senderId, const QString &message,
                                          const QDateTime &timestamp, const QStringList &regionScopes)
{
    qint64 messageId = generateMessageId();
    
    Message msg;
    msg.id = messageId;
    msg.contactId = senderId;
    msg.senderId = senderId;
    msg.content = message;
    msg.timestamp = timestamp;
    msg.isOutgoing = false;
    msg.isRead = false;
    msg.isDelivered = true;
    msg.isFailed = false;
    msg.regionScopes = regionScopes;
    msg.status = "received";
    
    m_messages.append(msg);
    m_messageMap[messageId] = msg;
    m_conversationMessages[senderId].append(messageId);
    
    emit messageReceived(messageId, senderId, message);
    
    return messageId;
}

QList<QVariantMap> DirectMessageManager::getMessages(const QString &contactId, int limit,
                                                    int offset, const QString &sortOrder)
{
    QList<QVariantMap> result;
    
    if (!m_conversationMessages.contains(contactId)) {
        return result;
    }
    
    const QList<qint64> &messageIds = m_conversationMessages[contactId];
    
    // Apply offset and limit
    int end = qMin(offset + limit, messageIds.size());
    for (int i = offset; i < end; ++i) {
        qint64 msgId = messageIds[i];
        if (m_messageMap.contains(msgId)) {
            const Message &msg = m_messageMap[msgId];
            QVariantMap msgMap;
            msgMap["id"] = msg.id;
            msgMap["contactId"] = msg.contactId;
            msgMap["senderId"] = msg.senderId;
            msgMap["content"] = msg.content;
            msgMap["timestamp"] = msg.timestamp.toString(Qt::ISODate);
            msgMap["isOutgoing"] = msg.isOutgoing;
            msgMap["isRead"] = msg.isRead;
            msgMap["isDelivered"] = msg.isDelivered;
            msgMap["regionScopes"] = msg.regionScopes;
            msgMap["status"] = msg.status;
            result.append(msgMap);
        }
    }
    
    // Sort if needed (default is DESC - newest first)
    if (sortOrder == "ASC") {
        std::sort(result.begin(), result.end(), [](const QVariantMap &a, const QVariantMap &b) {
            return a.value("timestamp").toDateTime() < b.value("timestamp").toDateTime();
        });
    } else {
        std::sort(result.begin(), result.end(), [](const QVariantMap &a, const QVariantMap &b) {
            return a.value("timestamp").toDateTime() > b.value("timestamp").toDateTime();
        });
    }
    
    return result;
}

QVariantMap DirectMessageManager::getLatestMessage(const QString &contactId)
{
    if (!m_conversationMessages.contains(contactId) || 
        m_conversationMessages[contactId].isEmpty()) {
        return QVariantMap();
    }
    
    qint64 latestId = m_conversationMessages[contactId].last();
    return getMessage(latestId);
}

int DirectMessageManager::markMessagesAsRead(const QList<qint64> &messageIds)
{
    int count = 0;
    for (qint64 msgId : messageIds) {
        if (m_messageMap.contains(msgId) && !m_messageMap[msgId].isOutgoing) {
            m_messageMap[msgId].isRead = true;
            count++;
        }
    }
    return count;
}

int DirectMessageManager::markConversationAsRead(const QString &contactId)
{
    if (!m_conversationMessages.contains(contactId)) {
        return 0;
    }
    
    int count = 0;
    for (qint64 msgId : m_conversationMessages[contactId]) {
        if (m_messageMap.contains(msgId) && !m_messageMap[msgId].isOutgoing) {
            m_messageMap[msgId].isRead = true;
            count++;
        }
    }
    
    return count;
}

int DirectMessageManager::getUnreadCount(const QString &contactId)
{
    if (!m_conversationMessages.contains(contactId)) {
        return 0;
    }
    
    int count = 0;
    for (qint64 msgId : m_conversationMessages[contactId]) {
        if (m_messageMap.contains(msgId) && 
            !m_messageMap[msgId].isOutgoing && 
            !m_messageMap[msgId].isRead) {
            count++;
        }
    }
    return count;
}

bool DirectMessageManager::deleteMessage(qint64 messageId, bool isOutgoing)
{
    if (!m_messageMap.contains(messageId)) {
        return false;
    }
    
    Message &msg = m_messageMap[messageId];
    
    // Remove from conversation
    if (m_conversationMessages.contains(msg.contactId)) {
        QList<qint64> &conversation = m_conversationMessages[msg.contactId];
        conversation.removeAll(messageId);
    }
    
    // Remove from messages list
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].id == messageId) {
            m_messages.removeAt(i);
            break;
        }
    }
    
    // Remove from map
    m_messageMap.remove(messageId);
    
    return true;
}

QList<QVariantMap> DirectMessageManager::searchMessages(const QString &query, 
                                                       const QString &contactId, int limit)
{
    QList<QVariantMap> results;
    QString lowerQuery = query.toLower();
    
    // Search all messages or specific conversation
    QList<qint64> messageIds;
    if (contactId.isEmpty()) {
        // Search all messages
        for (const Message &msg : m_messages) {
            messageIds.append(msg.id);
        }
    } else if (m_conversationMessages.contains(contactId)) {
        messageIds = m_conversationMessages[contactId];
    }
    
    for (qint64 msgId : messageIds) {
        if (m_messageMap.contains(msgId)) {
            const Message &msg = m_messageMap[msgId];
            if (msg.content.contains(lowerQuery, Qt::CaseInsensitive)) {
                results.append(getMessage(msgId));
                if (results.size() >= limit) {
                    break;
                }
            }
        }
    }
    
    return results;
}

QList<QVariantMap> DirectMessageManager::getConversations(int limit, int offset)
{
    QList<QVariantMap> conversations;
    
    for (auto it = m_conversationMessages.constBegin(); it != m_conversationMessages.constEnd(); ++it) {
        const QString &contactId = it.key();
        const QList<qint64> &msgIds = it.value();
        
        if (!msgIds.isEmpty()) {
            QVariantMap conversation;
            conversation["contactId"] = contactId;
            conversation["messageCount"] = msgIds.size();
            conversation["unreadCount"] = getUnreadCount(contactId);
            conversation["lastMessage"] = getLatestMessage(contactId);
            conversations.append(conversation);
        }
    }
    
    return conversations;
}

QList<QVariantMap> DirectMessageManager::getConversationsSortedByLastMessage(bool descending)
{
    QList<QVariantMap> conversations = getConversations();
    
    std::sort(conversations.begin(), conversations.end(), [descending](const QVariantMap &a, const QVariantMap &b) {
        QVariantMap lastA = a.value("lastMessage").toMap();
        QVariantMap lastB = b.value("lastMessage").toMap();
        QDateTime timeA = QDateTime::fromString(lastA.value("timestamp").toString(), Qt::ISODate);
        QDateTime timeB = QDateTime::fromString(lastB.value("timestamp").toString(), Qt::ISODate);
        
        if (descending) {
            return timeA > timeB;
        }
        return timeA < timeB;
    });
    
    return conversations;
}

bool DirectMessageManager::hasUnreadMessages(const QString &contactId)
{
    return getUnreadCount(contactId) > 0;
}

void DirectMessageManager::sendTypingIndicator(const QString &contactId, bool isTyping)
{
    qDebug() << "Sending typing indicator to" << contactId << ":" << isTyping;
    // In a real implementation, send typing indicator to the contact
}

void DirectMessageManager::sendReadReceipt(const QString &contactId, qint64 messageId)
{
    qDebug() << "Sending read receipt for message" << messageId << "to" << contactId;
    // In a real implementation, send read receipt
}

void DirectMessageManager::retryMessage(qint64 messageId, int retryDelay)
{
    if (!m_messageMap.contains(messageId)) {
        return;
    }
    
    Message &msg = m_messageMap[messageId];
    if (msg.status == "failed") {
        msg.status = "retrying";
        qDebug() << "Retrying message" << messageId;
        
        // Simulate retry
        QMetaObject::invokeMethod(this, [this, messageId, msg]() {
            if (m_messageMap.contains(messageId)) {
                // Randomly succeed or fail
                bool success = (qrand() % 2) == 0;
                if (success) {
                    m_messageMap[messageId].status = "delivered";
                    m_messageMap[messageId].isDelivered = true;
                    emit messageDelivered(messageId, msg.contactId);
                } else {
                    m_messageMap[messageId].status = "failed";
                    emit messageFailed(messageId, msg.contactId, "Retry failed");
                }
            }
        }, Qt::QueuedConnection, Q_ARG(int, retryDelay));
    }
}

void DirectMessageManager::cancelRetry(qint64 messageId)
{
    if (m_messageMap.contains(messageId)) {
        Message &msg = m_messageMap[messageId];
        if (msg.status == "retrying") {
            msg.status = "failed";
            qDebug() << "Cancelled retry for message" << messageId;
        }
    }
}

QVariantMap DirectMessageManager::getMessage(qint64 messageId)
{
    if (!m_messageMap.contains(messageId)) {
        return QVariantMap();
    }
    
    const Message &msg = m_messageMap[messageId];
    QVariantMap msgMap;
    msgMap["id"] = msg.id;
    msgMap["contactId"] = msg.contactId;
    msgMap["senderId"] = msg.senderId;
    msgMap["content"] = msg.content;
    msgMap["timestamp"] = msg.timestamp.toString(Qt::ISODate);
    msgMap["isOutgoing"] = msg.isOutgoing;
    msgMap["isRead"] = msg.isRead;
    msgMap["isDelivered"] = msg.isDelivered;
    msgMap["regionScopes"] = msg.regionScopes;
    msgMap["status"] = msg.status;
    return msgMap;
}

int DirectMessageManager::getMessageCount(const QString &contactId)
{
    if (!m_conversationMessages.contains(contactId)) {
        return 0;
    }
    return m_conversationMessages[contactId].size();
}

QVariantMap DirectMessageManager::getConversationStats(const QString &contactId)
{
    QVariantMap stats;
    stats["messageCount"] = getMessageCount(contactId);
    stats["unreadCount"] = getUnreadCount(contactId);
    stats["hasUnread"] = hasUnreadMessages(contactId);
    return stats;
}

QString DirectMessageManager::getMessageStatus(qint64 messageId)
{
    if (m_messageMap.contains(messageId)) {
        return m_messageMap[messageId].status;
    }
    return "unknown";
}

DirectMessageManager::Message *DirectMessageManager::findMessage(qint64 messageId)
{
    if (m_messageMap.contains(messageId)) {
        return &m_messageMap[messageId];
    }
    return nullptr;
}

qint64 DirectMessageManager::generateMessageId()
{
    static qint64 counter = 1;
    return counter++;
}

#ifndef MESSAGESTORE_H
#define MESSAGESTORE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

/// Persistent channel message history.
///
/// The node hands every message over exactly once (CMD_SYNC_NEXT_MESSAGE), so
/// whatever the app does not keep is gone after a restart - and Lomiri kills
/// suspended apps freely. Messages are appended as JSON lines to
/// ~/.local/share/ubmeshcore/channel_messages.jsonl (inside the app's
/// AppArmor-writable data dir) and reloaded on startup.
class MessageStore : public QObject
{
    Q_OBJECT
public:
    explicit MessageStore(QObject *parent = nullptr);

    /// All stored messages, oldest first. Each entry has channel, sender,
    /// text, time (and ts, epoch secs).
    Q_INVOKABLE QVariantList loadAll();
    /// Appends one message ({channel, sender, text, time}).
    Q_INVOKABLE void append(const QVariantMap &message);
    /// Removes the history of one channel (index), or everything if -1.
    Q_INVOKABLE void clear(int channel = -1);

    /// Last opened channel, restored on startup ({idx, name}; idx -1 if none).
    Q_INVOKABLE QVariantMap lastChannel() const;
    Q_INVOKABLE void setLastChannel(int idx, const QString &name);

private:
    void rewrite(const QVariantList &messages);
    QString m_path;
    static constexpr int kMaxMessages = 3000;
};

#endif // MESSAGESTORE_H

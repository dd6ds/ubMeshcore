#include "messagestore.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

MessageStore::MessageStore(QObject *parent)
    : QObject(parent)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/ubmeshcore");
    QDir().mkpath(dir);
    m_path = dir + QStringLiteral("/channel_messages.jsonl");
}

QVariantList MessageStore::loadAll()
{
    QVariantList out;
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;

    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject())
            out.append(doc.object().toVariantMap());
        // A torn last line (app killed mid-write) is simply skipped.
    }
    file.close();

    // Keep the file bounded: drop the oldest entries once over the cap.
    if (out.size() > kMaxMessages) {
        out = out.mid(out.size() - kMaxMessages);
        rewrite(out);
    }
    qDebug() << "MessageStore: loaded" << out.size() << "message(s) from" << m_path;
    return out;
}

void MessageStore::append(const QVariantMap &message)
{
    QVariantMap m = message;
    if (!m.contains(QStringLiteral("ts")))
        m.insert(QStringLiteral("ts"), QDateTime::currentSecsSinceEpoch());

    QFile file(m_path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "MessageStore: cannot write" << m_path << file.errorString();
        return;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(m)).toJson(QJsonDocument::Compact));
    file.write("\n");
}

void MessageStore::clear(int channel)
{
    if (channel < 0) {
        QFile::remove(m_path);
        return;
    }
    QVariantList kept;
    const QVariantList all = loadAll();
    for (const QVariant &v : all) {
        if (v.toMap().value(QStringLiteral("channel")).toInt() != channel)
            kept.append(v);
    }
    rewrite(kept);
}

QVariantMap MessageStore::lastChannel() const
{
    QSettings s;
    return QVariantMap{
        { QStringLiteral("idx"),  s.value(QStringLiteral("channels/lastIdx"), -1).toInt() },
        { QStringLiteral("name"), s.value(QStringLiteral("channels/lastName")).toString() }
    };
}

void MessageStore::setLastChannel(int idx, const QString &name)
{
    QSettings s;
    s.setValue(QStringLiteral("channels/lastIdx"), idx);
    s.setValue(QStringLiteral("channels/lastName"), name);
}

QVariantMap MessageStore::unreadCounts() const
{
    return QSettings().value(QStringLiteral("channels/unread")).toMap();
}

void MessageStore::setUnreadCounts(const QVariantMap &counts)
{
    QSettings().setValue(QStringLiteral("channels/unread"), counts);
}

void MessageStore::rewrite(const QVariantList &messages)
{
    // QSaveFile: write to a temp file and rename, so a crash cannot leave a
    // half-written history behind.
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    for (const QVariant &v : messages) {
        file.write(QJsonDocument(QJsonObject::fromVariantMap(v.toMap()))
                       .toJson(QJsonDocument::Compact));
        file.write("\n");
    }
    file.commit();
}

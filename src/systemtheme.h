// Whether the phone uses a dark theme ("SystemTheme" in QML). Lomiri keeps
// the theme in ~/.config/lomiri-ui-toolkit/theme.ini (e.g. SuruDark or
// Ambiance); the file is watched, so switching the theme in the system
// settings recolours the running app.

#ifndef SYSTEMTHEME_H
#define SYSTEMTHEME_H

#include <QFileSystemWatcher>
#include <QObject>

class SystemTheme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)

public:
    explicit SystemTheme(QObject *parent = nullptr);

    bool dark() const { return m_dark; }

signals:
    void darkChanged();

private:
    void read();

    QFileSystemWatcher m_watcher;
    QString m_path;
    bool m_dark = false;
};

#endif

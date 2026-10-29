// Lomiri theme detection, see systemtheme.h.

#include "systemtheme.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {

const QString kThemeFile = QStringLiteral("/.config/lomiri-ui-toolkit/theme.ini");
// QSettings maps an INI file's [General] section to top-level keys.
const QString kThemeKey = QStringLiteral("theme");

} // namespace

SystemTheme::SystemTheme(QObject *parent) : QObject(parent), m_path(QDir::homePath() + kThemeFile)
{
    // The settings app replaces the file, which drops a file watch; watching
    // the directory as well catches the new file.
    m_watcher.addPath(QFileInfo(m_path).absolutePath());
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &SystemTheme::read);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &SystemTheme::read);
    read();
}

void SystemTheme::read()
{
    if (QFileInfo::exists(m_path) && !m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
    const QString theme = QSettings(m_path, QSettings::IniFormat).value(kThemeKey).toString();
    const bool dark = theme.contains(QLatin1String("Dark"), Qt::CaseInsensitive);
    if (dark == m_dark)
        return;
    m_dark = dark;
    qDebug() << "SystemTheme:" << theme << (dark ? "(dark)" : "(light)");
    emit darkChanged();
}

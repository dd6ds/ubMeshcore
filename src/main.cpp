// Entry point: sets up logging, translations and the QML engine, and
// exposes the C++ backend (BluetoothManager, MessageStore) and the phone's
// light/dark theme (SystemTheme) to QML.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include "bluetoothmanager.h"
#include "messagestore.h"
#include "systemtheme.h"

// Confined click apps may only write below ~/.local/share/<package-name>,
// ~/.cache/<package-name> and ~/.config/<package-name>. /tmp and the bare
// ~/.local/share are denied by AppArmor, so the log lives in the app's dir.
static QString logFilePath() {
    static const QString path = [] {
        const QString dir = QStandardPaths::writableLocation(
                    QStandardPaths::GenericDataLocation) + QStringLiteral("/ubmeshcore");
        QDir().mkpath(dir);
        return dir + QStringLiteral("/ubmeshcore.log");
    }();
    return path;
}

void logMessage(const QString &message) {
    QFile logFile(logFilePath());
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
            << " " << message << "\n";
    }
    // Also goes to the journal: journalctl --user -f on the phone.
    qDebug().noquote() << message;
}

// Follows the language selected on the phone (Lomiri exports it as
// LANG/LANGUAGE, which QLocale::system() reads). App texts are embedded as
// :/i18n/ubmeshcore_<lang>.qm; English is the source language, so a missing
// translation simply leaves the English text.
static void installTranslators(QGuiApplication &app) {
    const QLocale locale = QLocale::system();
    QTranslator *appTranslator = new QTranslator(&app);
    if (appTranslator->load(locale, QStringLiteral("ubmeshcore"), QStringLiteral("_"),
                            QStringLiteral(":/i18n"))) {
        app.installTranslator(appTranslator);
        logMessage(QString("Language: %1 (translation loaded)").arg(locale.name()));
    } else {
        logMessage(QString("Language: %1 (no translation, using English)").arg(locale.name()));
    }
    // Qt's own texts (e.g. the Yes/Cancel dialog buttons), if the system has
    // them installed.
    QTranslator *qtTranslator = new QTranslator(&app);
    if (qtTranslator->load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                           QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        app.installTranslator(qtTranslator);
}

// Lomiri suspends background apps, which drops the BLE link. Reconnect to the
// last selected node as soon as the app is in the foreground again.
static void onAppStateChanged(BluetoothManager *bluetoothManager, Qt::ApplicationState state) {
    if (state != Qt::ApplicationActive)
        return;
    if (bluetoothManager->sessionReady()) {
        // Still connected, but "message waiting" pushes that arrived while
        // suspended were lost - fetch whatever the node cached meanwhile.
        logMessage("App resumed - fetching cached messages from node");
        bluetoothManager->syncNextMessage();
    } else if (!bluetoothManager->connected() && !bluetoothManager->connecting()
               && bluetoothManager->hasSelection()) {
        // Reconnecting starts a new session, which drains the queue too.
        logMessage("App resumed and link is down - reconnecting");
        bluetoothManager->connectToSelected();
    }
}

// Ubuntu Touch exports GRID_UNIT_PX (typically 18-27 on a phone, 8 on a
// desktop). All QML sizes are expressed as multiples of it so that text stays
// readable on a high-DPI phone screen.
static int gridUnitPx() {
    bool ok = false;
    const int px = qEnvironmentVariableIntValue("GRID_UNIT_PX", &ok);
    return (ok && px > 0) ? px : 8;
}

// Makes the C++ backend and the grid unit available to QML.
static BluetoothManager *exposeBackend(QQmlApplicationEngine &engine) {
    // Bluetooth is initialised lazily inside BluetoothManager - no
    // QBluetoothLocalDevice / D-Bus access at startup.
    // Owned by the engine, NOT a stack object: a stack object declared after
    // the engine is destroyed before it, and every QML binding then evaluates
    // against a null context property during shutdown.
    auto *bluetoothManager = new BluetoothManager(&engine);
    qmlRegisterUncreatableType<DeviceListModel>(
        "ubMeshcore", 1, 0, "DeviceListModel",
        QStringLiteral("Provided by BluetoothManager.devicesModel"));
    qmlRegisterUncreatableType<ContactListModel>(
        "ubMeshcore", 1, 0, "ContactListModel",
        QStringLiteral("Provided by BluetoothManager.contactsModel"));
    engine.rootContext()->setContextProperty("BluetoothManager", bluetoothManager);
    engine.rootContext()->setContextProperty("MessageStore", new MessageStore(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("SystemTheme"),
                                             new SystemTheme(&engine));
    const int gridUnit = gridUnitPx();
    engine.rootContext()->setContextProperty("gridUnitPx", gridUnit);
    logMessage(QString("GRID_UNIT_PX = %1").arg(gridUnit));
    return bluetoothManager;
}

int main(int argc, char *argv[]) {
    // Force the Default Controls 2 style. The platform style brings its own
    // palette, and controls without explicit colours then render dark text on
    // a dark background.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Default");

    // QT_QPA_PLATFORM, MIR_SOCKET and the Qt plugin paths are set by
    // lomiri-app-launch for the running shell. Do not override them here -
    // hard-coding them breaks the app on other OS versions/devices.

    QGuiApplication app(argc, argv);
    app.setApplicationName("ubMeshcore");
    app.setOrganizationName("ubmeshcore");
    logMessage("=== ubMeshcore starting ===");
    installTranslators(app);

    QQmlApplicationEngine engine;

    BluetoothManager *bluetoothManager = exposeBackend(engine);

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, bluetoothManager,
                     [bluetoothManager](Qt::ApplicationState state) {
                         onAppStateChanged(bluetoothManager, state);
                     });

    // Route QML load errors into our log too, not just stderr.
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &w : warnings)
            logMessage("QML: " + w.toString());
    });

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        logMessage("ERROR: Failed to load QML - no root objects");
        return -1;
    }

    logMessage("QML loaded, entering event loop");
    const int result = app.exec();
    logMessage(QString("App finished with code: %1").arg(result));
    return result;
}

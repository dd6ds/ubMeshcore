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

#include "bluetoothmanager.h"
#include "messagestore.h"

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

int main(int argc, char *argv[]) {
    // Force the Default Controls 2 style. Otherwise the platform style brings
    // its own palette, and controls that do not set explicit colours render
    // dark-on-dark - which is why typed text was invisible.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Default");

    // NOTE: QT_QPA_PLATFORM, MIR_SOCKET and the Qt plugin paths are set by
    // lomiri-app-launch for the running shell. Do not override them here -
    // hard-coding them breaks the app on other OS versions/devices.

    QGuiApplication app(argc, argv);
    app.setApplicationName("ubMeshcore");
    app.setOrganizationName("ubmeshcore");
    logMessage("=== ubMeshcore starting ===");

    QQmlApplicationEngine engine;

    // Expose the C++ Bluetooth backend to QML as "BluetoothManager".
    // Bluetooth is initialised lazily inside BluetoothManager - no
    // QBluetoothLocalDevice / D-Bus access at startup.
    // Owned by the engine, NOT a stack object: a stack object declared after
    // the engine is destroyed before it, and every QML binding then evaluates
    // against a null context property during shutdown.
    BluetoothManager *bluetoothManager = new BluetoothManager(&engine);
    qmlRegisterUncreatableType<DeviceListModel>(
        "ubMeshcore", 1, 0, "DeviceListModel",
        QStringLiteral("Provided by BluetoothManager.devicesModel"));
    qmlRegisterUncreatableType<ContactListModel>(
        "ubMeshcore", 1, 0, "ContactListModel",
        QStringLiteral("Provided by BluetoothManager.contactsModel"));
    engine.rootContext()->setContextProperty("BluetoothManager", bluetoothManager);
    engine.rootContext()->setContextProperty("MessageStore", new MessageStore(&engine));

    // Lomiri suspends background apps, which drops the BLE link. Reconnect to
    // the last selected node as soon as the app is in the foreground again.
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, bluetoothManager,
                     [bluetoothManager](Qt::ApplicationState state) {
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
    });

    // Ubuntu Touch exports GRID_UNIT_PX (typically 18-27 on a phone, 8 on a
    // desktop). All QML sizes are expressed as multiples of it so that text
    // stays readable on a high-DPI phone screen.
    bool gridUnitOk = false;
    int gridUnitPx = qEnvironmentVariableIntValue("GRID_UNIT_PX", &gridUnitOk);
    if (!gridUnitOk || gridUnitPx <= 0)
        gridUnitPx = 8;
    engine.rootContext()->setContextProperty("gridUnitPx", gridUnitPx);
    logMessage(QString("GRID_UNIT_PX = %1").arg(gridUnitPx));

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

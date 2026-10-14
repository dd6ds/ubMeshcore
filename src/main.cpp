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
#include <QBluetoothLocalDevice>
#include <cstdio>

#include "bluetoothmanager.h"

void logMessage(const QString &message) {
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/ubmeshcore.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") << " " << message << "\n";
        out.flush();
        logFile.close();
    }
    qDebug().noquote() << message;
}

int main(int argc, char *argv[]) {
    // Use plain C I/O BEFORE any Qt objects to log the absolute earliest point
    FILE *earlyLog = fopen("/tmp/ubmeshcore-early.log", "a");
    if (earlyLog) {
        fprintf(earlyLog, "%% MAIN STARTED - argc=%d\n", argc);
        fflush(earlyLog);
    }
    
    logMessage("=== ubMeshcore starting ===");
    
    // Create data directory if it doesn't exist
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    
    // Force the Default Controls 2 style. Otherwise the platform style brings
    // its own palette, and controls that do not set explicit colours render
    // dark-on-dark - which is why typed text was invisible.
    qputenv("QT_QUICK_CONTROLS_STYLE", "Default");

    // Set platform plugin for Ubuntu Touch
    qputenv("QT_QPA_PLATFORM", "ubuntumirclient");
    if (earlyLog) {
        fprintf(earlyLog, "%% Platform set to ubuntumirclient\n");
        fflush(earlyLog);
    }
    logMessage("Platform set to ubuntumirclient");
    
    // Set Qt plugin paths BEFORE creating QGuiApplication
    qputenv("QT_PLUGIN_PATH", "/usr/lib/aarch64-linux-gnu/qt5/plugins:/usr/lib/arm-linux-gnueabihf/qt5/plugins");
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", "/usr/lib/aarch64-linux-gnu/qt5/plugins/platforms:/usr/lib/arm-linux-gnueabihf/qt5/plugins/platforms");
    
    logMessage("Platform plugin paths set, about to create QGuiApplication");
    if (earlyLog) {
        fprintf(earlyLog, "%% About to create QGuiApplication\n");
        fflush(earlyLog);
    }
    
    QGuiApplication app(argc, argv);
    app.setApplicationName("ubMeshcore");
    app.setOrganizationName("ubmeshcore");
    logMessage("QGuiApplication created");
    if (earlyLog) {
        fprintf(earlyLog, "%% QGuiApplication created\n");
        fflush(earlyLog);
    }
    
    QQmlApplicationEngine engine;
    logMessage("QQmlApplicationEngine created");
    if (earlyLog) {
        fprintf(earlyLog, "%% QQmlApplicationEngine created\n");
        fflush(earlyLog);
    }
    
    // Initialize Bluetooth
    QBluetoothLocalDevice localDevice;
    if (localDevice.isValid()) {
        logMessage("Bluetooth: Local device " + localDevice.name() + " (" + localDevice.address().toString() + ")");
        if (localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
            logMessage("Bluetooth: adapter is powered off, powering on");
            localDevice.powerOn();
        }
    } else {
        logMessage("Bluetooth: Local device not available or no adapter found");
    }

    // Expose the C++ Bluetooth backend to QML as "BluetoothManager".
    // (The old QML-only BluetoothManager.qml singleton could not work: the QML
    //  QtBluetooth module of Qt 5.12 has no DeviceDiscoveryAgent type and no BLE
    //  scanning, and the file was never registered as a singleton.)
    // Owned by the engine, NOT a stack object: a stack object declared after
    // the engine is destroyed before it, and every QML binding then evaluates
    // against a null context property during shutdown ("TypeError: Cannot read
    // property 'uartReady' of null" and friends).
    BluetoothManager *bluetoothManager = new BluetoothManager(&engine);
    qmlRegisterUncreatableType<DeviceListModel>(
        "ubMeshcore", 1, 0, "DeviceListModel",
        QStringLiteral("Provided by BluetoothManager.devicesModel"));
    engine.rootContext()->setContextProperty("BluetoothManager", bluetoothManager);
    logMessage("BluetoothManager registered as QML context property");

    // Ubuntu Touch exports GRID_UNIT_PX (typically 18-27 on a phone, 8 on a
    // desktop). All QML sizes are expressed as multiples of it so that text
    // stays readable on a high-DPI phone screen.
    bool gridUnitOk = false;
    int gridUnitPx = qEnvironmentVariableIntValue("GRID_UNIT_PX", &gridUnitOk);
    if (!gridUnitOk || gridUnitPx <= 0)
        gridUnitPx = 8;
    engine.rootContext()->setContextProperty("gridUnitPx", gridUnitPx);
    logMessage(QString("GRID_UNIT_PX = %1").arg(gridUnitPx));
    
    logMessage("Loading QML from qrc:/qml/main.qml");
    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    
    if (engine.rootObjects().isEmpty()) {
        logMessage("ERROR: Failed to load QML - no root objects");
        qWarning() << "Failed to load QML";
        if (earlyLog) {
            fprintf(earlyLog, "%% ERROR: QML failed to load\n");
            fflush(earlyLog);
        }
        fclose(earlyLog);
        return -1;
    }
    
    logMessage("QML loaded successfully, starting app exec");
    if (earlyLog) {
        fprintf(earlyLog, "%% Starting app.exec()\n");
        fflush(earlyLog);
        fclose(earlyLog);
    }
    
    int result = app.exec();
    logMessage(QString("App finished with code: %1").arg(result));
    return result;
}

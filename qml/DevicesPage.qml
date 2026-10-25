import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// First page: scan for Bluetooth devices and pick the MeshCore node.
Page {
    id: page
    property var app                 // main.qml root (sizes + navigation)

    title: qsTr("Discover MeshCore Nodes")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: app.spacingUnit
        spacing: app.spacingUnit

        AppLabel {
            text: qsTr("Scan for Bluetooth devices")
            font.bold: true
            font.pixelSize: app.fontTitle
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        Button {
            text: BluetoothManager.scanActive ? qsTr("Stop Scan") : qsTr("Start Scan")
            enabled: BluetoothManager.adapterReady || BluetoothManager.scanActive
            font.pixelSize: app.fontBody
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.round(app.gu * 22)
            Layout.preferredHeight: Math.round(app.gu * 5)
            onClicked: {
                if (BluetoothManager.scanActive)
                    BluetoothManager.stopScan()
                else
                    BluetoothManager.startScan()
            }
        }

        AppLabel {
            text: BluetoothManager.status
            font.pixelSize: app.fontSecondary
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: "#666"
        }

        AppLabel {
            text: BluetoothManager.connected
                  ? qsTr("Connected to: %1").arg(BluetoothManager.connectedDevice)
                  : qsTr("%1 device(s) found").arg(BluetoothManager.devicesModel.count)
            font.pixelSize: app.fontSecondary
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            color: "#666"
        }

        ListView {
            id: deviceListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Math.round(app.gu * 0.75)
            model: BluetoothManager.devicesModel

            delegate: Rectangle {
                readonly property bool selected: BluetoothManager.selectedAddress === model.address

                width: deviceListView.width
                height: Math.round(app.gu * 8)
                radius: Math.round(app.gu * 0.5)
                color: rowMouse.pressed
                       ? "#b0d4ff"
                       : (model.isMeshCore ? "#e6f4ea" : "#f0f0f0")
                border.color: selected ? "#0078d7"
                                       : (model.isMeshCore ? "#2e7d32" : "#cccccc")
                border.width: selected ? 3 : 1

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: app.spacingUnit
                    spacing: Math.round(app.gu * 0.5)

                    AppLabel {
                        text: model.name
                        font.bold: model.hasName
                        font.italic: !model.hasName
                        font.pixelSize: app.fontBody
                        color: model.hasName ? "#000" : "#888"
                        elide: Text.ElideRight
                        width: parent.width
                    }
                    AppLabel {
                        text: model.address
                              + (model.lowEnergy ? "  ·  BLE" : "  ·  " + qsTr("Classic"))
                              + (model.rssi !== 0 ? "  ·  " + model.rssi + " dBm" : "")
                        color: "#555"
                        font.pixelSize: app.fontSecondary
                        elide: Text.ElideRight
                        width: parent.width
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    onClicked: {
                        BluetoothManager.selectDevice(model.address, model.name)
                        app.showPage(app.pageConnection)
                    }
                }
            }
        }
    }
}

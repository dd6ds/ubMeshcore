import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Second page: connect to the selected node, show its name, battery and the
// protocol trace, and lead on to channels and contacts.
Page {
    id: page
    property var app                 // main.qml root (sizes, navigation, unread counts)

    title: qsTr("Connect to Node")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: app.spacingUnit
        spacing: app.spacingUnit

        AppLabel {
            text: qsTr("Selected Node: %1")
                  .arg(BluetoothManager.hasSelection ? BluetoothManager.selectedName
                                                     : qsTr("None"))
            font.bold: true
            font.pixelSize: app.fontTitle
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        AppLabel {
            text: BluetoothManager.selectedAddress
            color: app.colorSecondaryText
            font.pixelSize: app.fontSecondary
            Layout.fillWidth: true
        }

        AppLabel {
            text: BluetoothManager.connectionState
            color: BluetoothManager.uartReady ? app.colorSuccess : app.colorSecondaryText
            font.pixelSize: app.fontBody
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        BusyIndicator {
            running: BluetoothManager.connecting
            visible: BluetoothManager.connecting
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            text: BluetoothManager.connected ? qsTr("Disconnect") : qsTr("Connect")
            enabled: BluetoothManager.hasSelection && !BluetoothManager.connecting
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.round(app.gu * 22)
            Layout.preferredHeight: Math.round(app.gu * 5)
            onClicked: {
                if (BluetoothManager.connected)
                    BluetoothManager.disconnect()
                else
                    BluetoothManager.connectToSelected()
            }
        }

        AppLabel {
            text: qsTr("Node: %1").arg(BluetoothManager.nodeName || qsTr("(unnamed)"))
            visible: BluetoothManager.sessionReady
            font.pixelSize: app.fontBody
            color: app.colorSuccess
            Layout.fillWidth: true
        }

        AppLabel {
            text: app.batteryText()
            visible: BluetoothManager.sessionReady && text.length > 0
            font.pixelSize: app.fontBody
            color: app.batteryColor()
            Layout.fillWidth: true
        }

        // Companion protocol trace - the quickest way to see where a
        // handshake stalls without digging through clickable logs.
        AppLabel {
            text: BluetoothManager.trafficLog.slice(-7).join("\n")
            visible: BluetoothManager.uartReady
            color: app.colorSecondaryText
            font.pixelSize: Math.round(app.gu * 1.0)
            font.family: "monospace"
            Layout.fillWidth: true
            wrapMode: Text.WrapAnywhere
            maximumLineCount: 7
            elide: Text.ElideRight
        }

        Item { Layout.fillHeight: true }

        Button {
            text: qsTr("Channels")
            enabled: BluetoothManager.uartReady
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.showPage(app.pageChannels)
            UnreadBadge {
                count: app.unreadTotal
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -width / 3
                anchors.topMargin: -height / 3
            }
        }

        Button {
            text: qsTr("Contacts")
            enabled: BluetoothManager.uartReady
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.showPage(app.pageContacts)
            // Unread direct messages.
            UnreadBadge {
                count: app.dmUnreadTotal
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -width / 3
                anchors.topMargin: -height / 3
            }
        }

        Button {
            text: qsTr("Back")
            Layout.alignment: Qt.AlignHCenter
            onClicked: app.showPage(app.pageDevices)
        }
    }
}

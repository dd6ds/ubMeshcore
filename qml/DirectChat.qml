import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * Direct (private) conversation with one contact.
 *
 * Messages live in the app-wide message model (app.messageModel); rows with
 * dm === keyHex belong to this chat. Sent messages show their delivery
 * state: MeshCore acknowledges direct messages, so "delivered" means the
 * other node confirmed receiving it.
 */
Item {
    id: chat

    property var app                 // main.qml root (helpers + message model)
    property string keyHex: ""
    property string contactName: ""

    signal backRequested()

    readonly property real gu: gridUnitPx > 0 ? gridUnitPx : 10
    readonly property int fontBody: Math.round(gu * 1.6)
    readonly property int fontSecondary: Math.round(gu * 1.3)
    readonly property int spacingUnit: Math.round(gu * 1.0)

    // While this page is open, incoming messages from this contact are read.
    Component.onCompleted: {
        app.openDmKey = keyHex
        app.markDmRead(keyHex)
    }
    Component.onDestruction: {
        if (app && app.openDmKey === keyHex)
            app.openDmKey = ""
    }

    function statusText(status) {
        if (status === "sending") return qsTr("sending...")
        if (status === "sent") return qsTr("sent - waiting for confirmation")
        if (status === "delivered") return qsTr("✓ delivered")
        if (status === "unconfirmed") return qsTr("⚠ no confirmation received")
        return ""
    }

    function send() {
        var txt = input.text.trim()
        if (txt.length === 0)
            return
        if (BluetoothManager.sendDirectMessage(keyHex, txt)) {
            app.addDm(keyHex, "You", txt, true, 0, "sending")
            input.text = ""
        }
    }

    Connections {
        target: BluetoothManager
        onSendFailed: errorLabel.text = reason
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: chat.spacingUnit

        // --- header ---------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("‹ Contacts")
                onClicked: {
                    Qt.inputMethod.hide()
                    chat.backRequested()
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                AppLabel {
                    Layout.fillWidth: true
                    text: chat.contactName
                    font.bold: true
                    font.pixelSize: chat.fontBody
                    elide: Text.ElideRight
                }
                AppLabel {
                    Layout.fillWidth: true
                    text: BluetoothManager.sessionReady
                          ? qsTr("Direct message") + "  ·  " + chat.keyHex.substring(0, 12)
                          : qsTr("Not connected to your node")
                    color: BluetoothManager.sessionReady ? "#666" : "#b00"
                    font.pixelSize: chat.fontSecondary
                    elide: Text.ElideRight
                }
            }
        }

        // --- conversation -------------------------------------------------------
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: chat.app.messageModel
            spacing: 0
            onCountChanged: positionViewAtEnd()
            Component.onCompleted: positionViewAtEnd()

            AppLabel {
                anchors.centerIn: parent
                width: parent.width * 0.85
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: list.contentHeight < 1
                text: qsTr("No messages with %1 yet.").arg(chat.contactName)
                color: "#888"
                font.pixelSize: chat.fontSecondary
            }

            delegate: Item {
                readonly property bool shown: model.dm === chat.keyHex
                readonly property bool mine: model.sender === "You"
                visible: shown
                width: ListView.view.width
                height: shown ? bubble.height + Math.round(chat.gu * 0.8) : 0

                Rectangle {
                    id: bubble
                    // Own messages right (blue), received left (grey).
                    anchors.right: mine ? parent.right : undefined
                    anchors.left: mine ? undefined : parent.left
                    width: Math.min(parent.width * 0.85,
                                    Math.max(bubbleText.implicitWidth, statusLabel.implicitWidth)
                                    + Math.round(chat.gu * 2))
                    height: bubbleColumn.implicitHeight + Math.round(chat.gu * 1.2)
                    radius: Math.round(chat.gu * 0.8)
                    color: mine ? "#dcebfb" : "#f0f0f0"

                    Column {
                        id: bubbleColumn
                        anchors.fill: parent
                        anchors.margins: Math.round(chat.gu * 0.6)
                        spacing: 2
                        AppLabel {
                            id: bubbleText
                            width: parent.width
                            text: model.text
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                        }
                        AppLabel {
                            id: statusLabel
                            text: model.time + (mine && chat.statusText(model.status).length
                                                ? "  ·  " + chat.statusText(model.status) : "")
                            color: model.status === "delivered" ? "#2e7d32"
                                 : model.status === "unconfirmed" ? "#e65100" : "#666"
                            font.pixelSize: chat.fontSecondary
                        }
                    }
                }
            }
        }

        AppLabel {
            id: errorLabel
            Layout.fillWidth: true
            visible: text.length > 0
            color: "#b00"
            wrapMode: Text.Wrap
            font.pixelSize: chat.fontSecondary
        }

        // --- input --------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            AppField {
                id: input
                Layout.fillWidth: true
                placeholderText: qsTr("Message to %1...").arg(chat.contactName)
                onAccepted: chat.send()
                onTextChanged: errorLabel.text = ""
            }
            Button {
                text: qsTr("Send")
                enabled: input.text.trim().length > 0 && BluetoothManager.sessionReady
                onClicked: chat.send()
            }
        }
    }
}

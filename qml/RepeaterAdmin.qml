import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * Remote administration of a repeater or room server over the mesh:
 * login (admin password), status block, and remote CLI commands.
 */
Item {
    id: admin

    property string keyHex: ""
    property string nodeName: ""
    property string nodeType: "repeater"

    property bool loggedIn: false
    property bool isAdmin: false
    property string busy: ""            // what we are waiting for, "" = idle
    property var statusRows: []

    signal backRequested()

    readonly property real gu: gridUnitPx > 0 ? gridUnitPx : 10
    readonly property int fontBody: Math.round(gu * 1.6)
    readonly property int fontSecondary: Math.round(gu * 1.3)
    readonly property int spacingUnit: Math.round(gu * 1.0)

    function isMe(k) {
        // Replies may only carry the 6-byte prefix (12 hex chars).
        return k.length > 0 && (keyHex.indexOf(k) === 0 || k.indexOf(keyHex) === 0)
    }

    function log(line) {
        var t = new Date().toLocaleTimeString(Qt.locale(), "HH:mm:ss")
        cliOutput.text += (cliOutput.text.length ? "\n" : "") + t + "  " + line
        cliOutput.cursorPosition = cliOutput.length
    }

    function sendCli(cmd) {
        if (!cmd || cmd.trim().length === 0)
            return
        if (BluetoothManager.sendNodeCommand(keyHex, cmd)) {
            log("> " + cmd.trim())
            busy = "Waiting for reply to \"" + cmd.trim() + "\"..."
            waitTimer.restart()
        }
    }

    // Replies travel over LoRa, possibly via several hops: give up after a while.
    Timer {
        id: waitTimer
        interval: 30000
        onTriggered: {
            if (admin.busy.length) {
                admin.log("! no reply (timeout) - " + admin.busy)
                admin.busy = ""
            }
        }
    }

    Connections {
        target: BluetoothManager
        onNodeLoginResult: {
            if (!admin.isMe(keyHex)) return
            waitTimer.stop()
            admin.busy = ""
            admin.loggedIn = success
            admin.isAdmin = success && isAdmin
            admin.log(success ? (isAdmin ? "Logged in as admin" : "Logged in as guest (read-only)")
                              : "Login failed - wrong password or no reply")
        }
        onNodeStatusReceived: {
            if (!admin.isMe(keyHex)) return
            waitTimer.stop()
            admin.busy = ""
            admin.statusRows = status
            admin.log("Status received")
        }
        onNodeCliReply: {
            if (!admin.isMe(keyHex)) return
            waitTimer.stop()
            admin.busy = ""
            admin.log(text)
        }
        onSendFailed: {
            // Always show it here: failures such as "not connected" happen
            // before anything is pending and used to be swallowed silently.
            waitTimer.stop()
            admin.log("! " + reason)
            admin.busy = ""
        }
    }

    Flickable {
        id: scroller
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true

        ColumnLayout {
            id: column
            width: scroller.width
            spacing: admin.spacingUnit

            // --- header ---------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "‹ Contacts"
                    onClicked: admin.backRequested()
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    AppLabel {
                        Layout.fillWidth: true
                        text: admin.nodeName
                        font.bold: true
                        font.pixelSize: admin.fontBody
                        elide: Text.ElideRight
                    }
                    AppLabel {
                        Layout.fillWidth: true
                        text: (admin.nodeType === "room" ? "Room server" : "Repeater")
                              + "  ·  " + admin.keyHex.substring(0, 12)
                        color: "#666"
                        font.pixelSize: admin.fontSecondary
                    }
                }
                AppLabel {
                    id: favStar
                    property bool fav: BluetoothManager.contactsModel.isFavorite(admin.keyHex)
                    text: fav ? "★" : "☆"
                    color: fav ? "#f9a825" : "#9e9e9e"
                    font.pixelSize: Math.round(admin.gu * 2.8)
                    Connections {
                        target: BluetoothManager.contactsModel
                        onFavoritesChanged: favStar.fav =
                            BluetoothManager.contactsModel.isFavorite(admin.keyHex)
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -Math.round(admin.gu * 0.6)
                        onClicked: BluetoothManager.contactsModel.toggleFavorite(admin.keyHex)
                    }
                }
            }

            // Connection state - the contact list stays visible after the
            // Bluetooth link drops (e.g. app suspended), so say so here.
            Rectangle {
                Layout.fillWidth: true
                visible: !BluetoothManager.sessionReady
                color: "#fff3e0"
                border.color: "#f57c00"
                radius: 4
                implicitHeight: connLabel.implicitHeight + Math.round(admin.gu * 1.2)
                AppLabel {
                    id: connLabel
                    anchors.fill: parent
                    anchors.margins: Math.round(admin.gu * 0.6)
                    wrapMode: Text.Wrap
                    font.pixelSize: admin.fontSecondary
                    color: "#e65100"
                    text: BluetoothManager.connected
                          ? "Connected, node session still starting... (" + BluetoothManager.connectionState + ")"
                          : "Not connected to your node. Go to the device page and connect again, then come back."
                }
            }

            AppLabel {
                Layout.fillWidth: true
                visible: admin.busy.length > 0
                text: admin.busy
                color: "#0078d7"
                font.pixelSize: admin.fontSecondary
                wrapMode: Text.Wrap
            }

            // --- login ----------------------------------------------------
            AppLabel {
                text: "Login"
                font.bold: true
                font.pixelSize: admin.fontBody
            }
            RowLayout {
                Layout.fillWidth: true
                AppField {
                    id: passwordField
                    Layout.fillWidth: true
                    placeholderText: "Admin password"
                    echoMode: TextInput.Password
                    maximumLength: 15
                    onAccepted: loginButton.clicked()
                }
                Button {
                    id: loginButton
                    text: admin.loggedIn ? "Re-login" : "Login"
                    // Only blocked while a request is pending. Without a node
                    // session the backend reports why in the console below.
                    enabled: admin.busy.length === 0
                    onClicked: {
                        if (BluetoothManager.loginToNode(admin.keyHex, passwordField.text)) {
                            admin.busy = "Logging in..."
                            admin.log("Login request sent")
                            waitTimer.restart()
                        }
                    }
                }
            }
            AppLabel {
                Layout.fillWidth: true
                font.pixelSize: admin.fontSecondary
                wrapMode: Text.Wrap
                color: admin.isAdmin ? "#2e7d32" : (admin.loggedIn ? "#f57c00" : "#666")
                text: admin.isAdmin ? "Admin session active - CLI commands enabled"
                    : admin.loggedIn ? "Guest login - status only, CLI needs the admin password"
                    : "Not logged in. Leave empty for guest access if the node allows it."
            }

            // --- status ---------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                AppLabel {
                    Layout.fillWidth: true
                    text: "Status"
                    font.bold: true
                    font.pixelSize: admin.fontBody
                }
                Button {
                    text: "Request status"
                    // Only blocked while a request is pending. Without a node
                    // session the backend reports why in the console below.
                    enabled: admin.busy.length === 0
                    onClicked: {
                        if (BluetoothManager.requestNodeStatus(admin.keyHex)) {
                            admin.busy = "Requesting status..."
                            waitTimer.restart()
                        }
                    }
                }
            }
            AppLabel {
                Layout.fillWidth: true
                visible: admin.statusRows.length === 0
                text: "No status yet. Most repeaters answer status requests only after a login."
                color: "#888"
                font.pixelSize: admin.fontSecondary
                wrapMode: Text.Wrap
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: admin.spacingUnit
                rowSpacing: Math.round(admin.gu * 0.3)
                visible: admin.statusRows.length > 0
                Repeater {
                    model: admin.statusRows.length * 2
                    delegate: AppLabel {
                        readonly property var r: admin.statusRows[Math.floor(index / 2)]
                        Layout.fillWidth: index % 2 === 1
                        text: index % 2 === 0 ? r.label : r.value
                        color: index % 2 === 0 ? "#555" : "#111"
                        font.bold: index % 2 === 1
                        font.pixelSize: admin.fontSecondary
                    }
                }
            }

            // --- remote CLI -----------------------------------------------
            AppLabel {
                text: "Commands"
                font.bold: true
                font.pixelSize: admin.fontBody
            }
            Flow {
                Layout.fillWidth: true
                spacing: Math.round(admin.gu * 0.5)
                enabled: admin.isAdmin && admin.busy.length === 0
                Repeater {
                    model: [
                        { t: "Version",    c: "ver" },
                        { t: "Clock",      c: "clock" },
                        { t: "Sync clock", c: "clock sync" },
                        { t: "Neighbours", c: "neighbors" },
                        { t: "Name",       c: "get name" },
                        { t: "Radio",      c: "get radio" },
                        { t: "TX power",   c: "get tx" },
                        { t: "Send advert", c: "advert" }
                    ]
                    delegate: Button {
                        text: modelData.t
                        font.pixelSize: admin.fontSecondary
                        onClicked: admin.sendCli(modelData.c)
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: admin.isAdmin
                AppField {
                    id: cliField
                    Layout.fillWidth: true
                    placeholderText: admin.isAdmin ? "CLI command, e.g. set name MyRepeater"
                                                   : "Log in as admin to send commands"
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    onAccepted: sendButton.clicked()
                }
                Button {
                    id: sendButton
                    text: "Send"
                    enabled: admin.busy.length === 0 && cliField.text.trim().length > 0
                    onClicked: {
                        admin.sendCli(cliField.text)
                        cliField.text = ""
                    }
                }
            }

            // --- output ---------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.round(admin.gu * 22)
                color: "#1e1e1e"
                radius: 4
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Math.round(admin.gu * 0.5)
                    TextArea {
                        id: cliOutput
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        color: "#d0f0d0"
                        font.family: "monospace"
                        font.pixelSize: admin.fontSecondary
                        background: null
                        selectByMouse: true
                    }
                }
            }
            Item { Layout.preferredHeight: admin.spacingUnit }
        }
    }
}

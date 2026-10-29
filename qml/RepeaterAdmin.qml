import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * Remote administration of a repeater or room server over the mesh:
 * login (admin password), status block, and remote CLI commands.
 */
Item {
    id: admin

    property var app                 // main.qml root (colours)
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
            busy = qsTr("Waiting for reply to \"%1\"...").arg(cmd.trim())
            waitTimer.restart()
        }
    }

    // Replies travel over LoRa, possibly via several hops: give up after a while.
    Timer {
        id: waitTimer
        interval: 30000
        onTriggered: {
            if (admin.busy.length) {
                admin.log(qsTr("! no reply (timeout) - %1").arg(admin.busy))
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
            admin.log(success ? (isAdmin ? qsTr("Logged in as admin")
                                      : qsTr("Logged in as guest (read-only)"))
                              : qsTr("Login failed - wrong password or no reply"))
        }
        onNodeStatusReceived: {
            if (!admin.isMe(keyHex)) return
            waitTimer.stop()
            admin.busy = ""
            admin.statusRows = status
            admin.log(qsTr("Status received"))
        }
        onNodeCliReply: {
            if (!admin.isMe(keyHex)) return
            waitTimer.stop()
            admin.busy = ""
            admin.log(text)
        }
        onSendFailed: {
            // Always show it here: failures such as "not connected" happen
            // before anything is pending and would otherwise go unnoticed.
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
                    text: qsTr("‹ Contacts")
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
                        color: admin.app.colorSecondaryText
                        font.pixelSize: admin.fontSecondary
                    }
                }
                AppLabel {
                    id: favStar
                    property bool fav: BluetoothManager.contactsModel.isFavorite(admin.keyHex)
                    text: fav ? "★" : "☆"
                    color: fav ? admin.app.colorFavorite : admin.app.colorInactive
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
                color: admin.app.colorWarningBackground
                border.color: admin.app.colorWarningBorder
                radius: 4
                implicitHeight: connLabel.implicitHeight + Math.round(admin.gu * 1.2)
                AppLabel {
                    id: connLabel
                    anchors.fill: parent
                    anchors.margins: Math.round(admin.gu * 0.6)
                    wrapMode: Text.Wrap
                    font.pixelSize: admin.fontSecondary
                    color: admin.app.colorWarning
                    text: BluetoothManager.connected
                          ? qsTr("Connected, node session still starting... (%1)")
                            .arg(BluetoothManager.connectionState)
                          : qsTr("Not connected to your node. Go to the device page and "
                                 + "connect again, then come back.")
                }
            }

            AppLabel {
                Layout.fillWidth: true
                visible: admin.busy.length > 0
                text: admin.busy
                color: admin.app.colorAccent
                font.pixelSize: admin.fontSecondary
                wrapMode: Text.Wrap
            }

            // --- login ----------------------------------------------------
            AppLabel {
                text: qsTr("Login")
                font.bold: true
                font.pixelSize: admin.fontBody
            }
            RowLayout {
                Layout.fillWidth: true
                AppField {
                    id: passwordField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Admin password")
                    echoMode: TextInput.Password
                    maximumLength: 15
                    onAccepted: loginButton.clicked()
                }
                Button {
                    id: loginButton
                    text: admin.loggedIn ? qsTr("Re-login") : qsTr("Login")
                    // Only blocked while a request is pending. Without a node
                    // session the backend reports why in the console below.
                    enabled: admin.busy.length === 0
                    onClicked: {
                        if (BluetoothManager.loginToNode(admin.keyHex, passwordField.text)) {
                            admin.busy = qsTr("Logging in...")
                            admin.log(qsTr("Login request sent"))
                            waitTimer.restart()
                        }
                    }
                }
            }
            AppLabel {
                Layout.fillWidth: true
                font.pixelSize: admin.fontSecondary
                wrapMode: Text.Wrap
                color: admin.isAdmin ? admin.app.colorSuccess
                     : (admin.loggedIn ? admin.app.colorWarning : admin.app.colorSecondaryText)
                text: admin.isAdmin ? qsTr("Admin session active - CLI commands enabled")
                    : admin.loggedIn
                      ? qsTr("Guest login - status only, CLI needs the admin password")
                    : qsTr("Not logged in. Leave empty for guest access if the node allows it.")
            }

            // --- status ---------------------------------------------------
            RowLayout {
                Layout.fillWidth: true
                AppLabel {
                    Layout.fillWidth: true
                    text: qsTr("Status")
                    font.bold: true
                    font.pixelSize: admin.fontBody
                }
                Button {
                    text: qsTr("Request status")
                    // Only blocked while a request is pending. Without a node
                    // session the backend reports why in the console below.
                    enabled: admin.busy.length === 0
                    onClicked: {
                        if (BluetoothManager.requestNodeStatus(admin.keyHex)) {
                            admin.busy = qsTr("Requesting status...")
                            waitTimer.restart()
                        }
                    }
                }
            }
            AppLabel {
                Layout.fillWidth: true
                visible: admin.statusRows.length === 0
                text: qsTr("No status yet. Most repeaters answer status requests "
                           + "only after a login.")
                color: admin.app.colorSecondaryText
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
                        color: index % 2 === 0 ? admin.app.colorSecondaryText : admin.app.colorText
                        font.bold: index % 2 === 1
                        font.pixelSize: admin.fontSecondary
                    }
                }
            }

            // --- remote CLI -----------------------------------------------
            AppLabel {
                text: qsTr("Commands")
                font.bold: true
                font.pixelSize: admin.fontBody
            }
            Flow {
                Layout.fillWidth: true
                spacing: Math.round(admin.gu * 0.5)
                enabled: admin.isAdmin && admin.busy.length === 0
                Repeater {
                    model: [
                        { t: qsTr("Version"),    c: "ver" },
                        { t: qsTr("Clock"),      c: "clock" },
                        { t: qsTr("Sync clock"), c: "clock sync" },
                        { t: qsTr("Neighbours"), c: "neighbors" },
                        { t: qsTr("Name"),       c: "get name" },
                        { t: qsTr("Radio"),      c: "get radio" },
                        { t: qsTr("TX power"),   c: "get tx" },
                        { t: qsTr("Send advert"), c: "advert" }
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
                    placeholderText: admin.isAdmin ? qsTr("CLI command, e.g. set name MyRepeater")
                                                   : qsTr("Log in as admin to send commands")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    onAccepted: sendButton.clicked()
                }
                Button {
                    id: sendButton
                    text: qsTr("Send")
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

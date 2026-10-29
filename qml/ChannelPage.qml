import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Fourth page: the messages of the open channel, with the input to send one.
Page {
    id: page
    property var app                 // main.qml root (message model, open channel)

    title: qsTr("Messages - %1").arg(app.currentChannelName || qsTr("No Channel"))

    // "4 hops · cc4b › 2a67 › 6228 › db02" -> "4 hops: cc4b→2a67→6228→db02",
    // the compact form ping bots use in their replies. Summaries without a
    // recorded path ("3 hops", "direct", "0 hops - heard directly") stay as is.
    function compactRoute(route) {
        var sep = route.indexOf(" · ")
        if (sep < 0)
            return route
        return route.substring(0, sep) + ": "
                + route.substring(sep + 3).split(" › ").join("→")
    }

    // Prefills the input with a MeshCore mention ("@[name] ") - the format
    // other MeshCore apps show as a reply - plus optional extra text.
    function startReply(sender, extra) {
        var text = "@[" + sender + "] " + (extra.length ? extra + " " : "")
        messageInput.text = text
        messageInput.cursorPosition = text.length
        messageInput.forceActiveFocus()
        Qt.inputMethod.show()
    }

    // The checks happen here instead of disabling the button: a disabled
    // button gives no feedback at all - it just looks broken.
    function send() {
        var txt = messageInput.text
        if (txt.length === 0) {
            app.addLocalMessage("!", qsTr("Nothing typed yet"))
            return
        }
        if (!BluetoothManager.sessionReady) {
            app.addLocalMessage("!", qsTr("No node session - connect first"))
            return
        }
        if (app.currentChannelIdx < 0) {
            app.addLocalMessage("!", qsTr("No channel selected"))
            return
        }
        if (BluetoothManager.sendChannelMessage(app.currentChannelIdx, txt)) {
            app.addLocalMessage("You", txt)
            messageInput.text = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: app.spacingUnit
        spacing: app.spacingUnit

        // Header with the back button at the TOP: a button at the bottom is
        // hidden behind the on-screen keyboard.
        RowLayout {
            Layout.fillWidth: true
            spacing: app.spacingUnit

            Button {
                text: qsTr("‹ Channels")
                Layout.preferredHeight: Math.round(app.gu * 5)
                onClicked: {
                    messageInput.focus = false
                    Qt.inputMethod.hide()
                    app.showPage(app.pageChannels)
                }
                // New messages waiting in OTHER channels.
                UnreadBadge {
                    count: app.unreadElsewhere()
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: -width / 3
                    anchors.topMargin: -height / 3
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                AppLabel {
                    Layout.fillWidth: true
                    text: app.currentChannelName || qsTr("No channel")
                    font.bold: true
                    elide: Text.ElideRight
                }
                AppLabel {
                    Layout.fillWidth: true
                    text: BluetoothManager.sessionReady
                          ? qsTr("session ready")
                            + (BluetoothManager.batteryPercent >= 0
                               ? "  🔋 " + BluetoothManager.batteryPercent + "%" : "")
                          : qsTr("no node session")
                    color: BluetoothManager.sessionReady ? app.colorSuccess : app.colorError
                    font.pixelSize: app.fontSecondary
                }
                AppLabel {
                    Layout.fillWidth: true
                    visible: BluetoothManager.lastSyncInfo.length > 0
                    text: "↓ " + BluetoothManager.lastSyncInfo
                    color: app.colorAccent
                    font.pixelSize: app.fontSecondary
                    elide: Text.ElideRight
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: app.spacingUnit

            AppField {
                id: messageInput
                placeholderText: qsTr("Enter message...")
                font.pixelSize: app.fontBody
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Send")
                Layout.preferredHeight: Math.round(app.gu * 5)
                onClicked: page.send()
            }

            Button {
                text: qsTr("Refresh")
                Layout.preferredHeight: Math.round(app.gu * 5)
                onClicked: BluetoothManager.syncNextMessage()
            }
        }

        ListView {
            id: messageView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.messageModel
            clip: true
            // Follow new messages of the open channel.
            onCountChanged: positionViewAtEnd()

            AppLabel {
                anchors.centerIn: parent
                visible: messageView.contentHeight < 1
                text: qsTr("No messages in this channel yet")
                color: app.colorSecondaryText
                font.pixelSize: app.fontSecondary
            }

            delegate: Item {
                id: messageDelegate
                // Only the open channel is shown; other rows collapse to zero
                // height but stay in the model. Direct messages (dm set,
                // channel -1) never show here, even when no channel is
                // selected (idx -1).
                readonly property bool shown: model.channel === app.currentChannelIdx
                                              && model.dm === ""
                // Received messages only - not our own or status lines.
                readonly property bool replyable: model.sender !== "You" && model.sender !== "!"
                property bool expanded: false

                visible: shown
                height: shown ? msgColumn.implicitHeight + app.spacingUnit : 0
                width: ListView.view.width

                Rectangle {
                    anchors.fill: parent
                    z: -1
                    color: messageMouse.pressed && messageDelegate.replyable
                           ? app.colorSelected : "transparent"
                }

                Column {
                    id: msgColumn
                    spacing: 2
                    anchors { left: parent.left; right: parent.right; margins: app.spacingUnit }

                    AppLabel {
                        width: parent.width
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        text: model.sender + ": " + model.text
                        color: model.sender === "!" ? app.colorError
                             : model.sender === "You" ? app.colorOwnName : app.colorText
                    }
                    AppLabel {
                        width: parent.width
                        // "08:31  ·  ↳ 2 hops · 2a67 › 6228"
                        text: model.time + (model.route.length ? "  ·  ↳ " + model.route : "")
                        color: app.colorSecondaryText
                        font.pixelSize: app.fontSecondary
                        elide: Text.ElideRight
                    }
                    // Message path, hop by hop - tap the message.
                    Rectangle {
                        visible: messageDelegate.expanded
                        width: parent.width
                        height: visible ? pathText.implicitHeight + Math.round(app.gu * 1.0) : 0
                        color: app.colorSelected
                        border.color: app.colorBorder
                        radius: 4
                        AppLabel {
                            id: pathText
                            anchors.fill: parent
                            anchors.margins: Math.round(app.gu * 0.5)
                            text: model.routeDetail
                            textFormat: Text.PlainText
                            wrapMode: Text.Wrap
                            font.family: "monospace"
                            font.pixelSize: app.fontSecondary
                        }
                    }
                }

                MouseArea {
                    id: messageMouse
                    anchors.fill: parent
                    // Tap: show/hide the hop-by-hop path.
                    onClicked: {
                        if (model.routeDetail.length > 0)
                            messageDelegate.expanded = !messageDelegate.expanded
                    }
                    // Long press: reply menu.
                    onPressAndHold: {
                        if (!messageDelegate.replyable)
                            return
                        messageMenu.sender = model.sender
                        messageMenu.route = model.route
                        var p = mapToItem(messageView, mouse.x, mouse.y)
                        messageMenu.popup(messageView, p.x, p.y)
                    }
                }
            }

            Menu {
                id: messageMenu
                property string sender: ""
                property string route: ""
                MenuItem {
                    text: qsTr("Reply to %1").arg(messageMenu.sender)
                    onTriggered: page.startReply(messageMenu.sender, "")
                }
                MenuItem {
                    text: qsTr("Reply with route")
                    enabled: messageMenu.route.length > 0
                    onTriggered: page.startReply(messageMenu.sender,
                                                 page.compactRoute(messageMenu.route))
                }
            }
        }
    }
}

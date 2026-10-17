// Ubuntu Touch 20.04 ships Qt 5.12 - importing 2.15 makes the whole file fail
// to load. 2.12 is the highest version actually available on the device.
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

ApplicationWindow {
    id: rootWindow
    visible: true
    width: Screen.width
    height: Screen.height
    title: "ubMeshcore"

    // Pin the background instead of inheriting the platform palette, so the
    // explicit dark text colours always have the contrast they assume.
    color: "#ffffff"

    // --- Scaling -----------------------------------------------------------
    // gridUnitPx comes from main.cpp (Ubuntu Touch's GRID_UNIT_PX). Everything
    // is sized from it so text is readable on a high-DPI phone display.
    readonly property real gu: gridUnitPx
    readonly property int fontTitle:     Math.round(gu * 2.0)
    readonly property int fontBody:      Math.round(gu * 1.6)
    readonly property int fontSecondary: Math.round(gu * 1.3)
    readonly property int spacingUnit:   Math.round(gu * 1.5)

    // Propagates to every control and Label that does not override it.
    font.pixelSize: fontBody

    // Currently joined channel (index is what the companion protocol wants).
    property int currentChannelIdx: -1
    property string currentChannelName: ""

    // Incoming channel messages land here from C++.
    Connections {
        target: BluetoothManager
        onChannelMessage: {
            // Channel texts arrive as "SenderName: message".
            var sep = text.indexOf(": ")
            var hasSender = sep > 0 && sep <= 32
            rootWindow.addMessage(channelIdx,
                                  hasSender ? text.substring(0, sep) : "Ch" + channelIdx,
                                  hasSender ? text.substring(sep + 2) : text,
                                  true, timestamp, route)
        }
        onSendFailed: rootWindow.addLocalMessage("!", reason)
    }

    // Every message carries the channel it belongs to; the Messages page only
    // shows the ones of currentChannelIdx. Real messages are also written to
    // MessageStore so the history survives disconnects and app restarts -
    // the node hands each message over only once.
    // "hh:mm" for today, "dd.MM. hh:mm" for older messages - cached messages
    // fetched from the node in the morning can be from last night.
    function timeText(ts) {
        var d = new Date(ts * 1000)
        var today = new Date()
        today.setHours(0, 0, 0, 0)
        return Qt.formatDateTime(d, d.getTime() < today.getTime() ? "dd.MM. hh:mm" : "hh:mm")
    }

    // ts = sender's send time in epoch secs (defaults to now).
    // route = { route: one-line path summary, routeDetail: hop-by-hop text }
    // for received messages; every row carries both roles (ListModel needs a
    // consistent set), empty for our own and local entries.
    function addMessage(channel, sender, text, persist, ts, route) {
        if (!ts)
            ts = Math.floor(Date.now() / 1000)
        var r = route ? route.route || "" : ""
        var rd = route ? route.routeDetail || "" : ""
        messageList.append({ channel: channel, sender: sender, text: text,
                             time: timeText(ts), route: r, routeDetail: rd })
        if (persist)
            MessageStore.append({ channel: channel, sender: sender, text: text, ts: ts,
                                  route: r, routeDetail: rd })
    }

    function addLocalMessage(sender, text) {
        // "!" entries are transient status/errors and are not stored.
        addMessage(rootWindow.currentChannelIdx, sender, text, sender !== "!", 0, null)
    }

    // The channel pages set the index first and the name second, so save on
    // either change to never store a stale name.
    function saveLastChannel() {
        if (currentChannelIdx >= 0)
            MessageStore.setLastChannel(currentChannelIdx, currentChannelName)
    }
    onCurrentChannelIdxChanged: saveLastChannel()
    onCurrentChannelNameChanged: saveLastChannel()

    Component.onCompleted: {
        // Restore the history first, then the last opened channel.
        var stored = MessageStore.loadAll()
        for (var i = 0; i < stored.length; ++i) {
            var m = stored[i]
            messageList.append({ channel: m.channel, sender: m.sender, text: m.text,
                                 time: m.ts ? timeText(m.ts) : (m.time || ""),
                                 route: m.route || "", routeDetail: m.routeDetail || "" })
        }
        var last = MessageStore.lastChannel()
        if (last.idx >= 0) {
            currentChannelName = last.name
            currentChannelIdx = last.idx
        }
    }

    ListModel { id: messageList }
    
    // Main stack for navigation
    StackLayout {
        anchors.fill: parent
        currentIndex: 0
        
        id: mainStack
        
        // Page 1: Node Discovery
        Page {
            id: nodeDiscoveryPage
            title: "Discover MeshCore Nodes"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit

                AppLabel {
                    text: "Scan for Bluetooth devices"
                    font.bold: true
                    font.pixelSize: rootWindow.fontTitle
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    text: BluetoothManager.scanActive ? "Stop Scan" : "Start Scan"
                    enabled: BluetoothManager.adapterReady || BluetoothManager.scanActive
                    font.pixelSize: rootWindow.fontBody
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.round(rootWindow.gu * 22)
                    Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                    onClicked: {
                        if (BluetoothManager.scanActive)
                            BluetoothManager.stopScan()
                        else
                            BluetoothManager.startScan()
                    }
                }

                AppLabel {
                    text: BluetoothManager.status
                    font.pixelSize: rootWindow.fontSecondary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: "#666"
                }

                AppLabel {
                    text: BluetoothManager.connected
                          ? "Connected to: " + BluetoothManager.connectedDevice
                          : BluetoothManager.devicesModel.count + " device(s) found"
                    font.pixelSize: rootWindow.fontSecondary
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    color: "#666"
                }

                ListView {
                    id: deviceListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: Math.round(rootWindow.gu * 0.75)
                    model: BluetoothManager.devicesModel

                    delegate: Rectangle {
                        id: deviceRow
                        width: deviceListView.width
                        height: Math.round(rootWindow.gu * 8)
                        radius: Math.round(rootWindow.gu * 0.5)
                        color: rowMouse.pressed
                               ? "#b0d4ff"
                               : (model.isMeshCore ? "#e6f4ea" : "#f0f0f0")
                        border.color: BluetoothManager.selectedAddress === model.address
                                      ? "#0078d7"
                                      : (model.isMeshCore ? "#2e7d32" : "#cccccc")
                        border.width: BluetoothManager.selectedAddress === model.address ? 3 : 1

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.margins: rootWindow.spacingUnit
                            spacing: Math.round(rootWindow.gu * 0.5)

                            AppLabel {
                                text: model.name
                                font.bold: model.hasName
                                font.italic: !model.hasName
                                font.pixelSize: rootWindow.fontBody
                                color: model.hasName ? "#000" : "#888"
                                elide: Text.ElideRight
                                width: parent.width
                            }
                            AppLabel {
                                text: model.address
                                      + (model.lowEnergy ? "  ·  BLE" : "  ·  Classic")
                                      + (model.rssi !== 0 ? "  ·  " + model.rssi + " dBm" : "")
                                color: "#555"
                                font.pixelSize: rootWindow.fontSecondary
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            onClicked: {
                                console.log("device row clicked:", model.name, model.address)
                                BluetoothManager.selectDevice(model.address, model.name)
                                mainStack.currentIndex = 1
                            }
                        }
                    }
                }
            }
        }
        
        // Page 2: Node Connection
        Page {
            id: nodeConnectionPage
            title: "Connect to Node"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: "Selected Node: "
                          + (BluetoothManager.hasSelection ? BluetoothManager.selectedName
                                                           : "None")
                    font.bold: true
                    font.pixelSize: rootWindow.fontTitle
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                AppLabel {
                    text: BluetoothManager.selectedAddress
                    color: "#666"
                    font.pixelSize: rootWindow.fontSecondary
                    Layout.fillWidth: true
                }

                AppLabel {
                    text: BluetoothManager.connectionState
                    color: BluetoothManager.uartReady ? "#2e7d32" : "#666"
                    font.pixelSize: rootWindow.fontBody
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }

                BusyIndicator {
                    running: BluetoothManager.connecting
                    visible: BluetoothManager.connecting
                    Layout.alignment: Qt.AlignHCenter
                }

                Button {
                    text: BluetoothManager.connected ? "Disconnect" : "Connect"
                    enabled: BluetoothManager.hasSelection && !BluetoothManager.connecting
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.round(rootWindow.gu * 22)
                    Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                    onClicked: {
                        console.log("connect button clicked, selected:",
                                    BluetoothManager.selectedAddress)
                        if (BluetoothManager.connected)
                            BluetoothManager.disconnect()
                        else
                            BluetoothManager.connectToSelected()
                    }
                }

                AppLabel {
                    text: BluetoothManager.sessionReady
                          ? "Node: " + (BluetoothManager.nodeName || "(unnamed)")
                          : ""
                    visible: BluetoothManager.sessionReady
                    font.pixelSize: rootWindow.fontBody
                    color: "#2e7d32"
                    Layout.fillWidth: true
                }

                // Companion protocol trace - the quickest way to see where a
                // handshake stalls without digging through clickable logs.
                AppLabel {
                    text: BluetoothManager.trafficLog.slice(-7).join("\n")
                    visible: BluetoothManager.uartReady
                    color: "#555"
                    font.pixelSize: Math.round(rootWindow.gu * 1.0)
                    font.family: "monospace"
                    Layout.fillWidth: true
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 7
                    elide: Text.ElideRight
                }

                Item { Layout.fillHeight: true }

                Button {
                    text: "Channels"
                    enabled: BluetoothManager.uartReady
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 2
                }

                Button {
                    text: "Contacts"
                    enabled: BluetoothManager.uartReady
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 4
                }

                Button {
                    text: "Back"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 0
                }
            }
            

        }
        
        // Page 3: Channel Selection
        Page {
            id: channelPage
            title: "MeshCore Channels"
            
            // A Flickable instead of a stretched ColumnLayout: a ListView has a
            // viewport that has to be sized correctly, and whatever did not fit
            // ended up underneath the buttons below it. A Repeater has no
            // viewport, so every row is simply laid out, and the Flickable lets
            // the page scroll if it gets taller than the screen.
            Flickable {
                anchors.fill: parent
                contentWidth: width
                contentHeight: channelColumn.implicitHeight + 2 * rootWindow.spacingUnit
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                ColumnLayout {
                    id: channelColumn
                    x: rootWindow.spacingUnit
                    y: rootWindow.spacingUnit
                    width: parent.width - 2 * rootWindow.spacingUnit
                    spacing: rootWindow.spacingUnit

                    AppLabel {
                        text: "Connected to: "
                              + (BluetoothManager.uartReady ? BluetoothManager.selectedName
                                                            : "not connected")
                        font.bold: true
                        font.pixelSize: rootWindow.fontBody
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    AppField {
                        id: channelInput
                        placeholderText: "Channel name, e.g. #p53"
                        font.pixelSize: rootWindow.fontBody
                        Layout.fillWidth: true
                        // Typing a name picks the matching channel from the
                        // node's table, so the field is a shortcut rather than
                        // a dead end.
                        onTextChanged: {
                            var names = BluetoothManager.channelNames
                            for (var i = 0; i < names.length; ++i) {
                                if (names[i].toLowerCase() === text.toLowerCase().trim()) {
                                    rootWindow.currentChannelIdx = i
                                    rootWindow.currentChannelName = names[i]
                                    return
                                }
                            }
                        }
                    }

                    Button {
                        text: rootWindow.currentChannelIdx >= 0
                              ? "Open " + rootWindow.currentChannelName
                              : "Pick a channel"
                        enabled: rootWindow.currentChannelIdx >= 0
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.round(rootWindow.gu * 22)
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: mainStack.currentIndex = 3
                    }

                    // As soon as the node's channel table arrives, preselect
                    // the first entry so the page is immediately usable.
                    Connections {
                        target: BluetoothManager
                        onChannelsChanged: {
                            console.log("channelNames from C++:",
                                        JSON.stringify(BluetoothManager.channelNames))
                            if (rootWindow.currentChannelIdx < 0
                                && BluetoothManager.channelNames.length > 0) {
                                rootWindow.currentChannelIdx = 0
                                rootWindow.currentChannelName =
                                    BluetoothManager.channelNames[0]
                            }
                        }
                    }

                    AppLabel {
                        text: BluetoothManager.channelNames.length > 0
                              ? "Channels on " + BluetoothManager.nodeName + ":"
                              : "Waiting for the channel list from the node..."
                        color: "#666"
                        font.pixelSize: rootWindow.fontSecondary
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    Repeater {
                        // Straight from the node - no placeholder rows, so an
                        // empty list is visibly empty instead of showing two
                        // invented channels that cannot be opened.
                        model: BluetoothManager.channelNames
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.round(rootWindow.gu * 7)
                            radius: Math.round(rootWindow.gu * 0.5)
                            color: channelMouse.pressed
                                   ? "#b0d4ff"
                                   : (rootWindow.currentChannelIdx === index ? "#e6f0ff"
                                                                             : "#f0f0f0")
                            border.color: rootWindow.currentChannelIdx === index
                                          ? "#0078d7" : "#cccccc"
                            border.width: rootWindow.currentChannelIdx === index ? 3 : 1

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: rootWindow.spacingUnit
                                spacing: Math.round(rootWindow.gu * 0.5)
                                AppLabel {
                                    text: String(modelData)
                                    font.bold: true
                                    font.pixelSize: rootWindow.fontTitle
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }
                            MouseArea {
                                id: channelMouse
                                anchors.fill: parent
                                onClicked: {
                                    console.log("channel selected:", index, modelData)
                                    rootWindow.currentChannelIdx = index
                                    rootWindow.currentChannelName =
                                        (modelData && modelData.length > 0)
                                            ? modelData : "Channel " + index
                                    channelInput.text = rootWindow.currentChannelName
                                }
                            }
                        }
                    }

                    Button {
                        text: "Back"
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: mainStack.currentIndex = 1
                    }
                }
            }
            
            property string connectedNode: ""
        }
        
        // Page 4: Messages
        Page {
            id: messagesPage
            title: "Messages - " + (rootWindow.currentChannelName
                                    ? rootWindow.currentChannelName : "No Channel")
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit

                // Header with the back button at the TOP: the old button at
                // the bottom was hidden behind the on-screen keyboard.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: rootWindow.spacingUnit

                    Button {
                        text: "‹ Channels"
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: {
                            messageInput.focus = false
                            Qt.inputMethod.hide()
                            mainStack.currentIndex = 2
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        AppLabel {
                            Layout.fillWidth: true
                            text: rootWindow.currentChannelName || "No channel"
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        AppLabel {
                            Layout.fillWidth: true
                            text: BluetoothManager.sessionReady ? "session ready" : "no node session"
                            color: BluetoothManager.sessionReady ? "#2e7d32" : "#b00"
                            font.pixelSize: rootWindow.fontSecondary
                        }
                        AppLabel {
                            Layout.fillWidth: true
                            visible: BluetoothManager.lastSyncInfo.length > 0
                            text: "↓ " + BluetoothManager.lastSyncInfo
                            color: "#0078d7"
                            font.pixelSize: rootWindow.fontSecondary
                            elide: Text.ElideRight
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: rootWindow.spacingUnit
                    
                    AppField {
                        id: messageInput
                        placeholderText: "Enter message..."
                        font.pixelSize: rootWindow.fontBody
                        Layout.fillWidth: true
                    }
                    
                    Button {
                        text: "Send"
                        // Deliberately always enabled. A disabled button gives
                        // no feedback at all - it just looks broken - so the
                        // checks happen here and say what is missing.
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: {
                            var txt = messageInput.text
                            console.log("send clicked: channel",
                                        rootWindow.currentChannelIdx,
                                        "session", BluetoothManager.sessionReady,
                                        "len", txt.length)
                            if (txt.length === 0) {
                                rootWindow.addLocalMessage("!", "Nothing typed yet")
                                return
                            }
                            if (!BluetoothManager.sessionReady) {
                                rootWindow.addLocalMessage("!", "No node session - connect first")
                                return
                            }
                            if (rootWindow.currentChannelIdx < 0) {
                                rootWindow.addLocalMessage("!", "No channel selected")
                                return
                            }
                            if (BluetoothManager.sendChannelMessage(
                                    rootWindow.currentChannelIdx, txt)) {
                                rootWindow.addLocalMessage("You", txt)
                                messageInput.text = ""
                            }
                        }
                    }
                    
                    Button {
                        text: "Refresh"
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: {
                            console.log("Refreshing messages...")
                            BluetoothManager.syncNextMessage()
                        }
                    }
                }
                
                ListView {
                    id: messageView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: messageList
                    clip: true
                    // Follow new messages of the open channel.
                    onCountChanged: positionViewAtEnd()

                    AppLabel {
                        anchors.centerIn: parent
                        visible: messageView.contentHeight < 1
                        text: "No messages in this channel yet"
                        color: "#888"
                        font.pixelSize: rootWindow.fontSecondary
                    }

                    delegate: Item {
                        id: messageDelegate
                        // Only the selected channel is shown; other rows
                        // collapse to zero height but stay in the model.
                        readonly property bool shown: model.channel === rootWindow.currentChannelIdx
                        visible: shown
                        height: shown ? msgColumn.implicitHeight + rootWindow.spacingUnit : 0
                        width: ListView.view.width
                        Column {
                            id: msgColumn
                            spacing: 2
                            anchors { left: parent.left; right: parent.right; margins: rootWindow.spacingUnit }
                            AppLabel {
                                width: parent.width
                                wrapMode: Text.Wrap
                                textFormat: Text.PlainText
                                text: model.sender + ": " + model.text
                                color: model.sender === "!" ? "#b00"
                                     : model.sender === "You" ? "#0050a0" : "#111111"
                            }
                            AppLabel {
                                width: parent.width
                                // "08:31  ·  ↳ 2 hops · 2a67 › 6228"
                                text: model.time + (model.route.length
                                                    ? "  ·  ↳ " + model.route : "")
                                color: "#666"
                                font.pixelSize: rootWindow.fontSecondary
                                elide: Text.ElideRight
                            }
                            // Message path, hop by hop - tap the message.
                            Rectangle {
                                visible: messageDelegate.expanded
                                width: parent.width
                                height: visible ? pathText.implicitHeight + Math.round(rootWindow.gu * 1.0) : 0
                                color: "#f2f6fb"
                                border.color: "#c5d6ea"
                                radius: 4
                                AppLabel {
                                    id: pathText
                                    anchors.fill: parent
                                    anchors.margins: Math.round(rootWindow.gu * 0.5)
                                    text: model.routeDetail
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    font.family: "monospace"
                                    font.pixelSize: rootWindow.fontSecondary
                                }
                            }
                        }
                        property bool expanded: false
                        MouseArea {
                            anchors.fill: parent
                            enabled: model.routeDetail.length > 0
                            onClicked: messageDelegate.expanded = !messageDelegate.expanded
                        }
                    }
                }
            }
            
        }
        
        // Page 4: Contacts
        Page {
            id: contactsPage
            title: "Contacts"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                ContactList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                
                Button {
                    text: "Back"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: Math.round(rootWindow.gu * 22)
                    Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                    onClicked: mainStack.currentIndex = 1
                }
            }
        }
        
        // Page 5: Settings
        Page {
            id: settingsPage
            title: "Settings"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: "App Settings"
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: "Node Administration"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 6
                }
                
                Button {
                    text: "About"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 7
                }
                
                Button {
                    text: "Back"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 0
                }
            }
        }
        
        // Page 6: Node Admin
        Page {
            id: adminPage
            title: "Node Administration"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: "Administer connected node"
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }
                
                AppField {
                    placeholderText: "Admin Password"
                    echoMode: TextInput.Password
                    Layout.fillWidth: true
                }
                
                Button {
                    text: "Send Command"
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: "Back"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 5
                }
            }
        }
        
        // Page 7: About
        Page {
            id: aboutPage
            title: "About ubMeshcore"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: "ubMeshcore"
                    font.bold: true
                    font.pixelSize: rootWindow.fontTitle
                    Layout.alignment: Qt.AlignHCenter
                }
                
                AppLabel {
                    text: "Version " + Qt.application.version
                    Layout.alignment: Qt.AlignHCenter
                }
                
                AppLabel {
                    text: "Bluetooth companion app for MeshCore nodes on Ubuntu Touch"
                    Layout.alignment: Qt.AlignHCenter
                    wrapMode: Text.Wrap
                }
                
                AppLabel {
                    text: "Based on meshcore-info-display project"
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: "Back"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 5
                }
            }
        }
    }
    
    // Ubuntu Touch specific: use system toolbar
    ToolBar {
        id: toolbar
        visible: Qt.platform.os === "ubuntu" ? true : false
        anchors.bottom: parent.bottom
        
        ToolButton {
            text: "Nodes"
            onClicked: mainStack.currentIndex = 0
        }
        ToolButton {
            text: "Channels"
            onClicked: mainStack.currentIndex = 2
        }
        ToolButton {
            text: "Messages"
            onClicked: mainStack.currentIndex = 3
        }
        ToolButton {
            text: "Contacts"
            onClicked: mainStack.currentIndex = 4
        }
        ToolButton {
            text: "Settings"
            onClicked: mainStack.currentIndex = 5
        }
    }
}

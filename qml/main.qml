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
            // Count it as new unless that channel is open on screen right now.
            if (!rootWindow.isReading(channelIdx))
                rootWindow.bumpUnread(channelIdx)
        }
        onSendFailed: rootWindow.addLocalMessage("!", reason)

        // --- direct messages ---------------------------------------------
        onDirectMessage: {
            rootWindow.addDm(keyHex, name.length ? name : keyHex.substring(0, 8),
                             text, true, timestamp, "")
            if (!rootWindow.isReadingDm(keyHex))
                rootWindow.bumpUnread("dm:" + keyHex)
        }
        // The node queued our DM: remember its ack code (oldest unacked
        // message to that contact) and when to give up waiting.
        onDmSent: {
            for (var i = 0; i < messageList.count; ++i) {
                var m = messageList.get(i)
                if (m.dm === keyHex && m.status === "sending") {
                    messageList.setProperty(i, "ack", ackHex)
                    messageList.setProperty(i, "status", "sent")
                    messageList.setProperty(i, "sentAt",
                                            Date.now() + Math.max(timeoutMs, 10000) + 5000)
                    return
                }
            }
        }
        onDmDelivered: {
            for (var i = messageList.count - 1; i >= 0; --i) {
                var m = messageList.get(i)
                if (m.ack === ackHex && m.dm.length) {
                    messageList.setProperty(i, "status", "delivered")
                    return
                }
            }
        }
        // History is keyed by slot index: when a slot gets a new channel or
        // is emptied, the old channel's messages must not show up under it.
        onChannelSlotCleared: rootWindow.clearChannelHistory(channelIdx)
    }

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

    function clearChannelHistory(idx) {
        for (var i = messageList.count - 1; i >= 0; --i) {
            if (messageList.get(i).channel === idx)
                messageList.remove(i)
        }
        MessageStore.clear(idx)
        markRead(idx)
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
                             time: timeText(ts), route: r, routeDetail: rd,
                             dm: "", status: "", ack: "", sentAt: 0 })
        if (persist)
            MessageStore.append({ channel: channel, sender: sender, text: text, ts: ts,
                                  route: r, routeDetail: rd })
    }

    // --- unread counters ------------------------------------------------
    // unread = { "<channel idx>": count }. Reassigned (never mutated in place)
    // so every binding that reads it re-evaluates.
    property var unread: ({})
    // Channel unread (keys "<idx>") and direct-message unread (keys
    // "dm:<key>") share the map but are counted separately.
    readonly property int unreadTotal: {
        var n = 0
        for (var k in unread)
            if (k.indexOf("dm:") !== 0) n += unread[k]
        return n
    }
    readonly property int dmUnreadTotal: {
        var n = 0
        for (var k in unread)
            if (k.indexOf("dm:") === 0) n += unread[k]
        return n
    }
    readonly property int contactsPageIndex: 4
    property string openDmKey: ""      // chat open on the contacts page
    property var messageModel: messageList

    function dmUnreadFor(key) {
        return unread["dm:" + key] || 0
    }
    function isReadingDm(key) {
        return mainStack.currentIndex === contactsPageIndex && openDmKey === key
                && Qt.application.state === Qt.ApplicationActive
    }
    function markDmRead(key) {
        markRead("dm:" + key)
    }

    // Direct message row. status: "" (received), "sending", "sent",
    // "delivered", "unconfirmed". Rows carry channel -1, so channel views
    // never show them.
    function addDm(key, sender, text, persist, ts, status) {
        if (!ts)
            ts = Math.floor(Date.now() / 1000)
        // sentAt: negative = "sending since", positive = "give up waiting at".
        messageList.append({ channel: -1, sender: sender, text: text, time: timeText(ts),
                             route: "", routeDetail: "", dm: key,
                             status: status || "", ack: "",
                             sentAt: status === "sending" ? -Date.now() : 0 })
        if (persist)
            MessageStore.append({ channel: -1, dm: key, sender: sender, text: text, ts: ts })
    }

    // Sent DMs not confirmed in time are marked; MeshCore retries are up to
    // the user (send again).
    Timer {
        interval: 5000
        repeat: true
        running: true
        onTriggered: {
            var now = Date.now()
            for (var i = 0; i < messageList.count; ++i) {
                var m = messageList.get(i)
                // Node confirmed the send, but no ACK from the recipient in time.
                if (m.status === "sent" && m.sentAt > 0 && now > m.sentAt)
                    messageList.setProperty(i, "status", "unconfirmed")
                // The node never even acknowledged the send.
                else if (m.status === "sending" && m.sentAt < 0 && now + m.sentAt > 15000)
                    messageList.setProperty(i, "status", "unconfirmed")
            }
        }
    }
    readonly property int messagesPageIndex: 3

    function unreadFor(idx) {
        return unread[String(idx)] || 0
    }

    // Battery of the connected node, e.g. "Battery: 78% (3.95 V)".
    function batteryText() {
        var mv = BluetoothManager.batteryMilliVolts
        if (mv <= 0)
            return ""
        return qsTr("Battery: %1% (%2 V)").arg(BluetoothManager.batteryPercent)
                                          .arg((mv / 1000).toFixed(2))
    }

    function batteryColor() {
        var p = BluetoothManager.batteryPercent
        return p < 0 ? "#555" : p <= 15 ? "#b00" : p <= 35 ? "#e65100" : "#2e7d32"
    }

    // Unread in all channels except the one open on the Messages page.
    function unreadElsewhere() {
        return unreadTotal - unreadFor(currentChannelIdx)
    }

    function isReading(idx) {
        return mainStack.currentIndex === messagesPageIndex
                && idx === currentChannelIdx
                && Qt.application.state === Qt.ApplicationActive
    }

    function bumpUnread(idx) {
        var u = Object.assign({}, unread)
        u[String(idx)] = (u[String(idx)] || 0) + 1
        unread = u
        MessageStore.setUnreadCounts(u)
    }

    function markRead(idx) {
        if (!unreadFor(idx))
            return
        var u = Object.assign({}, unread)
        delete u[String(idx)]
        unread = u
        MessageStore.setUnreadCounts(u)
    }

    // Opening a channel (or coming back to the app on it) marks it read.
    function markCurrentReadIfVisible() {
        if (isReading(currentChannelIdx))
            markRead(currentChannelIdx)
        if (openDmKey.length && isReadingDm(openDmKey))
            markDmRead(openDmKey)
    }
    Connections {
        target: Qt.application
        onStateChanged: rootWindow.markCurrentReadIfVisible()
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
    onCurrentChannelIdxChanged: {
        saveLastChannel()
        markCurrentReadIfVisible()
    }
    onCurrentChannelNameChanged: saveLastChannel()

    Component.onCompleted: {
        // Restore the history first, then the last opened channel.
        var stored = MessageStore.loadAll()
        for (var i = 0; i < stored.length; ++i) {
            var m = stored[i]
            messageList.append({ channel: m.dm ? -1 : m.channel, sender: m.sender, text: m.text,
                                 time: m.ts ? timeText(m.ts) : (m.time || ""),
                                 route: m.route || "", routeDetail: m.routeDetail || "",
                                 dm: m.dm || "", status: m.status || "", ack: "", sentAt: 0 })
        }
        unread = MessageStore.unreadCounts()
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
        onCurrentIndexChanged: rootWindow.markCurrentReadIfVisible()
        
        // Page 1: Node Discovery
        Page {
            id: nodeDiscoveryPage
            title: qsTr("Discover MeshCore Nodes")
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit

                AppLabel {
                    text: qsTr("Scan for Bluetooth devices")
                    font.bold: true
                    font.pixelSize: rootWindow.fontTitle
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    text: BluetoothManager.scanActive ? qsTr("Stop Scan") : qsTr("Start Scan")
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
                          ? qsTr("Connected to: %1").arg(BluetoothManager.connectedDevice)
                          : qsTr("%1 device(s) found").arg(BluetoothManager.devicesModel.count)
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
                                      + (model.lowEnergy ? "  ·  BLE" : "  ·  " + qsTr("Classic"))
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
            title: qsTr("Connect to Node")
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: qsTr("Selected Node: %1")
                          .arg(BluetoothManager.hasSelection ? BluetoothManager.selectedName
                                                             : qsTr("None"))
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
                    text: BluetoothManager.connected ? qsTr("Disconnect") : qsTr("Connect")
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
                          ? qsTr("Node: %1").arg(BluetoothManager.nodeName || qsTr("(unnamed)"))
                          : ""
                    visible: BluetoothManager.sessionReady
                    font.pixelSize: rootWindow.fontBody
                    color: "#2e7d32"
                    Layout.fillWidth: true
                }

                AppLabel {
                    text: rootWindow.batteryText()
                    visible: BluetoothManager.sessionReady && text.length > 0
                    font.pixelSize: rootWindow.fontBody
                    color: rootWindow.batteryColor()
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
                    text: qsTr("Channels")
                    enabled: BluetoothManager.uartReady
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 2
                    UnreadBadge {
                        count: rootWindow.unreadTotal
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
                    onClicked: mainStack.currentIndex = 4
                    // Unread direct messages.
                    UnreadBadge {
                        count: rootWindow.dmUnreadTotal
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: -width / 3
                        anchors.topMargin: -height / 3
                    }
                }

                Button {
                    text: qsTr("Back")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 0
                }
            }
            

        }
        
        // Page 3: Channel Selection
        Page {
            id: channelPage
            title: qsTr("MeshCore Channels")
            
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
                        text: qsTr("Connected to: %1")
                              .arg(BluetoothManager.uartReady ? BluetoothManager.selectedName
                                                              : qsTr("not connected"))
                        font.bold: true
                        font.pixelSize: rootWindow.fontBody
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    AppField {
                        id: channelInput
                        placeholderText: qsTr("Channel name, e.g. #p53")
                        font.pixelSize: rootWindow.fontBody
                        Layout.fillWidth: true
                        inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                        onAccepted: openOrAddButton.clicked()
                    }

                    // What the field refers to. A typed name that is not on the
                    // node must NOT fall back to the previously selected
                    // channel - that opened the wrong channel's messages.
                    property string typedName: BluetoothManager.normalizeChannelName(channelInput.text)
                    property int typedIdx: {
                        BluetoothManager.channelNames   // re-evaluate on changes
                        return BluetoothManager.channelIndexOf(typedName)
                    }

                    Button {
                        id: openOrAddButton
                        readonly property bool typing: channelInput.text.trim().length > 0
                        text: !typing
                              ? (rootWindow.currentChannelIdx >= 0
                                 ? qsTr("Open %1").arg(rootWindow.currentChannelName) : qsTr("Pick a channel"))
                              : channelColumn.typedName.length === 0
                                ? qsTr("Invalid channel name")
                                : channelColumn.typedIdx >= 0
                                  ? qsTr("Open %1").arg(channelColumn.typedName)
                                  : qsTr("Add %1").arg(channelColumn.typedName)
                        enabled: !typing ? rootWindow.currentChannelIdx >= 0
                                         : channelColumn.typedName.length > 0
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.round(rootWindow.gu * 22)
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: {
                            var idx = rootWindow.currentChannelIdx
                            var name = rootWindow.currentChannelName
                            if (typing) {
                                name = channelColumn.typedName
                                idx = channelColumn.typedIdx
                                if (idx < 0)
                                    idx = BluetoothManager.addChannel(name)   // reason via sendFailed
                                if (idx < 0) {
                                    channelStatus.text = channelStatus.lastError
                                    return
                                }
                            }
                            rootWindow.currentChannelIdx = idx
                            rootWindow.currentChannelName = name
                            channelInput.text = ""
                            channelStatus.text = ""
                            Qt.inputMethod.hide()
                            mainStack.currentIndex = 3
                        }
                    }

                    AppLabel {
                        id: channelStatus
                        property string lastError: ""
                        Layout.fillWidth: true
                        visible: text.length > 0
                        color: "#b00"
                        wrapMode: Text.Wrap
                        font.pixelSize: rootWindow.fontSecondary
                        Connections {
                            target: BluetoothManager
                            onSendFailed: channelStatus.lastError = reason
                        }
                    }

                    // As soon as the node's channel table arrives, preselect
                    // the first entry so the page is immediately usable.
                    Connections {
                        target: BluetoothManager
                        onChannelsChanged: {
                            var names = BluetoothManager.channelNames
                            var cur = rootWindow.currentChannelIdx
                            // Slots are still being read after a connect: an
                            // unread slot is not "empty", leave the selection.
                            if (cur >= 0 && cur >= names.length)
                                return
                            var curValid = cur >= 0 && names[cur].length > 0
                            if (curValid) {
                                // Keep the name in sync with the node.
                                rootWindow.currentChannelName = names[cur]
                                return
                            }
                            // No selection, or its slot was emptied:
                            // fall back to the first channel the node has.
                            for (var i = 0; i < names.length; ++i) {
                                if (names[i].length > 0) {
                                    rootWindow.currentChannelIdx = i
                                    rootWindow.currentChannelName = names[i]
                                    return
                                }
                            }
                        }
                    }

                    AppLabel {
                        text: {
                            var names = BluetoothManager.channelNames
                            if (names.length === 0)
                                return qsTr("Waiting for the channel list from the node...")
                            var used = 0
                            for (var i = 0; i < names.length; ++i)
                                if (names[i].length > 0) ++used
                            return BluetoothManager.maxChannels > 0
                                    ? qsTr("Channels on %1 (%2 of %3 slots used):")
                                      .arg(BluetoothManager.nodeName).arg(used)
                                      .arg(BluetoothManager.maxChannels)
                                    : qsTr("Channels on %1:").arg(BluetoothManager.nodeName)
                        }
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
                            // Empty slots are part of the list (so free slots
                            // are known) but are not shown.
                            visible: String(modelData).length > 0
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
                                anchors.right: channelBadge.left
                                anchors.margins: rootWindow.spacingUnit
                                spacing: Math.round(rootWindow.gu * 0.2)
                                AppLabel {
                                    text: String(modelData)
                                    font.bold: true
                                    font.pixelSize: rootWindow.fontTitle
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                                AppLabel {
                                    visible: rootWindow.unreadFor(index) > 0
                                    text: rootWindow.unreadFor(index) === 1
                                          ? qsTr("1 new message")
                                          : qsTr("%1 new messages").arg(rootWindow.unreadFor(index))
                                    color: "#d32f2f"
                                    font.pixelSize: rootWindow.fontSecondary
                                }
                            }
                            UnreadBadge {
                                id: channelBadge
                                count: rootWindow.unreadFor(index)
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: removeChannelButton.left
                                anchors.rightMargin: rootWindow.spacingUnit
                            }
                            // Remove. z: 2 keeps it above the row-wide
                            // MouseArea declared below it.
                            Rectangle {
                                id: removeChannelButton
                                z: 2
                                width: Math.round(rootWindow.gu * 4.5)
                                height: width
                                radius: width / 2
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: rootWindow.spacingUnit
                                color: removeMouse.pressed ? "#ffcdd2" : "transparent"
                                border.color: "#e57373"
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: "✕"
                                    color: "#c62828"
                                    font.pixelSize: Math.round(rootWindow.gu * 2)
                                }
                                MouseArea {
                                    id: removeMouse
                                    anchors.fill: parent
                                    onClicked: {
                                        removeDialog.channelIdx = index
                                        removeDialog.channelName = String(modelData)
                                        removeDialog.open()
                                    }
                                }
                            }
                            MouseArea {
                                id: channelMouse
                                anchors.fill: parent
                                // Tap opens the channel directly.
                                onClicked: {
                                    rootWindow.currentChannelIdx = index
                                    rootWindow.currentChannelName = String(modelData)
                                    channelInput.text = ""
                                    channelStatus.text = ""
                                    Qt.inputMethod.hide()
                                    mainStack.currentIndex = 3
                                }
                            }
                        }
                    }

                    Button {
                        text: qsTr("Back")
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: mainStack.currentIndex = 1
                    }
                }
            }
            
            property string connectedNode: ""

            // Removing a channel changes the node's configuration, so ask.
            Dialog {
                id: removeDialog
                property int channelIdx: -1
                property string channelName: ""
                modal: true
                width: Math.min(parent.width - 2 * rootWindow.spacingUnit,
                                Math.round(rootWindow.gu * 40))
                x: (parent.width - width) / 2
                y: (parent.height - height) / 3
                title: qsTr("Remove channel")
                standardButtons: Dialog.Yes | Dialog.Cancel
                // Text in a plain Item: keeps the dialog height from feeding
                // back into the wrapping width ("binding loop" warning).
                contentItem: Item {
                    implicitHeight: removeText.implicitHeight
                    AppLabel {
                        id: removeText
                        width: parent.width
                        wrapMode: Text.Wrap
                        text: qsTr("Remove %1 from your node?\n\n"
                                   + "Its message history in the app is deleted too. You can add "
                                   + "the channel again later by typing its name.")
                              .arg(removeDialog.channelName)
                    }
                }
                onAccepted: {
                    if (BluetoothManager.removeChannel(channelIdx))
                        channelStatus.text = ""
                    else
                        channelStatus.text = channelStatus.lastError
                }
            }
        }
        
        // Page 4: Messages
        Page {
            id: messagesPage
            title: qsTr("Messages - %1").arg(rootWindow.currentChannelName
                                    ? rootWindow.currentChannelName : qsTr("No Channel"))
            
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
                        text: qsTr("‹ Channels")
                        Layout.preferredHeight: Math.round(rootWindow.gu * 5)
                        onClicked: {
                            messageInput.focus = false
                            Qt.inputMethod.hide()
                            mainStack.currentIndex = 2
                        }
                        // New messages waiting in OTHER channels.
                        UnreadBadge {
                            count: rootWindow.unreadElsewhere()
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
                            text: rootWindow.currentChannelName || qsTr("No channel")
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
                        placeholderText: qsTr("Enter message...")
                        font.pixelSize: rootWindow.fontBody
                        Layout.fillWidth: true
                    }
                    
                    Button {
                        text: qsTr("Send")
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
                                rootWindow.addLocalMessage("!", qsTr("Nothing typed yet"))
                                return
                            }
                            if (!BluetoothManager.sessionReady) {
                                rootWindow.addLocalMessage("!", qsTr("No node session - connect first"))
                                return
                            }
                            if (rootWindow.currentChannelIdx < 0) {
                                rootWindow.addLocalMessage("!", qsTr("No channel selected"))
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
                        text: qsTr("Refresh")
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
                        text: qsTr("No messages in this channel yet")
                        color: "#888"
                        font.pixelSize: rootWindow.fontSecondary
                    }

                    delegate: Item {
                        id: messageDelegate
                        // Only the selected channel is shown; other rows
                        // collapse to zero height but stay in the model.
                        // Direct messages (dm set, channel -1) never show
                        // here, even when no channel is selected (idx -1).
                        readonly property bool shown: model.channel === rootWindow.currentChannelIdx
                                                      && model.dm === ""
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
                        // Received messages only - not our own or status lines.
                        readonly property bool replyable: model.sender !== "You" && model.sender !== "!"
                        Rectangle {
                            anchors.fill: parent
                            z: -1
                            color: messageMouse.pressed && messageDelegate.replyable
                                   ? "#e3edf9" : "transparent"
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
                            onTriggered: rootWindow.startReply(messageMenu.sender, "")
                        }
                        MenuItem {
                            text: qsTr("Reply with route")
                            enabled: messageMenu.route.length > 0
                            onTriggered: rootWindow.startReply(
                                             messageMenu.sender,
                                             rootWindow.compactRoute(messageMenu.route))
                        }
                    }
                }
            }
            
        }
        
        // Page 4: Contacts
        Page {
            id: contactsPage
            title: qsTr("Contacts")
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                ContactList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    app: rootWindow     // message model + DM helpers for the chat page
                }
                
                Button {
                    text: qsTr("Back")
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
            title: qsTr("Settings")
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: qsTr("App Settings")
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: qsTr("Node Administration")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 6
                }
                
                Button {
                    text: qsTr("About")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 7
                }
                
                Button {
                    text: qsTr("Back")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 0
                }
            }
        }
        
        // Page 6: Node Admin
        Page {
            id: adminPage
            title: qsTr("Node Administration")
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: rootWindow.spacingUnit
                spacing: rootWindow.spacingUnit
                
                AppLabel {
                    text: qsTr("Administer connected node")
                    font.bold: true
                    Layout.alignment: Qt.AlignHCenter
                }
                
                AppField {
                    placeholderText: qsTr("Admin Password")
                    echoMode: TextInput.Password
                    Layout.fillWidth: true
                }
                
                Button {
                    text: qsTr("Send Command")
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: qsTr("Back")
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: mainStack.currentIndex = 5
                }
            }
        }
        
        // Page 7: About
        Page {
            id: aboutPage
            title: qsTr("About ubMeshcore")
            
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
                    text: qsTr("Version %1").arg(Qt.application.version)
                    Layout.alignment: Qt.AlignHCenter
                }
                
                AppLabel {
                    text: qsTr("Bluetooth companion app for MeshCore nodes on Ubuntu Touch")
                    Layout.alignment: Qt.AlignHCenter
                    wrapMode: Text.Wrap
                }
                
                AppLabel {
                    text: qsTr("Based on meshcore-info-display project")
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: qsTr("Back")
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
            text: qsTr("Nodes")
            onClicked: mainStack.currentIndex = 0
        }
        ToolButton {
            text: qsTr("Channels")
            onClicked: mainStack.currentIndex = 2
            UnreadBadge {
                count: rootWindow.unreadTotal
                anchors.right: parent.right
                anchors.top: parent.top
            }
        }
        ToolButton {
            text: qsTr("Messages")
            onClicked: mainStack.currentIndex = 3
        }
        ToolButton {
            text: qsTr("Contacts")
            onClicked: mainStack.currentIndex = 4
            UnreadBadge {
                count: rootWindow.dmUnreadTotal
                anchors.right: parent.right
                anchors.top: parent.top
            }
        }
        ToolButton {
            text: qsTr("Settings")
            onClicked: mainStack.currentIndex = 5
        }
    }
}

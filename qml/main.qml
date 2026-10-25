// Ubuntu Touch 20.04 ships Qt 5.12 - importing 2.15 makes the whole file fail
// to load. 2.12 is the highest version actually available on the device.
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

// Application window. Holds the state that several pages share (message
// history, unread counters, the open channel) and the page stack. The pages
// themselves live in their own files and get this window as `app`.
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

    // --- Navigation --------------------------------------------------------
    // Position of each page in mainStack below.
    readonly property int pageDevices:    0
    readonly property int pageConnection: 1
    readonly property int pageChannels:   2
    readonly property int pageChannel:    3
    readonly property int pageContacts:   4

    function showPage(page) {
        mainStack.currentIndex = page
    }

    // --- Shared state ------------------------------------------------------
    // Currently open channel (index is what the companion protocol wants).
    property int currentChannelIdx: -1
    property string currentChannelName: ""

    // Direct message chat open on the contacts page ("" = none).
    property string openDmKey: ""

    // Every channel message and direct message, see addMessage() / addDm().
    property var messageModel: messageList
    ListModel { id: messageList }

    // Unread counts: { "<channel idx>": n, "dm:<contact key>": n }.
    // Reassigned (never mutated in place) so every binding that reads it
    // re-evaluates.
    property var unread: ({})

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

    // --- Messages ----------------------------------------------------------
    // Every message carries the channel it belongs to; the channel page only
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

    // Our own messages ("You") and status/error lines ("!") in the open
    // channel. "!" entries are transient and are not stored.
    function addLocalMessage(sender, text) {
        addMessage(currentChannelIdx, sender, text, sender !== "!", 0, null)
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

    function clearChannelHistory(idx) {
        for (var i = messageList.count - 1; i >= 0; --i) {
            if (messageList.get(i).channel === idx)
                messageList.remove(i)
        }
        MessageStore.clear(idx)
        markRead(idx)
    }

    // --- Unread counters ---------------------------------------------------
    function unreadFor(idx) {
        return unread[String(idx)] || 0
    }

    function dmUnreadFor(key) {
        return unread["dm:" + key] || 0
    }

    // Unread in all channels except the one that is open.
    function unreadElsewhere() {
        return unreadTotal - unreadFor(currentChannelIdx)
    }

    function isReading(idx) {
        return mainStack.currentIndex === pageChannel
                && idx === currentChannelIdx
                && Qt.application.state === Qt.ApplicationActive
    }

    function isReadingDm(key) {
        return mainStack.currentIndex === pageContacts && openDmKey === key
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

    function markDmRead(key) {
        markRead("dm:" + key)
    }

    // Opening a channel or chat (or coming back to the app on it) marks it read.
    function markCurrentReadIfVisible() {
        if (isReading(currentChannelIdx))
            markRead(currentChannelIdx)
        if (openDmKey.length && isReadingDm(openDmKey))
            markDmRead(openDmKey)
    }

    // --- Node battery ------------------------------------------------------
    // e.g. "Battery: 78% (3.95 V)".
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

    // --- Events from the node ----------------------------------------------
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

        // History is keyed by slot index: when a slot gets a new channel or
        // is emptied, the old channel's messages must not show up under it.
        onChannelSlotCleared: rootWindow.clearChannelHistory(channelIdx)

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

    Connections {
        target: Qt.application
        onStateChanged: rootWindow.markCurrentReadIfVisible()
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

    // --- Pages -------------------------------------------------------------
    // Order must match the page* constants above.
    StackLayout {
        id: mainStack
        anchors.fill: parent
        currentIndex: rootWindow.pageDevices
        onCurrentIndexChanged: rootWindow.markCurrentReadIfVisible()

        DevicesPage { app: rootWindow }
        ConnectionPage { app: rootWindow }
        ChannelsPage { app: rootWindow }
        ChannelPage { app: rootWindow }
        ContactsPage { app: rootWindow }
    }
}

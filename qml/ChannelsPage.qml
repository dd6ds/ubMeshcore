import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Third page: the node's channel table. Open a channel, add a hashtag
// channel by name, or remove one.
Page {
    id: page
    property var app                 // main.qml root (sizes, navigation, open channel)

    title: qsTr("MeshCore Channels")

    // What the input field refers to. A typed name that is not on the node
    // must NOT fall back to the currently selected channel, or the wrong
    // channel's messages would open.
    readonly property string typedName: BluetoothManager.normalizeChannelName(channelInput.text)
    readonly property int typedIdx: {
        BluetoothManager.channelNames   // re-evaluate on changes
        return BluetoothManager.channelIndexOf(typedName)
    }

    function openChannel(idx, name) {
        app.currentChannelIdx = idx
        app.currentChannelName = name
        channelInput.text = ""
        channelStatus.text = ""
        Qt.inputMethod.hide()
        app.showPage(app.pageChannel)
    }

    // As soon as the node's channel table arrives, preselect the first entry
    // so the page is immediately usable.
    Connections {
        target: BluetoothManager
        onChannelsChanged: {
            var names = BluetoothManager.channelNames
            var cur = app.currentChannelIdx
            // Slots are still being read after a connect: an unread slot is
            // not "empty", leave the selection.
            if (cur >= 0 && cur >= names.length)
                return
            if (cur >= 0 && names[cur].length > 0) {
                // Keep the name in sync with the node.
                app.currentChannelName = names[cur]
                return
            }
            // No selection, or its slot was emptied: fall back to the first
            // channel the node has.
            for (var i = 0; i < names.length; ++i) {
                if (names[i].length > 0) {
                    app.currentChannelIdx = i
                    app.currentChannelName = names[i]
                    return
                }
            }
        }
        onSendFailed: channelStatus.lastError = reason
    }

    // A Flickable instead of a stretched ColumnLayout: a ListView has a
    // viewport that has to be sized correctly, and whatever did not fit ended
    // up underneath the buttons below it. A Repeater has no viewport, so every
    // row is simply laid out, and the Flickable lets the page scroll if it
    // gets taller than the screen.
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: channelColumn.implicitHeight + 2 * app.spacingUnit
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: channelColumn
            x: app.spacingUnit
            y: app.spacingUnit
            width: parent.width - 2 * app.spacingUnit
            spacing: app.spacingUnit

            AppLabel {
                text: qsTr("Connected to: %1")
                      .arg(BluetoothManager.uartReady ? BluetoothManager.selectedName
                                                      : qsTr("not connected"))
                font.bold: true
                font.pixelSize: app.fontBody
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            AppField {
                id: channelInput
                placeholderText: qsTr("Channel name, e.g. #p53")
                font.pixelSize: app.fontBody
                Layout.fillWidth: true
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                onAccepted: openOrAddButton.clicked()
            }

            // Empty field: reopen the current channel. Typed name: open it if
            // the node has it, otherwise add it to the node first.
            Button {
                id: openOrAddButton
                readonly property bool typing: channelInput.text.trim().length > 0
                text: {
                    if (!typing)
                        return app.currentChannelIdx >= 0
                                ? qsTr("Open %1").arg(app.currentChannelName)
                                : qsTr("Pick a channel")
                    if (page.typedName.length === 0)
                        return qsTr("Invalid channel name")
                    return page.typedIdx >= 0 ? qsTr("Open %1").arg(page.typedName)
                                              : qsTr("Add %1").arg(page.typedName)
                }
                enabled: typing ? page.typedName.length > 0 : app.currentChannelIdx >= 0
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.round(app.gu * 22)
                Layout.preferredHeight: Math.round(app.gu * 5)
                onClicked: {
                    if (!typing) {
                        page.openChannel(app.currentChannelIdx, app.currentChannelName)
                        return
                    }
                    var idx = page.typedIdx
                    if (idx < 0)
                        idx = BluetoothManager.addChannel(page.typedName)  // reason via sendFailed
                    if (idx < 0) {
                        channelStatus.text = channelStatus.lastError
                        return
                    }
                    page.openChannel(idx, page.typedName)
                }
            }

            AppLabel {
                id: channelStatus
                property string lastError: ""
                Layout.fillWidth: true
                visible: text.length > 0
                color: "#b00"
                wrapMode: Text.Wrap
                font.pixelSize: app.fontSecondary
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
                font.pixelSize: app.fontSecondary
                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            Repeater {
                // Straight from the node - no placeholder rows, so an empty
                // list is visibly empty instead of showing invented channels
                // that cannot be opened.
                model: BluetoothManager.channelNames

                delegate: Rectangle {
                    readonly property bool current: app.currentChannelIdx === index

                    // Empty slots are part of the list (so free slots are
                    // known) but are not shown.
                    visible: String(modelData).length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.round(app.gu * 7)
                    radius: Math.round(app.gu * 0.5)
                    color: channelMouse.pressed ? "#b0d4ff" : (current ? "#e6f0ff" : "#f0f0f0")
                    border.color: current ? "#0078d7" : "#cccccc"
                    border.width: current ? 3 : 1

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: channelBadge.left
                        anchors.margins: app.spacingUnit
                        spacing: Math.round(app.gu * 0.2)
                        AppLabel {
                            text: String(modelData)
                            font.bold: true
                            font.pixelSize: app.fontTitle
                            elide: Text.ElideRight
                            width: parent.width
                        }
                        AppLabel {
                            visible: app.unreadFor(index) > 0
                            text: app.unreadFor(index) === 1
                                  ? qsTr("1 new message")
                                  : qsTr("%1 new messages").arg(app.unreadFor(index))
                            color: "#d32f2f"
                            font.pixelSize: app.fontSecondary
                        }
                    }

                    UnreadBadge {
                        id: channelBadge
                        count: app.unreadFor(index)
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: removeChannelButton.left
                        anchors.rightMargin: app.spacingUnit
                    }

                    // Remove button. z: 2 keeps it above the row-wide
                    // MouseArea declared below it.
                    Rectangle {
                        id: removeChannelButton
                        z: 2
                        width: Math.round(app.gu * 4.5)
                        height: width
                        radius: width / 2
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: app.spacingUnit
                        color: removeMouse.pressed ? "#ffcdd2" : "transparent"
                        border.color: "#e57373"
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "✕"
                            color: "#c62828"
                            font.pixelSize: Math.round(app.gu * 2)
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
                        onClicked: page.openChannel(index, String(modelData))
                    }
                }
            }

            Button {
                text: qsTr("Back")
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: Math.round(app.gu * 5)
                onClicked: app.showPage(app.pageConnection)
            }
        }
    }

    // Removing a channel changes the node's configuration, so ask.
    Dialog {
        id: removeDialog
        property int channelIdx: -1
        property string channelName: ""
        modal: true
        width: Math.min(parent.width - 2 * app.spacingUnit, Math.round(app.gu * 40))
        x: (parent.width - width) / 2
        y: (parent.height - height) / 3
        title: qsTr("Remove channel")
        standardButtons: Dialog.Yes | Dialog.Cancel
        // Text in a plain Item: keeps the dialog height from feeding back
        // into the wrapping width ("binding loop" warning).
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

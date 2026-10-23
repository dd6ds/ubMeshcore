import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/**
 * Live contact list of the connected MeshCore node.
 *
 * Entries come from the node's contact table, which the node fills from the
 * adverts it receives; new adverts refresh the list automatically. Tapping a
 * repeater or room server opens its admin page.
 */
Item {
    id: root

    readonly property real gu: gridUnitPx > 0 ? gridUnitPx : 10
    readonly property int fontBody: Math.round(gu * 1.6)
    readonly property int fontSecondary: Math.round(gu * 1.3)
    readonly property int spacingUnit: Math.round(gu * 1.0)

    // "all" | "repeater" | "chat" | "room"
    property string typeFilter: "all"
    property bool activeOnly: false
    property bool favoritesOnly: false
    property string searchText: ""

    function matches(type, name, keyPrefix, active, favorite) {
        if (favoritesOnly && !favorite)
            return false
        if (activeOnly && !active)
            return false
        if (typeFilter !== "all" && type !== typeFilter)
            return false
        var q = searchText.trim().toLowerCase()
        if (q.length === 0)
            return true
        return (name || "").toLowerCase().indexOf(q) !== -1
                || (keyPrefix || "").toLowerCase().indexOf(q) !== -1
    }

    function typeColor(type) {
        if (type === "repeater") return "#d32f2f"
        if (type === "room") return "#7b1fa2"
        if (type === "sensor") return "#00897b"
        return "#0078d7"
    }

    function typeLabel(type) {
        if (type === "repeater") return qsTr("Repeater")
        if (type === "room") return qsTr("Room")
        if (type === "chat") return qsTr("Companion")
        if (type === "sensor") return qsTr("Sensor")
        return qsTr("Unknown")
    }

    function hopsText(hops) {
        if (hops < 0) return qsTr("flood")
        if (hops === 0) return qsTr("direct")
        return hops === 1 ? qsTr("1 hop") : qsTr("%1 hops").arg(hops)
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: listPage
    }

    // main.qml root: shared message model and the direct-message helpers.
    property var app

    function openChat(keyHex, contactName) {
        stack.push(chatComponent, { app: root.app, keyHex: keyHex, contactName: contactName })
    }
    function openAdmin(keyHex, nodeName, nodeType) {
        stack.push(adminComponent, { keyHex: keyHex, nodeName: nodeName, nodeType: nodeType })
    }

    Component {
        id: chatComponent
        DirectChat {
            onBackRequested: stack.pop()
        }
    }

    // Long-press menu of a contact row.
    Menu {
        id: contactMenu
        property string keyHex: ""
        property string contactName: ""
        property string contactType: ""
        property bool favorite: false
        property bool onNode: true
        MenuItem {
            text: qsTr("Send message")
            // Direct messages go to companions; repeaters take CLI commands.
            visible: contactMenu.contactType === "chat"
            height: visible ? implicitHeight : 0
            onTriggered: root.openChat(contactMenu.keyHex, contactMenu.contactName)
        }
        MenuItem {
            text: qsTr("Manage")
            visible: contactMenu.contactType === "repeater" || contactMenu.contactType === "room"
            height: visible ? implicitHeight : 0
            onTriggered: root.openAdmin(contactMenu.keyHex, contactMenu.contactName,
                                        contactMenu.contactType)
        }
        MenuItem {
            text: contactMenu.favorite ? qsTr("Remove from favourites") : qsTr("Add to favourites")
            onTriggered: BluetoothManager.contactsModel.toggleFavorite(contactMenu.keyHex)
        }
        // The node's contact table is limited (e.g. 350 entries); these
        // manage what is stored there.
        MenuItem {
            text: qsTr("Save to node")
            visible: !contactMenu.onNode
            height: visible ? implicitHeight : 0
            onTriggered: BluetoothManager.saveContactToNode(contactMenu.keyHex)
        }
        MenuItem {
            text: qsTr("Delete contact")
            onTriggered: root.confirmDelete([contactMenu.keyHex], contactMenu.contactName)
        }
    }

    // --- deleting contacts ----------------------------------------------------
    // Single (long-press menu) and multiple (selection mode) deletes share one
    // confirmation dialog.
    function confirmDelete(keys, singleName) {
        deleteDialog.keys = keys
        deleteDialog.singleName = singleName
        deleteDialog.open()
    }

    property bool selectMode: false
    property var selected: ({})          // key -> true; reassigned on change
    readonly property int selectedCount: Object.keys(selected).length

    function toggleSelected(key) {
        var s = Object.assign({}, selected)
        if (s[key]) delete s[key]; else s[key] = true
        selected = s
    }
    function selectKeys(keys) {
        var s = Object.assign({}, selected)
        for (var i = 0; i < keys.length; ++i)
            s[keys[i]] = true
        selected = s
    }
    function endSelectMode() {
        selected = ({})
        selectMode = false
    }

    Dialog {
        id: deleteDialog
        property var keys: []
        property string singleName: ""
        modal: true
        width: Math.max(Math.round(root.gu * 20),
                        Math.min(root.width - 2 * root.spacingUnit, Math.round(root.gu * 40)))
        x: (root.width - width) / 2
        y: (root.height - height) / 3
        title: keys.length === 1 ? qsTr("Delete contact") : qsTr("Delete contacts")
        standardButtons: Dialog.Yes | Dialog.Cancel
        // The text sits in a plain Item with its own implicit size, so the
        // dialog's height does not feed back into the text's wrapping width
        // (that was the "binding loop" warning).
        contentItem: Item {
            implicitHeight: deleteText.implicitHeight
            AppLabel {
                id: deleteText
                width: parent.width
                wrapMode: Text.Wrap
                text: (deleteDialog.keys.length === 1
                       ? qsTr("Delete %1?").arg(deleteDialog.singleName)
                       : qsTr("Delete %1 contacts?").arg(deleteDialog.keys.length))
                      + "\n\n"
                      + qsTr("They are removed from your node and from this list. A contact "
                             + "shows up again the next time its advert is heard.")
            }
        }
        onAccepted: {
            BluetoothManager.deleteContacts(keys)
            root.endSelectMode()
        }
    }

    // Errors from saving/removing contacts (e.g. the node's list is full).
    property string lastError: ""
    Connections {
        target: BluetoothManager
        onSendFailed: root.lastError = reason
    }

    Component {
        id: adminComponent
        RepeaterAdmin {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: listPage

        ColumnLayout {
            spacing: root.spacingUnit

            // Header: count + refresh + advert
            RowLayout {
                Layout.fillWidth: true
                spacing: root.spacingUnit

                AppLabel {
                    Layout.fillWidth: true
                    font.pixelSize: root.fontSecondary
                    wrapMode: Text.Wrap
                    text: !BluetoothManager.sessionReady
                          ? qsTr("Connect to your node to load contacts")
                          : BluetoothManager.contactsSyncing
                            ? qsTr("Loading contacts...")
                            : BluetoothManager.maxContacts > 0
                              ? qsTr("%1 contact(s) · %2 of %3 stored on your node")
                                .arg(BluetoothManager.contactsModel.count)
                                .arg(BluetoothManager.contactsModel.nodeCount)
                                .arg(BluetoothManager.maxContacts)
                              : qsTr("%1 contact(s) from received adverts").arg(BluetoothManager.contactsModel.count)
                }
                Button {
                    text: qsTr("Refresh")
                    enabled: BluetoothManager.sessionReady && !BluetoothManager.contactsSyncing
                    onClicked: BluetoothManager.refreshContacts()
                }
                Button {
                    text: qsTr("Advert")
                    enabled: BluetoothManager.sessionReady
                    onClicked: BluetoothManager.sendAdvert(true)
                }
                Button {
                    text: root.selectMode ? qsTr("Done") : qsTr("Select")
                    enabled: BluetoothManager.sessionReady || root.selectMode
                    onClicked: root.selectMode ? root.endSelectMode() : (root.selectMode = true)
                }
            }

            // Selection mode: tap contacts to select, then delete them together.
            Rectangle {
                Layout.fillWidth: true
                visible: root.selectMode
                implicitHeight: selectRow.implicitHeight + Math.round(root.gu * 1.0)
                radius: 4
                color: "#fff8e1"
                border.color: "#ffb300"
                RowLayout {
                    id: selectRow
                    anchors.fill: parent
                    anchors.margins: Math.round(root.gu * 0.5)
                    spacing: root.spacingUnit
                    AppLabel {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.pixelSize: root.fontSecondary
                        text: root.selectedCount === 0
                              ? qsTr("Tap contacts to select them")
                              : qsTr("%1 selected").arg(root.selectedCount)
                    }
                    Button {
                        id: quickSelectButton
                        text: qsTr("Select ▾")
                        font.pixelSize: root.fontSecondary
                        onClicked: quickSelectMenu.popup(quickSelectButton, 0, quickSelectButton.height)
                    }
                    // Quick selections. Favourites are kept out of every one
                    // except "All" - deleting them should always be deliberate.
                    Menu {
                        id: quickSelectMenu
                        MenuItem {
                            text: qsTr("All except favourites")
                            onTriggered: root.selectKeys(
                                             BluetoothManager.contactsModel.allKeys(true))
                        }
                        MenuItem {
                            text: qsTr("Inactive for 7 days")
                            onTriggered: root.selectKeys(
                                             BluetoothManager.contactsModel.inactiveKeys(7, true))
                        }
                        MenuItem {
                            text: qsTr("Inactive for 30 days")
                            onTriggered: root.selectKeys(
                                             BluetoothManager.contactsModel.inactiveKeys(30, true))
                        }
                        MenuItem {
                            text: qsTr("All (including favourites)")
                            onTriggered: root.selectKeys(
                                             BluetoothManager.contactsModel.allKeys(false))
                        }
                        MenuItem {
                            text: qsTr("Clear selection")
                            enabled: root.selectedCount > 0
                            onTriggered: root.selected = ({})
                        }
                    }
                    Button {
                        text: qsTr("Delete")
                        enabled: root.selectedCount > 0
                        onClicked: root.confirmDelete(Object.keys(root.selected), "")
                    }
                }
            }

            // Last error from saving/removing a contact - tap to dismiss.
            Rectangle {
                Layout.fillWidth: true
                visible: root.lastError.length > 0
                implicitHeight: contactErrorLabel.implicitHeight + Math.round(root.gu * 1.0)
                radius: 4
                color: "#ffebee"
                border.color: "#e57373"
                AppLabel {
                    id: contactErrorLabel
                    anchors.fill: parent
                    anchors.margins: Math.round(root.gu * 0.5)
                    wrapMode: Text.Wrap
                    font.pixelSize: root.fontSecondary
                    color: "#b71c1c"
                    text: root.lastError
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.lastError = ""
                }
            }

            // Result of the last "Advert" tap - without it there was no way to
            // tell whether the node actually sent anything.
            Rectangle {
                Layout.fillWidth: true
                visible: BluetoothManager.lastAdvertInfo.length > 0
                implicitHeight: advertLabel.implicitHeight + Math.round(root.gu * 1.0)
                radius: 4
                color: BluetoothManager.lastAdvertOk ? "#e8f5e9" : "#ffebee"
                border.color: BluetoothManager.lastAdvertOk ? "#81c784" : "#e57373"
                AppLabel {
                    id: advertLabel
                    anchors.fill: parent
                    anchors.margins: Math.round(root.gu * 0.5)
                    wrapMode: Text.Wrap
                    font.pixelSize: root.fontSecondary
                    color: BluetoothManager.lastAdvertOk ? "#1b5e20" : "#b71c1c"
                    text: BluetoothManager.lastAdvertInfo
                          + (BluetoothManager.lastAdvertInfo.indexOf("✓") === 0
                             ? qsTr(" - nodes and repeaters in range will list you after receiving it.")
                             : "")
                }
            }

            AppField {
                Layout.fillWidth: true
                placeholderText: qsTr("Search name or key...")
                text: root.searchText
                onTextChanged: root.searchText = text
            }

            // Type filter
            RowLayout {
                Layout.fillWidth: true
                spacing: Math.round(root.gu * 0.5)
                Repeater {
                    model: [ { k: "all", t: qsTr("All") }, { k: "repeater", t: qsTr("Repeaters") },
                             { k: "chat", t: qsTr("Companions") }, { k: "room", t: qsTr("Rooms") } ]
                    delegate: Button {
                        text: modelData.t
                        checkable: true
                        autoExclusive: true
                        checked: root.typeFilter === modelData.k
                        font.pixelSize: root.fontSecondary
                        Layout.fillWidth: true
                        onClicked: root.typeFilter = modelData.k
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                CheckBox {
                    checked: root.favoritesOnly
                    onToggled: root.favoritesOnly = checked
                }
                AppLabel {
                    Layout.fillWidth: true
                    text: qsTr("★ Only favourites")
                    font.pixelSize: root.fontSecondary
                    wrapMode: Text.Wrap
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.favoritesOnly = !root.favoritesOnly
                    }
                }
                CheckBox {
                    checked: root.activeOnly
                    onToggled: root.activeOnly = checked
                }
                AppLabel {
                    Layout.fillWidth: true
                    text: qsTr("Only active (24 h)")
                    font.pixelSize: root.fontSecondary
                    wrapMode: Text.Wrap
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.activeOnly = !root.activeOnly
                    }
                }
            }

            ListView {
                id: contactList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: BluetoothManager.contactsModel

                AppLabel {
                    anchors.centerIn: parent
                    width: parent.width * 0.85
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: "#888"
                    font.pixelSize: root.fontSecondary
                    visible: (contactList.count === 0 && BluetoothManager.sessionReady
                              && !BluetoothManager.contactsSyncing)
                             || (root.favoritesOnly && contactList.count > 0
                                 && contactList.contentHeight < 1)
                    text: contactList.count > 0
                          ? qsTr("No favourites yet. Tap the ☆ on a contact to mark it.")
                          : qsTr("No contacts yet. Repeaters and nodes appear here as soon as "
                                 + "your node receives their adverts. Tap \"Advert\" to announce "
                                 + "yourself.")
                }

                delegate: Item {
                    // Non-matching rows collapse instead of leaving the model.
                    readonly property bool shown: root.matches(type, name, keyPrefix, isActive, isFavorite)
                    readonly property bool manageable: type === "repeater" || type === "room"
                    width: ListView.view ? ListView.view.width : 0
                    height: shown ? card.height + Math.round(root.gu * 0.6) : 0
                    visible: shown

                    Rectangle {
                        id: card
                        width: parent.width
                        height: Math.round(root.gu * 7.5)
                        radius: Math.round(root.gu * 0.5)
                        readonly property bool isSelected: root.selectMode && root.selected[publicKey] === true
                        color: isSelected ? "#ffe0b2" : (tap.pressed ? "#d6e8ff" : "#f5f5f5")
                        border.color: root.typeColor(type)
                        border.width: 2
                        opacity: isActive ? 1.0 : 0.6

                        RowLayout {
                            z: 1
                            anchors.fill: parent
                            anchors.margins: Math.round(root.gu * 0.8)
                            spacing: root.spacingUnit

                            // Selection box (selection mode only).
                            Rectangle {
                                visible: root.selectMode
                                Layout.preferredWidth: Math.round(root.gu * 2.6)
                                Layout.preferredHeight: Layout.preferredWidth
                                radius: 4
                                border.color: "#e65100"
                                border.width: 2
                                color: card.isSelected ? "#e65100" : "white"
                                Text {
                                    anchors.centerIn: parent
                                    visible: card.isSelected
                                    text: "✓"
                                    color: "white"
                                    font.bold: true
                                    font.pixelSize: Math.round(root.gu * 1.8)
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: Math.round(root.gu * 1.2)
                                Layout.preferredHeight: Layout.preferredWidth
                                radius: width / 2
                                color: isActive ? "#2e7d32" : "#9e9e9e"
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                AppLabel {
                                    Layout.fillWidth: true
                                    text: name
                                    font.bold: true
                                    font.pixelSize: root.fontBody
                                    elide: Text.ElideRight
                                }
                                AppLabel {
                                    Layout.fillWidth: true
                                    text: root.typeLabel(type) + "  ·  " + root.hopsText(hops)
                                          + "  ·  " + lastHeardText
                                          + (onNode ? "" : "  ·  " + qsTr("not on node"))
                                    // Orange: heard, but not in the node's table -
                                    // direct messages save it there first.
                                    color: onNode ? "#555" : "#e65100"
                                    font.pixelSize: root.fontSecondary
                                    elide: Text.ElideRight
                                }
                            }

                            // Unread direct messages from this contact.
                            UnreadBadge {
                                count: root.app ? root.app.dmUnreadFor(publicKey) : 0
                            }

                            AppLabel {
                                visible: manageable || type === "chat"
                                text: manageable ? qsTr("Manage ›") : qsTr("Chat ›")
                                color: root.typeColor(type)
                                font.pixelSize: root.fontSecondary
                            }

                            // Favourite toggle. The RowLayout is raised above
                            // the row-wide MouseArea (z: 1), so a tap on the
                            // star toggles it instead of opening the admin page.
                            AppLabel {
                                text: isFavorite ? "★" : "☆"
                                color: isFavorite ? "#f9a825" : "#9e9e9e"
                                font.pixelSize: Math.round(root.gu * 2.6)
                                Layout.preferredWidth: Math.round(root.gu * 3.5)
                                horizontalAlignment: Text.AlignHCenter
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -Math.round(root.gu * 0.6)
                                    onClicked: BluetoothManager.contactsModel.toggleFavorite(publicKey)
                                }
                            }
                        }

                        MouseArea {
                            id: tap
                            anchors.fill: parent
                            // Tap: chat with companions, admin for repeaters/rooms.
                            // In selection mode a tap (de)selects instead.
                            onClicked: {
                                if (root.selectMode)
                                    root.toggleSelected(publicKey)
                                else if (type === "chat")
                                    root.openChat(publicKey, name)
                                else if (manageable)
                                    root.openAdmin(publicKey, name, type)
                            }
                            // Long press: menu with all options.
                            onPressAndHold: {
                                if (root.selectMode) {
                                    root.toggleSelected(publicKey)
                                    return
                                }
                                contactMenu.keyHex = publicKey
                                contactMenu.contactName = name
                                contactMenu.contactType = type
                                contactMenu.favorite = isFavorite
                                contactMenu.onNode = onNode
                                var p = mapToItem(root, mouse.x, mouse.y)
                                contactMenu.popup(root, p.x, p.y)
                            }
                        }
                    }
                }
            }
        }
    }
}

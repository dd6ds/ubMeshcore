import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// One row of the contact list: type, name, hops, last heard, unread badge and
// favourite star. Rows that do not match the filters collapse to zero height
// instead of leaving the model.
Item {
    id: row
    property var list                // ContactList (filters, selection, actions)
    property var contact             // the delegate's model data (roles)

    readonly property bool shown: list.matches(contact.type, contact.name, contact.keyPrefix,
                                               contact.isActive, contact.isFavorite)
    readonly property bool manageable: contact.type === "repeater" || contact.type === "room"
    readonly property bool isSelected: list.selectMode && list.selected[contact.publicKey] === true

    width: ListView.view ? ListView.view.width : 0
    height: shown ? card.height + Math.round(list.gu * 0.6) : 0
    visible: shown

    Rectangle {
        id: card
        width: parent.width
        height: Math.round(list.gu * 7.5)
        radius: Math.round(list.gu * 0.5)
        color: row.isSelected ? "#ffe0b2" : (tap.pressed ? "#d6e8ff" : "#f5f5f5")
        border.color: list.typeColor(contact.type)
        border.width: 2
        opacity: contact.isActive ? 1.0 : 0.6

        RowLayout {
            z: 1
            anchors.fill: parent
            anchors.margins: Math.round(list.gu * 0.8)
            spacing: list.spacingUnit

            // Selection box (selection mode only).
            Rectangle {
                visible: list.selectMode
                Layout.preferredWidth: Math.round(list.gu * 2.6)
                Layout.preferredHeight: Layout.preferredWidth
                radius: 4
                border.color: "#e65100"
                border.width: 2
                color: row.isSelected ? "#e65100" : "white"
                Text {
                    anchors.centerIn: parent
                    visible: row.isSelected
                    text: "✓"
                    color: "white"
                    font.bold: true
                    font.pixelSize: Math.round(list.gu * 1.8)
                }
            }

            // Active indicator.
            Rectangle {
                Layout.preferredWidth: Math.round(list.gu * 1.2)
                Layout.preferredHeight: Layout.preferredWidth
                radius: width / 2
                color: contact.isActive ? "#2e7d32" : "#9e9e9e"
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                AppLabel {
                    Layout.fillWidth: true
                    text: contact.name
                    font.bold: true
                    font.pixelSize: list.fontBody
                    elide: Text.ElideRight
                }
                AppLabel {
                    Layout.fillWidth: true
                    text: list.typeLabel(contact.type) + "  ·  " + list.hopsText(contact.hops)
                          + "  ·  " + contact.lastHeardText
                          + (contact.onNode ? "" : "  ·  " + qsTr("not on node"))
                    // Orange: heard, but not in the node's table - direct
                    // messages save it there first.
                    color: contact.onNode ? "#555" : "#e65100"
                    font.pixelSize: list.fontSecondary
                    elide: Text.ElideRight
                }
            }

            // Unread direct messages from this contact.
            UnreadBadge {
                count: list.app ? list.app.dmUnreadFor(contact.publicKey) : 0
            }

            AppLabel {
                visible: row.manageable || contact.type === "chat"
                text: row.manageable ? qsTr("Manage ›") : qsTr("Chat ›")
                color: list.typeColor(contact.type)
                font.pixelSize: list.fontSecondary
            }

            // Favourite toggle. The RowLayout is raised above the row-wide
            // MouseArea (z: 1), so a tap on the star toggles it instead of
            // opening the contact.
            AppLabel {
                text: contact.isFavorite ? "★" : "☆"
                color: contact.isFavorite ? "#f9a825" : "#9e9e9e"
                font.pixelSize: Math.round(list.gu * 2.6)
                Layout.preferredWidth: Math.round(list.gu * 3.5)
                horizontalAlignment: Text.AlignHCenter
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -Math.round(list.gu * 0.6)
                    onClicked: BluetoothManager.contactsModel.toggleFavorite(contact.publicKey)
                }
            }
        }

        MouseArea {
            id: tap
            anchors.fill: parent
            // Tap: chat with companions, admin for repeaters/rooms.
            // In selection mode a tap (de)selects instead.
            onClicked: {
                if (list.selectMode)
                    list.toggleSelected(contact.publicKey)
                else if (contact.type === "chat")
                    list.openChat(contact.publicKey, contact.name)
                else if (row.manageable)
                    list.openAdmin(contact.publicKey, contact.name, contact.type)
            }
            // Long press: menu with all options.
            onPressAndHold: {
                if (list.selectMode)
                    list.toggleSelected(contact.publicKey)
                else
                    list.showContactMenu(contact, tap, mouse.x, mouse.y)
            }
        }
    }
}

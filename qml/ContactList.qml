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
        if (type === "repeater") return "Repeater"
        if (type === "room") return "Room"
        if (type === "chat") return "Companion"
        if (type === "sensor") return "Sensor"
        return "Unknown"
    }

    function hopsText(hops) {
        if (hops < 0) return "flood"
        if (hops === 0) return "direct"
        return hops + (hops === 1 ? " hop" : " hops")
    }

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: listPage
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
                          ? "Connect to your node to load contacts"
                          : BluetoothManager.contactsSyncing
                            ? "Loading contacts..."
                            : BluetoothManager.contactsModel.count + " contact(s) from received adverts"
                }
                Button {
                    text: "Refresh"
                    enabled: BluetoothManager.sessionReady && !BluetoothManager.contactsSyncing
                    onClicked: BluetoothManager.refreshContacts()
                }
                Button {
                    text: "Advert"
                    enabled: BluetoothManager.sessionReady
                    onClicked: BluetoothManager.sendAdvert(true)
                }
            }

            AppField {
                Layout.fillWidth: true
                placeholderText: "Search name or key..."
                text: root.searchText
                onTextChanged: root.searchText = text
            }

            // Type filter
            RowLayout {
                Layout.fillWidth: true
                spacing: Math.round(root.gu * 0.5)
                Repeater {
                    model: [ { k: "all", t: "All" }, { k: "repeater", t: "Repeaters" },
                             { k: "chat", t: "Companions" }, { k: "room", t: "Rooms" } ]
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
                    text: "★ Only favourites"
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
                    text: "Only active (24 h)"
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
                          ? "No favourites yet. Tap the ☆ on a contact to mark it."
                          : "No contacts yet. Repeaters and nodes appear here as soon as "
                            + "your node receives their adverts. Tap \"Advert\" to announce "
                            + "yourself."
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
                        color: tap.pressed ? "#d6e8ff" : "#f5f5f5"
                        border.color: root.typeColor(type)
                        border.width: 2
                        opacity: isActive ? 1.0 : 0.6

                        RowLayout {
                            z: 1
                            anchors.fill: parent
                            anchors.margins: Math.round(root.gu * 0.8)
                            spacing: root.spacingUnit

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
                                    color: "#555"
                                    font.pixelSize: root.fontSecondary
                                    elide: Text.ElideRight
                                }
                            }

                            AppLabel {
                                visible: manageable
                                text: "Manage ›"
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
                            enabled: manageable
                            onClicked: stack.push(adminComponent, {
                                                      keyHex: publicKey,
                                                      nodeName: name,
                                                      nodeType: type
                                                  })
                        }
                    }
                }
            }
        }
    }
}

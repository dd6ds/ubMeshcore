import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Search field, type filter and the "favourites" / "active" switches of the
// contact list. The filter state itself lives in ContactList.
ColumnLayout {
    id: bar
    property var list                // ContactList

    spacing: list.spacingUnit

    AppField {
        Layout.fillWidth: true
        placeholderText: qsTr("Search name or key...")
        text: list.searchText
        onTextChanged: list.searchText = text
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Math.round(list.gu * 0.5)
        Repeater {
            model: [ { k: "all", t: qsTr("All") },
                     { k: "repeater", t: qsTr("Repeaters") },
                     { k: "chat", t: qsTr("Companions") },
                     { k: "room", t: qsTr("Rooms") } ]
            delegate: Button {
                text: modelData.t
                checkable: true
                autoExclusive: true
                checked: list.typeFilter === modelData.k
                font.pixelSize: list.fontSecondary
                Layout.fillWidth: true
                onClicked: list.typeFilter = modelData.k
            }
        }
    }

    // CheckBox + AppLabel instead of the CheckBox's own text, which ignores
    // the explicit text colour (see AppLabel.qml).
    RowLayout {
        Layout.fillWidth: true
        CheckBox {
            checked: list.favoritesOnly
            onToggled: list.favoritesOnly = checked
        }
        AppLabel {
            Layout.fillWidth: true
            text: qsTr("★ Only favourites")
            font.pixelSize: list.fontSecondary
            wrapMode: Text.Wrap
            MouseArea {
                anchors.fill: parent
                onClicked: list.favoritesOnly = !list.favoritesOnly
            }
        }
        CheckBox {
            checked: list.activeOnly
            onToggled: list.activeOnly = checked
        }
        AppLabel {
            Layout.fillWidth: true
            text: qsTr("Only active (24 h)")
            font.pixelSize: list.fontSecondary
            wrapMode: Text.Wrap
            MouseArea {
                anchors.fill: parent
                onClicked: list.activeOnly = !list.activeOnly
            }
        }
    }
}

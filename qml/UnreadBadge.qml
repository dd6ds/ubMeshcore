import QtQuick 2.12

// Red pill with the number of unread messages; invisible at zero.
Rectangle {
    id: badge
    property int count: 0
    property real unit: (typeof gridUnitPx !== "undefined" && gridUnitPx > 0) ? gridUnitPx : 10

    visible: count > 0
    height: Math.round(unit * 2.4)
    width: Math.max(height, countText.implicitWidth + Math.round(unit * 1.2))
    radius: height / 2
    color: "#d32f2f"

    Text {
        id: countText
        anchors.centerIn: parent
        text: badge.count > 99 ? "99+" : String(badge.count)
        color: "#ffffff"
        font.bold: true
        font.pixelSize: Math.round(badge.unit * 1.4)
    }
}

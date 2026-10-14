import QtQuick 2.12
import QtQuick.Controls 2.12

// TextField with an explicit light background and dark text. Relying on the
// platform palette gave dark text on a dark background, so whatever was typed
// was invisible.
TextField {
    id: field
    color: "#111111"
    selectionColor: "#0078d7"
    selectedTextColor: "#ffffff"
    placeholderTextColor: "#999999"
    background: Rectangle {
        color: "#ffffff"
        border.color: field.activeFocus ? "#0078d7" : "#bbbbbb"
        border.width: field.activeFocus ? 2 : 1
        radius: 4
    }
}

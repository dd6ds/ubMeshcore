import QtQuick 2.12
import QtQuick.Controls 2.12

// TextField in the app's field colours (palette from main.qml), so typed text
// stays readable in the light and the dark theme.
TextField {
    id: field
    color: palette.text
    selectionColor: palette.highlight
    selectedTextColor: palette.highlightedText
    // The palette has no placeholder role in Qt 5.12; take it from the window.
    placeholderTextColor: ApplicationWindow.window ? ApplicationWindow.window.colorPlaceholder
                                                   : "#999999"
    background: Rectangle {
        color: field.palette.base
        border.color: field.activeFocus ? field.palette.highlight : field.palette.mid
        border.width: field.activeFocus ? 2 : 1
        radius: 4
    }
}

import QtQuick 2.12
import QtQuick.Controls 2.12

// Label in the app's text colour. The window sets the palette (main.qml), so
// this follows the light or dark theme; without it Lomiri's platform palette
// could put light text on a light background.
Label {
    color: palette.windowText
}

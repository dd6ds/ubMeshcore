import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Light/dark appearance: follow the phone's theme or pick one fixed. Stored in
// app.settings.themeMode, which main.qml turns into the colours.
ColumnLayout {
    id: choice
    property var app

    spacing: 0

    AppLabel {
        text: qsTr("Appearance")
        font.bold: true
        font.pixelSize: app.fontSecondary
    }

    Flow {
        Layout.fillWidth: true

        Repeater {
            model: [
                { mode: "system", label: qsTr("Like the phone") },
                { mode: "light", label: qsTr("Light") },
                { mode: "dark", label: qsTr("Dark") }
            ]
            RadioButton {
                id: themeOption
                // text must be set: without it the style centres the indicator.
                text: modelData.label
                checked: choice.app.settings.themeMode === modelData.mode
                onToggled: if (checked) choice.app.settings.themeMode = modelData.mode
                contentItem: AppLabel {
                    leftPadding: themeOption.indicator.width + themeOption.spacing
                    text: themeOption.text
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}

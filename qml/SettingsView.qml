import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

/// Settings view for the app

Page {
    title: "Settings"
    ColumnLayout {
        anchors.fill: parent
        AppLabel { text: "Settings"; Layout.alignment: Qt.AlignHCenter }
    }
}

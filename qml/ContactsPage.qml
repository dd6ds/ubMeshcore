import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

// Fifth page: the node's contacts (see ContactList.qml).
Page {
    id: page
    property var app                 // main.qml root (message model + DM helpers)

    title: qsTr("Contacts")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: app.spacingUnit
        spacing: app.spacingUnit

        ContactList {
            Layout.fillWidth: true
            Layout.fillHeight: true
            app: page.app
        }

        Button {
            text: qsTr("Back")
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.round(app.gu * 22)
            Layout.preferredHeight: Math.round(app.gu * 5)
            onClicked: app.showPage(app.pageConnection)
        }
    }
}

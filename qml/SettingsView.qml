import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/// Settings view for the app

Page {
    id: settingsPage
    title: "Settings"
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 11
        spacing: 11
        
        // App settings section
        Label {
            text: "Application Settings"
            font.bold: true
            font.pixelSize: 18
            Layout.alignment: Qt.AlignHCenter
        }
        
        // Node name
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: "Your Name:"
                Layout.alignment: Qt.AlignLeft
            }
            
            TextField {
                id: userNameField
                placeholderText: "Enter your name"
                Layout.fillWidth: true
            }
        }
        
        // Default channel
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: "Default Channel:"
                Layout.alignment: Qt.AlignLeft
            }
            
            TextField {
                id: defaultChannelField
                placeholderText: "#p53"
                Layout.fillWidth: true
            }
        }
        
        // Bluetooth settings
        GroupBox {
            title: "Bluetooth"
            Layout.fillWidth: true
            
            ColumnLayout {
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: "Bluetooth Enabled:"
                        Layout.alignment: Qt.AlignLeft
                    }
                    
                    Switch {
                        id: bluetoothSwitch
                        checked: true
                        onClicked: {
                            // Bluetooth permissions handled by the system
                        }
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: "Scan Duration:"
                        Layout.alignment: Qt.AlignLeft
                    }
                    
                    Slider {
                        id: scanDurationSlider
                        from: 5
                        to: 60
                        value: 10
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: scanDurationSlider.value + " seconds"
                        Layout.alignment: Qt.AlignRight
                    }
                }
            }
        }
        
        // Node administration section
        GroupBox {
            title: "Node Administration"
            Layout.fillWidth: true
            
            ColumnLayout {
                Button {
                    text: "Connect to Admin Node"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: {
                        // Open node selection
                        nodeAdminDialog.open()
                    }
                }
                
                Button {
                    text: "Node Settings"
                    Layout.alignment: Qt.AlignHCenter
                    enabled: nodeManager.connected
                    onClicked: {
                        nodeSettingsDialog.open()
                    }
                }
            }
        }
        
        // Advanced settings
        GroupBox {
            title: "Advanced"
            Layout.fillWidth: true
            
            ColumnLayout {
                Button {
                    text: "Clear Cache"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: clearCache()
                }
                
                Button {
                    text: "Reset App"
                    Layout.alignment: Qt.AlignHCenter
                    onClicked: resetApp()
                }
                
                Label {
                    text: "Version: " + Qt.application.version
                    color: "#666"
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }
        
        // Save button
        Button {
            text: "Save Settings"
            Layout.alignment: Qt.AlignHCenter
            onClicked: saveSettings()
        }
    }
    
    // Node administration dialog
    Dialog {
        id: nodeAdminDialog
        title: "Select Node for Administration"
        standardButtons: Dialog.Cancel | Dialog.Ok
        
        ColumnLayout {
            ListView {
                Layout.fillWidth: true
                height: 200
                model: nodeManager.nodes
                delegate: ItemDelegate {
                    text: modelData.name + " (" + modelData.address + ")"
                    onClicked: {
                        nodeAdminDialog.selectedNode = modelData
                    }
                }
            }
        }
        
        onAccepted: {
            if (selectedNode) {
                nodeManager.connectToNode(selectedNode)
            }
        }
        
        property var selectedNode: null
    }
    
    // Node settings dialog
    Dialog {
        id: nodeSettingsDialog
        title: "Node Settings"
        standardButtons: Dialog.Cancel | Dialog.Ok
        visible: nodeManager.connected
        
        ColumnLayout {
            Label {
                text: "Connected to: " + (nodeManager.currentNode ? nodeManager.currentNode.name : "Unknown")
            }
            
            TextField {
                placeholderText: "Admin Password"
                echoMode: TextInput.Password
            }
            
            Button {
                text: "Send Command"
                onClicked: sendAdminCommand()
            }
        }
        
        function sendAdminCommand() {
            // TODO: Implement admin command sending
        }
    }
    
    function clearCache() {
        // TODO: Implement cache clearing
    }
    
    function resetApp() {
        // TODO: Implement app reset
    }
    
    function saveSettings() {
        // TODO: Save settings to persistent storage
    }
}

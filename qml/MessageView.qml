import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/// Message view component for displaying channel messages

ColumnLayout {
    id: root
    spacing: 0
    
    property alias model: messageList.model
    property string channelName: ""
    
    // Header
    RowLayout {
        Layout.fillWidth: true
        padding: 11
        
        Label {
            text: "Channel: " + channelName
            font.bold: true
            font.pixelSize: 18
            Layout.alignment: Qt.AlignLeft
        }
        
        Label {
            text: messageCount + " messages"
            Layout.alignment: Qt.AlignRight
            color: "#666"
        }
    }
    
    // Message input
    RowLayout {
        Layout.fillWidth: true
        padding: 11
        spacing: 11
        
        TextField {
            id: messageInput
            placeholderText: "Type a message..."
            Layout.fillWidth: true
            
            Keys.onReturnPressed: sendButton.clicked()
        }
        
        Button {
            id: sendButton
            text: "Send"
            icon.name: "send"
            onClicked: {
                if (messageInput.text.trim() !== "") {
                    messageList.model.append({
                        sender: "You",
                        text: messageInput.text,
                        time: new Date().toLocaleTimeString(),
                        timestamp: Date.now(),
                        isOwn: true
                    })
                    messageInput.text = ""
                    messageInput.focus = true
                    nodeManager.sendMessage("You", messageInput.text)
                }
            }
        }
    }
    
    // Message list
    ListView {
        id: messageList
        Layout.fillWidth: true
        Layout.fillHeight: true
        
        model: ListModel {}
        
        delegate: MessageDelegate {
            message: modelData
        }
        
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
    }
    
    // Message delegate component
    component MessageDelegate: ItemDelegate {
        id: messageDelegate
        
        property var message: null
        
        contentItem: ColumnLayout {
            spacing: 4
            
            RowLayout {
                Layout.fillWidth: true
                
                Label {
                    text: message.sender
                    font.bold: message.isOwn ? false : true
                    color: message.isOwn ? "#0078d7" : "#000"
                    Layout.alignment: Qt.AlignLeft
                }
                
                Label {
                    text: message.time
                    color: "#666"
                    font.pixelSize: 12
                    Layout.alignment: Qt.AlignRight
                }
            }
            
            Label {
                text: message.text
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                padding: 8
                background: Rectangle {
                    color: message.isOwn ? "#e6f2ff" : "transparent"
                    radius: 4
                }
            }
        }
        
        onClicked: console.log("Message clicked:", message.text)
    }
    
    readonly property int messageCount: messageList.model.count
}

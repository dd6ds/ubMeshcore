import QtQuick 2.15
import QtQuick.Controls 2.15

/// Node manager for MeshCore nodes
/// Handles node discovery, connection, and management

pragma Singleton
QtObject {
    id: nodeManager
    
    property var nodes: []
    property var currentNode: null
    property var channels: []
    property string currentChannel: ""
    property bool connected: false
    
    signal nodesChanged()
    signal nodeConnected(var node)
    signal nodeDisconnected()
    signal messageReceived(var message)
    
    // Add a discovered MeshCore node
    function addNode(node) {
        var exists = false
        for (var i = 0; i < nodes.length; i++) {
            if (nodes[i].address === node.address) {
                exists = true
                nodes[i] = node
                break
            }
        }
        if (!exists) {
            nodes.push(node)
        }
        nodesChanged()
    }
    
    // Connect to a node
    function connectToNode(node) {
        // Disconnect from current node if connected
        if (connected) {
            disconnect()
        }
        
        // Connect via Bluetooth
        if (BluetoothManager.connectToDevice(node.address)) {
            currentNode = node
            connected = true
            nodeConnected(node)
            return true
        }
        return false
    }
    
    // Disconnect from current node
    function disconnect() {
        if (connected && currentNode) {
            BluetoothManager.disconnect()
            connected = false
            nodeDisconnected()
            currentNode = null
        }
    }
    
    // Join a channel
    function joinChannel(channelName) {
        // TODO: Send channel join command to node
        currentChannel = channelName
        
        // Add to channels list if not exists
        var exists = false
        for (var i = 0; i < channels.length; i++) {
            if (channels[i].name === channelName) {
                exists = true
                break
            }
        }
        if (!exists) {
            channels.push({
                name: channelName,
                messages: []
            })
        }
        
        return true
    }
    
    // Leave a channel
    function leaveChannel(channelName) {
        // TODO: Send channel leave command to node
        if (currentChannel === channelName) {
            currentChannel = ""
        }
        
        for (var i = 0; i < channels.length; i++) {
            if (channels[i].name === channelName) {
                channels.splice(i, 1)
                break
            }
        }
        
        return true
    }
    
    // Send a message to current channel
    function sendMessage(sender, text) {
        if (!connected || !currentNode || !currentChannel) {
            return false
        }
        
        // Format message
        var message = {
            sender: sender,
            text: text,
            time: new Date().getTime(),
            channel: currentChannel
        }
        
        // Send via Bluetooth
        var data = JSON.stringify(message)
        if (BluetoothManager.sendData(data)) {
            // Add to local channel
            for (var i = 0; i < channels.length; i++) {
                if (channels[i].name === currentChannel) {
                    channels[i].messages.push(message)
                    break
                }
            }
            return true
        }
        return false
    }
    
    // Process incoming message
    function processIncomingData(data) {
        try {
            var message = JSON.parse(data)
            if (message.channel) {
                // Find or create channel
                var channel = null
                for (var i = 0; i < channels.length; i++) {
                    if (channels[i].name === message.channel) {
                        channel = channels[i]
                        break
                    }
                }
                
                if (!channel) {
                    channel = {
                        name: message.channel,
                        messages: []
                    }
                    channels.push(channel)
                }
                
                // Add message
                channel.messages.push(message)
                messageReceived(message)
                
                // If this is the current channel, emit signal
                if (message.channel === currentChannel) {
                    messageReceived(message)
                }
                
                return true
            }
        } catch (e) {
            console.error("Error parsing message:", e)
        }
        return false
    }
}

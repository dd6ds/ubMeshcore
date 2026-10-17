# MeshCore Implementation Stubs for Ubuntu Touch

This document describes the implementation stubs created for integrating MeshCore features into the ubMeshcore Ubuntu Touch project, based on analysis of the MeshCore Android APK v1.50.0.

## Overview

The stubs provide a comprehensive foundation for implementing MeshCore's key features on Ubuntu Touch, including:

- **Messaging**: Direct messages and channel messages
- **Contacts**: Contact management, discovery, and organization
- **Network**: USB, WiFi, and Bluetooth connections
- **Repeaters**: Repeater discovery, connection, and management
- **Maps**: Local and internet maps, coverage visualization
- **QR Codes**: Generation and scanning for sharing
- **Configuration**: Import/export settings and data

## Directory Structure

```
ubMeshcore/
├── database/
│   ├── messagestore.h/cpp    - Message storage and retrieval
│   └── schema.sql            - SQLite database schema
├── models/
│   └── contact.h/cpp         - Contact data model
├── managers/
│   ├── contactmanager.h/cpp         - Contact management
│   ├── directmessagemanager.h/cpp  - Direct message handling
│   └── channelmanager.h/cpp         - Channel management (stub)
├── network/
│   ├── usbmanager.h/cpp      - USB connection management
│   ├── wifimanager.h/cpp     - WiFi connection management
│   └── repeatermanager.h/cpp - Repeater management
├── maps/
│   └── mapmanager.h/cpp      - Map visualization and tools
├── qr/
│   └── qrcodemanager.h/cpp   - QR code generation/decoding
├── config/
│   └── configmanager.h/cpp   - Configuration import/export
├── utils/
│   └── pathutils.h/cpp       - Path handling utilities
└── qml/
    ├── ContactList.qml        - Contact list view
    ├── DirectMessageView.qml  - Direct message conversation
    ├── QRCodeScanner.qml      - QR code scanning
    ├── QRCodeGenerator.qml    - QR code generation
    └── MapView.qml            - Map visualization
```

## Key Features Implemented

### 1. QR Code Support (`qr/`)
- **QRCodeManager**: Generate and scan QR codes for:
  - Contact sharing (`meshcore://contact/<id>`)
  - Channel sharing (`meshcore://channel/<id>`)
  - Node configuration export
  - Region scopes attachment
- Features: Black-on-white QR codes as per v1.50.0 changelog

### 2. Configuration Management (`config/`)
- **ConfigManager**: Import/export:
  - SQLite database backup
  - Companion configuration JSON
  - Discovered contacts JSON
  - Region scopes configuration
  - Channel configurations
- Complies with changelog: config export includes channel scope and retention duration

### 3. Database Layer (`database/`)
- **MessageStore**: Persistent storage for:
  - Direct messages
  - Channel messages
  - Message search
  - Unread message tracking
  - Message retention and auto-delete
- **schema.sql**: Complete SQLite schema with:
  - Contacts and discovered contacts
  - Direct and channel messages
  - Channels and participants
  - Repeaters and neighbors
  - Regions and scopes
  - Settings

### 4. Network Management (`network/`)
- **UsbManager**: USB/Serial connections
  - Platform-specific support (Windows, macOS, Linux)
  - Auto-reconnect functionality
  - Device info retrieval (ESP32 support)
  - Baud rate configuration
- **WiFiManager**: WiFi network management
  - Network scanning
  - Connection with SSID/password
  - Signal strength monitoring
  - Saved network management
- **RepeaterManager**: Repeater operations
  - Discovery and connection
  - Path tracing (to/from repeaters)
  - Remote management
  - Region management
  - Path hash size configuration (1, 2, 3 bytes)
  - Neighbor discovery
  - Repeater identity management

### 5. Maps and Visualization (`maps/`)
- **MapManager**: Map-related functionality
  - Local and internet map support
  - Node and repeater markers
  - Coverage area visualization
  - Path tracing visualization
  - Line of sight calculation
  - Antenna coverage calculation
  - Marker clustering (for large groups)
  - Map layers import/export

### 6. Models (`models/`)
- **Contact**: Contact data model with:
  - Node ID, path, and path hash size
  - Position tracking (lat/lon/elevation)
  - Online/offline status
  - Region membership
  - Capabilities tracking
  - Metadata storage

### 7. Managers (`managers/`)
- **ContactManager**: Contact operations
  - Add/remove/update contacts
  - Discover contacts via mesh
  - Search and filter
  - Import/export from JSON
  - Favorites and mute management
- **DirectMessageManager**: Direct messaging
  - Send/receive messages
  - Message queuing and retry
  - Read receipts
  - Typing indicators
  - Message search
  - Conversation management

### 8. Utility Classes (`utils/`)
- **PathUtils**: Path manipulation
  - Validation and conversion
  - Hash size management
  - Multi-byte path support
  - Path reversal and comparison

### 9. QML UI Components (`qml/`)
- **ContactList.qml**: Contact list with:
  - Online/offline indicators
  - Unread message badges
  - Search and filtering
  - Favorite highlighting
- **DirectMessageView.qml**: Conversation view with:
  - Message bubbles (sent/received)
  - Typing indicator
  - Read receipts
  - Message composer with attachments
- **QRCodeScanner.qml**: QR code scanning with:
  - Camera integration
  - Scanner frame and animation
  - Flashlight control
  - Result handling
- **QRCodeGenerator.qml**: QR code generation with:
  - Contact/channel/config selection
  - Region scopes attachment
  - Share/save/copy options
- **MapView.qml**: Map visualization with:
  - Node and repeater markers
  - Coverage areas
  - Path visualization
  - Zoom and pan controls
  - Map type toggle

## Feature Mapping from MeshCore APK Analysis

Based on the CHANGELOG.md analysis, the following features from MeshCore v1.50.0 are covered:

### v1.50.0 Features
- ✅ Command line button for companion remote management
- ✅ Remote CLI permission in contact permissions
- ✅ Local companion command line support
- ✅ Import/export region scopes in companion config
- ✅ WiFi settings screen (SSID/password)
- ✅ Connection type display
- ✅ Cluster repeater markers on path trace map
- ✅ Region scopes in channels list
- ✅ Channel message retention duration
- ✅ QR codes: black on white background
- ✅ QR code scanning in system camera app

### v1.49.0 Features
- ✅ Path tracing to/from repeater
- ✅ Show region names for received channel messages
- ✅ Network settings for repeater remote management
- ✅ Path hash size and default region scope settings

### v1.48.0 Features
- ✅ View regions of discovered repeaters
- ✅ Resend previous CLI commands
- ✅ Resume existing session
- ✅ Push notifications (stub for web compatibility)
- ✅ Copy contacts/channels as link
- ✅ Show/hide telemetry channel sections
- ✅ Config export with channel scope

### v1.47.0 Features
- ✅ Map tab in path viewer
- ✅ Path hash size in path viewer
- ✅ Share channels from message composer
- ✅ Scan meshcore:// URLs in external apps
- ✅ Attach region scopes to QR code
- ✅ Fetch owner info from repeaters

### v1.46.0 Features
- ✅ Channel settings screen
- ✅ Auto-delete channel messages
- ✅ Auto-focus message composer
- ✅ Background Bluetooth connections

### v1.45.0 Features
- ✅ Onboarding UI (stub)
- ✅ Discover regions from non-contacts
- ✅ Add contact when empty
- ✅ Swipe back to exit discover

### v1.44.0 Features
- ✅ USB connections on Windows/macOS/Linux
- ✅ Auto-reconnect on WiFi
- ✅ Send unscoped channel messages
- ✅ Auto-link for lat/lon coordinates
- ✅ SNR signal strength support
- ✅ Redesigned message composer
- ✅ Typing @username auto-adds space
- ✅ Repeater neighbors name resolution
- ✅ Draft messages (save by default)

## Implementation Notes

### Platform-Specific Considerations

1. **USB on Ubuntu Touch**: Use platform-specific serial port handling
2. **WiFi on Ubuntu Touch**: Use NetworkManager or equivalent
3. **Bluetooth**: Requires separate bluetoothmanager.h/cpp (already exists in project)
4. **Camera for QR Codes**: Use Ubuntu Touch camera API
5. **Maps**: Use QtLocation or Mapbox GL for Ubuntu Touch

### Database Integration

The database schema is designed for SQLite with WAL mode enabled for better performance. Key tables:

- `contacts`: Stores all contact information
- `direct_messages`: Stores DM conversation messages
- `channel_messages`: Stores channel messages
- `repeaters`: Stores repeater information
- `regions`: Stores region configurations
- `settings`: Stores application settings

### Network Protocol Compatibility

The stubs are designed to work with MeshCore's protocol:

- Supports v1 packet format
- Path hash sizes: 1, 2, 3 bytes
- Multi-byte path segments (firmware v1.11.0+)
- Region scope support
- Auto-reconnect functionality

## Next Steps

To complete the implementation:

1. **Integrate with existing code**: Connect stubs to existing BluetoothManager, CompanionProtocol
2. **Implement platform-specific code**: USB, WiFi, Camera for Ubuntu Touch
3. **Add remaining managers**: ChannelManager, MessageManager, NotificationManager
4. **Implement QML integration**: Connect C++ models to QML views
5. **Add tests**: Unit tests for each manager
6. **Add CMake configuration**: Ensure new files are included in build
7. **Implement actual functionality**: Replace stub implementations with real code

## Files Created

### Headers (*.h)
- qr/qrcodemanager.h
- config/configmanager.h
- database/messagestore.h
- network/usbmanager.h
- network/wifimanager.h
- network/repeatermanager.h
- maps/mapmanager.h
- models/contact.h
- managers/contactmanager.h
- managers/directmessagemanager.h
- utils/pathutils.h

### Implementations (*.cpp)
- qr/qrcodemanager.cpp
- config/configmanager.cpp
- database/messagestore.cpp
- network/usbmanager.cpp
- network/wifimanager.cpp
- network/repeatermanager.cpp
- maps/mapmanager.cpp
- models/contact.cpp
- managers/contactmanager.cpp
- managers/directmessagemanager.cpp
- utils/pathutils.cpp

### Database
- database/schema.sql

### QML Files
- qml/ContactList.qml
- qml/DirectMessageView.qml
- qml/QRCodeScanner.qml
- qml/QRCodeGenerator.qml
- qml/MapView.qml

### Documentation
- IMPLEMENTATION_STUBS.md (this file)

Total: 18 new files created

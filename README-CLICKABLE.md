# Building ubMeshcore with Clickable

This project is configured to use **Clickable**, the recommended build tool for Ubuntu Touch apps.

## Quick Start

### 1. Install Clickable

```bash
pip3 install --user clickable-ut
```

### 2. Build and Install

With your Ubuntu Touch device connected via ADB:

```bash
cd /home/developer/rust/ubMeshcore
clickable
```

This will:
- Build the Rust library for arm64
- Build the C++/Qt application
- Create the Click package
- Install it on your connected device
- Stream the build logs

### 3. Alternative: Build Only

```bash
clickable --build
```

This builds the package but doesn't install it. The package will be in `build/`.

### 4. Install Manually

```bash
adb push build/ubmeshcore_*.click /home/phablet/
adb shell pkcon install-local --allow-untrusted /home/phablet/ubmeshcore_*.click
```

## Project Structure

```
ubMeshcore/
├── clickable.yaml         # Clickable configuration
├── CMakeLists.txt         # CMake build configuration
├── qml.qrc               # QML resource file
├── manifest.json.in      # App manifest template
├── ubmeshcore.desktop.in # Desktop entry template
├── ubmeshcore.apparmor   # AppArmor security profile
├── ubmeshcore.png        # App icon
├── lib/                  # Pre-built libraries (for development)
│   └── libubmeshcore_core.so
├── qml/                  # QML source files
│   ├── main.qml
│   ├── BluetoothManager.qml
│   ├── NodeManager.qml
│   ├── MessageView.qml
│   └── SettingsView.qml
└── src/                  # C++ source files
    └── main.cpp
```

## Configuration

### clickable.yaml

```yaml
clickable_minimum_required: 8
builder: cmake
kill: ubmeshcore
arch: arm64
```

- `clickable_minimum_required`: Clickable version requirement
- `builder`: Build system (cmake in this case)
- `kill`: App name
- `arch`: Target architecture (arm64 for most Ubuntu Touch devices)

### CMakeLists.txt

The CMake configuration:
- Uses Qt5 Core, Gui, Qml, Quick
- Bundles QML files as resources
- Links with the Rust library
- Sets the correct RPATH for finding the library

### manifest.json.in

Click package manifest template. Clickable will replace:
- `@CLICK_ARCH@` with the target architecture
- `@CLICK_FRAMEWORK@` with the framework version

### ubmeshcore.desktop.in

Desktop entry file for the app launcher.

## Requirements

### On Development Machine

- Python 3
- pip
- Clickable: `pip3 install --user clickable-ut`
- CMake
- Qt5 development packages (for local builds)
- Rust toolchain (for building the Rust library)
- ADB (for installing to device)

### On Ubuntu Touch Device

- Developer mode enabled
- ADB debugging enabled
- Connected via USB or WiFi

## Build Process

Clickable performs these steps:

1. **Configure**: Sets up the build environment
2. **Build Rust Library**: Cross-compiles for arm64
3. **Build C++ App**: Compiles the Qt application
4. **Create Package**: Builds the Click package
5. **Install**: Transfers and installs on device
6. **Run**: Optionally launches the app

## Troubleshooting

### ADB Device Not Found

```bash
# Check if device is connected
adb devices

# If not, connect your device via USB
# Enable Developer Mode on your phone
```

### Build Failures

```bash
# Clean and rebuild
clickable --clean --build

# Verbose output
clickable --verbose

# Build for specific architecture
clickable --arch arm64
```

### Missing Dependencies

On Ubuntu/Debian:
```bash
sudo apt install cmake qt5-qmake qtbase5-dev libqt5bluetooth5-dev
```

On Arch:
```bash
sudo pacman -S cmake qt5-base qt5-declarative
```

## Customization

### Add More QML Files

1. Add the file to `qml/` directory
2. Add it to `qml.qrc`
3. Add it to `CMakeLists.txt` (already done via `QML_RESOURCES`)

### Add C++ Files

1. Add the file to `src/` directory
2. Add it to `CMakeLists.txt`

### Change App Metadata

Edit `manifest.json.in` to change:
- App name
- Version
- Description
- Permissions

## Package Contents

The built Click package contains:

```
ubmeshcore_<version>_arm64.click
├── manifest.json
├── ubmeshcore.desktop
├── ubmeshcore.apparmor
├── ubmeshcore.png
└── ubmeshcore/
    ├── ubmeshcore           # Executable
    ├── lib/                # Libraries
    │   └── libubmeshcore_core.so
    └── qml/                # QML files
        ├── main.qml
        ├── BluetoothManager.qml
        ├── NodeManager.qml
        ├── MessageView.qml
        └── SettingsView.qml
```

## Notes

- The Rust library (`libubmeshcore_core.so`) is cross-compiled for arm64
- The C++ application uses Qt5 and QtBluetooth
- QML files are bundled as resources in the executable
- The app will appear in the app drawer after installation

## More Information

- [Clickable Documentation](https://clickable.bhdouglass.com/)
- [Ubuntu Touch Development](https://docs.ubuntu.com/phone/en)
- [QtBluetooth Documentation](https://doc.qt.io/qt-5/qtbluetooth-index.html)

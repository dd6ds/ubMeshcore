# Building ubMeshcore

This document explains how to build and package ubMeshcore for Ubuntu Touch.

## Quick Start

The easiest way to get started is to use one of the provided build scripts:

```bash
# Option 1: Full build with dependencies check
./build-package.sh --dependencies --clean

# Option 2: Simple Click package creation
./make-click-package.sh

# Option 3: Manual build (see below)
```

## Prerequisites

### On Ubuntu/Debian/Derivatives (for cross-compilation)

```bash
# Install system dependencies
sudo apt update
sudo apt install -y \
    cmake \
    build-essential \
    git \
    qt5-qmake \
    qtbase5-dev \
    qtdeclarative5-dev \
    qtconnectivity5-dev \
    libqt5bluetooth5-dev \
    click \
    pkg-config

# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Add ARM target for cross-compilation
rustup target add arm-unknown-linux-gnueabihf
```

### On Ubuntu Touch Device (for native build)

```bash
# Enable developer mode on device
# Then SSH into device or use phablet-shell

# Install build dependencies
sudo apt update
sudo apt install -y cmake build-essential git qt5-qmake qtbase5-dev click

# Install Rust (if not already present)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

## Build Methods

### Method 1: Using build-package.sh (Recommended)

The `build-package.sh` script handles everything automatically:

```bash
# Install dependencies and build
./build-package.sh --dependencies

# Build for desktop testing
./build-package.sh --target desktop --clean

# Build for Ubuntu Touch (armhf)
./build-package.sh --target ubuntu-touch --arch armhf --clean

# Build and install to connected device
./build-package.sh --target ubuntu-touch --arch armhf --install
```

**Options:**
- `--target TARGET` - desktop or ubuntu-touch (default: ubuntu-touch)
- `--arch ARCH` - armhf, arm64, amd64 (default: auto-detect)
- `--build-type TYPE` - Debug or Release (default: Release)
- `--clean` - Clean build directory before building
- `--install` - Install to connected device
- `--dependencies` - Install build dependencies
- `--verbose` - Verbose output
- `--help` - Show help

### Method 2: Using make-click-package.sh (Simple)

For a quick Click package without full build:

```bash
# Create a basic Click package
./make-click-package.sh
```

This script:
1. Builds the Rust library
2. Creates the Click package structure
3. Generates a .click file ready for installation

### Method 3: Manual Build

#### Step 1: Build Rust Library

```bash
cd src/rust
cargo build --release --lib --target arm-unknown-linux-gnueabihf
cd ../..
```

#### Step 2: Build C++/Qt Application

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

#### Step 3: Create Click Package

```bash
# Create package structure
mkdir -p click-package/ubmeshcore
cp -r src/qml/* click-package/ubmeshcore/
cp build/ubmeshcore click-package/ubmeshcore/
cp manifest.json click-package/
cp ubmeshcore.desktop click-package/ubmeshcore/
cp ubmeshcore.apparmor click-package/ubmeshcore/

# Add icon
# (create or copy an 80x80 PNG)
cp icon.png click-package/ubmeshcore/

# Build Click package
cd click-package
click build .

# The package will be: ubmeshcore_0.1.0_armhf.click
```

## Cross-Compilation

### Using Ubuntu SDK

1. Install Ubuntu SDK:
   ```bash
   sudo apt install ubuntu-sdk
   ```

2. Open Qt Creator, load the project, and select the Ubuntu Touch kit

3. Build and run

### Manual Cross-Compilation

```bash
# Install ARM toolchain
sudo apt install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf

# Set environment variables
export CC=arm-linux-gnueabihf-gcc
export CXX=arm-linux-gnueabihf-g++
export CARGO_TARGET_ARM_UNKNOWN_LINUX_GNUEABIHF_LINKER=arm-linux-gnueabihf-gcc

# Build for ARM
cargo build --release --target arm-unknown-linux-gnueabihf
```

## Installing the Package

### On Ubuntu Touch Device

```bash
# Method 1: Using ADB
adb push ubmeshcore_0.1.0_armhf.click /home/phablet/
adb shell pkcon install-local --allow-untrusted /home/phablet/ubmeshcore_0.1.0_armhf.click

# Method 2: Using phablet-shell (if device is connected via USB)
phablet-shell pkcon install-local --allow-untrusted ubmeshcore_0.1.0_armhf.click

# Method 3: Direct copy (SSH into device)
scp ubmeshcore_0.1.0_armhf.click phablet@192.168.2.15:~/
ssh phablet@192.168.2.15 pkcon install-local --allow-untrusted ~/ubmeshcore_0.1.0_armhf.click
```

### Verifying Installation

```bash
# Check if app is installed
pkcon list | grep ubmeshcore

# Launch the app
ubmeshcore

# Check logs
journalctl -u ubuntu-touch-session@phablet -f | grep -i ubmeshcore
```

## Desktop Testing

For testing on desktop before deploying to device:

```bash
# Build for desktop
./build-package.sh --target desktop --clean

# Run the app
./build/ubmeshcore
```

Note: Bluetooth functionality may be limited on desktop depending on your hardware.

## Troubleshooting

### Common Issues

#### 1. Missing dependencies
```
Error: Required command 'qmake-qt5' is not installed
```
**Solution:** Install Qt5 development packages
```bash
sudo apt install qt5-qmake qtbase5-dev qtdeclarative5-dev
```

#### 2. Rust not found
```
Error: Required command 'cargo' is not installed
```
**Solution:** Install Rust
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

#### 3. Click command not found
```
Error: Required command 'click' is not installed
```
**Solution:** Install Click
```bash
sudo apt install click
```

#### 4. Cannot find -lQt5Bluetooth
```
Error: linker cannot find -lQt5Bluetooth
```
**Solution:** Install Qt5 Bluetooth development package
```bash
sudo apt install libqt5bluetooth5-dev
```

#### 5. Architecture mismatch
```
Error: Package architecture 'amd64' not compatible with device 'armhf'
```
**Solution:** Build for the correct architecture
```bash
./build-package.sh --arch armhf
```

### Debug Builds

For debugging, use Debug builds:

```bash
./build-package.sh --build-type Debug --verbose
```

This will:
- Disable optimizations
- Include debug symbols
- Enable verbose logging

### Logging

View app logs on device:
```bash
journalctl -u ubuntu-touch-session@phablet -f | grep -i ubmeshcore
```

Or enable debug logging:
```bash
QT_LOGGING_RULES="ubmeshcore=debug" ubmeshcore
```

## Package Contents

The Click package contains:

```
ubmeshcore.click
├── manifest.json          # App metadata
├── ubmeshcore.desktop     # Desktop entry
├── ubmeshcore.apparmor    # Security profile
└── ubmeshcore/            # App directory
    ├── ubmeshcore          # Main executable
    ├── lib/                # Libraries
    │   └── libubmeshcore_core.so  # Rust library
    ├── qml/                # QML files
    │   ├── main.qml
    │   ├── BluetoothManager.qml
    │   ├── NodeManager.qml
    │   ├── MessageView.qml
    │   └── SettingsView.qml
    └── ubmeshcore.png      # App icon
```

## Customization

### Changing App Metadata

Edit `manifest.json` to change:
- App name
- Version
- Description
- Permissions

### Adding Features

1. Add new QML components in `src/qml/`
2. Add Rust functions in `src/rust/src/lib.rs`
3. Expose to C++ via FFI
4. Connect to QML signals/slots

### Bluetooth Protocol

The app uses QtBluetooth for BLE connectivity. To customize:
- Edit `src/cpp/main.cpp` for Bluetooth handling
- Edit `src/qml/BluetoothManager.qml` for QML Bluetooth interface

## Performance Tips

1. **Release builds**: Always use `--release` for production
2. **Optimizations**: Rust is already optimized by default
3. **Strip symbols**: For smaller packages
   ```bash
   arm-linux-gnueabihf-strip libubmeshcore_core.so
   ```

## Clean Build

To start fresh:

```bash
# Remove all build artifacts
rm -rf build target click-build

# Or use the --clean flag
./build-package.sh --clean
```

## Continuous Integration

For automated builds:

```bash
# Example GitHub Actions workflow
name: Build ubMeshcore

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt update
          sudo apt install -y cmake qt5-qmake qtbase5-dev libqt5bluetooth5-dev click
          curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
          source $HOME/.cargo/env
          rustup target add arm-unknown-linux-gnueabihf
      
      - name: Build
        run: ./build-package.sh --target ubuntu-touch --arch armhf
      
      - name: Upload package
        uses: actions/upload-artifact@v3
        with:
          name: ubmeshcore-click-package
          path: click-build/*.click
```

## Additional Resources

- [Ubuntu Touch Documentation](https://docs.ubuntu.com/phone/en)
- [Click Packaging Guide](https://developer.ubuntu.com/en/phone/platform/guides/click-packaging/)
- [QtBluetooth Documentation](https://doc.qt.io/qt-5/qtbluetooth-index.html)
- [MeshCore Protocol](https://github.com/meshcore-dev/MeshCore)

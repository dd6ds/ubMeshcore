#!/bin/bash

# fix-startup.sh - Automated troubleshooting script for ubMeshcore
# Run this after building and installing the app

echo "=========================================="
echo "  ubMeshcore Startup Fix Script"
echo "=========================================="
echo ""

# Check if device is connected
DEVICE=$(adb devices 2>/dev/null | grep -v "List of devices" | grep -v "^$" | awk '{print $1}')
if [ -z "$DEVICE" ]; then
    echo "ERROR: No Android device connected via ADB"
    echo "Please connect your Ubuntu Touch device and try again"
    exit 1
fi

echo "✓ Device connected: $DEVICE"
echo ""

# Check if app is installed
APP_NAME="ubmeshcore"
PACKAGE_NAME="click_ubmeshcore_ubmeshcore_0.2.1"

echo "Checking if app is installed..."
if adb shell click list 2>/dev/null | grep -q "ubmeshcore"; then
    echo "✓ App is installed"
else
    echo "✗ App is not installed"
    echo "Run: cd /home/developer/rust/ubMeshcore && clickable --arch arm64"
    exit 1
fi

echo ""
echo "Step 1: Setting AppArmor to complain mode..."
if adb shell aa-complain "$PACKAGE_NAME" 2>&1; then
    echo "✓ AppArmor complain mode set"
else
    echo "⚠ Trying with sudo..."
    adb shell sudo aa-complain "$PACKAGE_NAME" 2>&1
    if [ $? -eq 0 ]; then
        echo "✓ AppArmor complain mode set (with sudo)"
    else
        echo "✗ Failed to set AppArmor complain mode"
        echo "Try manually: adb shell sudo aa-complain $PACKAGE_NAME"
        echo "Password might be: 1234 or phablet"
    fi
fi

echo ""
echo "Step 2: Verifying AppArmor status..."
STATUS=$(adb shell aa-status 2>/dev/null | grep "$PACKAGE_NAME")
echo "  Status: $STATUS"
if echo "$STATUS" | grep -q "complain"; then
    echo "✓ AppArmor is in complain mode"
else
    echo "✗ AppArmor is NOT in complain mode"
    echo "Try: adb shell sudo aa-complain $PACKAGE_NAME"
fi

echo ""
echo "Step 3: Checking MIR_SOCKET..."
MIR_SOCKET=$(adb shell ls /run/user/*/mir_socket 2>/dev/null | head -1)
if [ -n "$MIR_SOCKET" ]; then
    echo "✓ Found MIR_SOCKET: $MIR_SOCKET"
else
    echo "⚠ No MIR_SOCKET found - app might have issues displaying"
    echo "Try checking running processes: adb shell ps aux | grep mir"
fi

echo ""
echo "Step 4: Checking Bluetooth..."
BT_STATUS=$(adb shell bluetoothctl status 2>&1)
echo "  Bluetooth status:"
echo "$BT_STATUS" | sed 's/^/    /'
if echo "$BT_STATUS" | grep -q "Powered: yes"; then
    echo "✓ Bluetooth is powered on"
else
    echo "⚠ Bluetooth is off - enable it first"
    adb shell bluetoothctl power on
fi

echo ""
echo "Step 5: Launching app..."
echo "  Attempting to launch $APP_NAME..."
if adb shell lomiri-app-launch "$APP_NAME" 2>&1; then
    echo "✓ App launched successfully"
else
    echo "⚠ App launch command returned error (might still be starting)"
fi

echo ""
echo "Step 6: Checking if app is running..."
sleep 2
PROCESS=$(adb shell ps aux 2>/dev/null | grep -i "ubmeshcore" | grep -v grep)
if [ -n "$PROCESS" ]; then
    echo "✓ App is running:"
    echo "$PROCESS" | sed 's/^/    /'
else
    echo "✗ App is not running"
    echo "Check logs with: adb shell cat /home/phablet/.local/share/ubmeshcore/ubmeshcore.log"
fi

echo ""
echo "Step 7: Checking logs..."
LOG_FILE="/home/phablet/.local/share/ubmeshcore/ubmeshcore.log"
if adb shell test -f "$LOG_FILE" 2>/dev/null; then
    echo "  Recent log entries:"
    adb shell tail -20 "$LOG_FILE" | sed 's/^/    /'
else
    echo "  No log file found at $LOG_FILE"
fi

echo ""
echo "=========================================="
echo "  Troubleshooting Summary"
echo "=========================================="
echo ""
echo "If app is running but not visible:"
echo "  1. Check MIR_SOCKET: adb shell ls /run/user/*/mir_socket"
echo "  2. Update main.cpp with correct UID if needed"
echo ""
echo "If app crashes or doesn't start:"
echo "  1. Check AppArmor: adb shell aa-status | grep ubmeshcore"
echo "  2. Check logs: adb shell cat /home/phablet/.local/share/ubmeshcore/ubmeshcore.log"
echo "  3. Try: adb shell aa-complain $PACKAGE_NAME"
echo ""
echo "If Bluetooth doesn't work:"
echo "  1. Enable Bluetooth: adb shell bluetoothctl power on"
echo "  2. Check adapter: adb shell hciconfig -a"
echo "  3. Restart Bluetooth: adb shell sudo systemctl restart bluetooth"
echo ""
echo "Done!"

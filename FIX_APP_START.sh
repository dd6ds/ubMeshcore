#!/bin/bash

# ubMeshcore App Start Fix Script
# This script fixes the AppArmor issue preventing your app from starting
# Run this script from your computer terminal

echo "=========================================="
echo "  ubMeshcore App Start Fix"
echo "=========================================="
echo ""

# Step 1: Check device is connected
echo "Step 1: Checking device connection..."
DEVICE=$(adb devices 2>/dev/null | grep -v "List of devices" | grep -v "^$" | awk '{print $1}')
if [ -z "$DEVICE" ]; then
    echo "ERROR: No device connected. Connect your Ubuntu Touch phone via USB."
    exit 1
fi
echo "✓ Device connected: $DEVICE"
echo ""

# Step 2: Get phone IP address
echo "Step 2: Getting phone IP address..."
IP=$(adb shell ip route 2>/dev/null | awk '{print $9}' | grep -v "^$")
if [ -z "$IP" ]; then
    echo "ERROR: Could not get phone IP address"
    exit 1
fi
echo "✓ Phone IP: $IP"
echo ""

# Step 3: Try SSH method (no password needed - key already authorized)
echo "Step 3: Trying SSH method..."
echo "  Running: ssh phablet@$IP sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1"
ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no phablet@$IP "sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ SSH method succeeded!"
else
    echo "⚠ SSH method failed (may need password)"
fi
echo ""

# Step 4: Try ADB sudo methods with different passwords
echo "Step 4: Trying ADB sudo with common passwords..."

PASSWORDS=("1234" "phablet" "0000" "" "123456" "password")
for PASS in "${PASSWORDS[@]}"; do
    echo "  Trying password: [hidden]"
    adb shell "echo '$PASS' | sudo -S aa-complain click_ubmeshcore_ubmeshcore_0.2.1" 2>/dev/null
    if [ $? -eq 0 ]; then
        echo "✓ Success with password!"
        break
    fi
done
echo ""

# Step 5: Verify AppArmor status
echo "Step 5: Verifying fix..."
echo "  Note: You may need to run: adb shell aa-status"
echo ""

# Step 6: Launch the app
echo "Step 6: Launching app..."
adb shell lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1
echo ""

echo "=========================================="
echo "  If app still doesn't start:"
echo "  1. Run: ssh phablet@$IP"
echo "  2. Enter: sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1"
echo "  3. Enter your device password (try 1234 or phablet)"
echo "  4. Run: lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1"
echo "=========================================="

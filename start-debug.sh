#!/bin/bash
# Debug script for ubMeshcore - sets AppArmor to complain mode and launches the app

echo "Setting AppArmor to complain mode for ubmeshcore..."
adb shell aa-complain ubmeshcore_ubmeshcore_0.2.1

echo "Launching ubmeshcore..."
cd /home/developer/rust/ubMeshcore
clickable --arch arm64 --no-review

echo ""
echo "To check AppArmor status:"
echo "  adb shell aa-status | grep ubmeshcore"
echo ""
echo "To check logs on device:"
echo "  adb shell tail -f /var/log/syslog | grep -i bluetooth"
echo "  adb shell tail -f /var/log/syslog | grep -i ubmeshcore"

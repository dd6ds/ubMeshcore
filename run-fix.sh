#!/bin/bash

echo "=========================================="
echo "ubMeshcore Fix - Simple Version"
echo "=========================================="
echo ""

IP=$(adb shell ip route 2>/dev/null | awk '{print $9}' | grep -v "^$")
if [ -z "$IP" ]; then
    echo "ERROR: Connect phone via USB first"
    exit 1
fi

echo "Phone IP: $IP"
echo ""
echo "Connecting via SSH..."
echo "When asked for password, try: 1234, phablet, 0000, or blank"
echo ""

# Use sshpass if available, otherwise regular ssh
if command -v sshpass &> /dev/null; then
    # Try with sshpass and common passwords
    for PASS in "1234" "phablet" "0000" ""; do
        echo "Trying password..."
        sshpass -p "$PASS" ssh -o StrictHostKeyChecking=no phablet@$IP "sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "✓ Success!"
            break
        fi
    done
else
    # Regular ssh - user will enter password manually
    ssh -t -o StrictHostKeyChecking=no phablet@$IP "sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1"
fi

echo ""
echo "Checking if it worked..."
echo "Run this on phone: aa-status | grep ubmeshcore"
echo "Should show: click_ubmeshcore_ubmeshcore_0.2.1 (complain)"
echo ""
echo "Then launch app: adb shell lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1"

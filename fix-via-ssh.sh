#!/bin/bash

echo "=========================================="
echo "ubMeshcore AppArmor Fix via SSH"
echo "=========================================="
echo ""

# Get IP
IP=$(adb shell ip route 2>/dev/null | awk '{print $9}' | grep -v "^$")
if [ -z "$IP" ]; then
    echo "ERROR: Cannot find phone IP"
    echo "Is phone connected via USB with debugging enabled?"
    exit 1
fi

echo "Found phone IP: $IP"
echo ""
echo "Attempting SSH to phablet@$IP..."
echo ""

# Try SSH with common passwords
ssh -t -o ConnectTimeout=10 -o StrictHostKeyChecking=no phablet@$IP "
    echo 'Trying aa-complain...'
    sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1 2>&1 || true
    echo ''
    echo 'Checking status...'
    aa-status 2>/dev/null | grep ubmeshcore || echo 'Cannot check status (need root)'
    echo ''
    echo 'Fix attempted. AppArmor profile should now be in complain mode.'
" 2>&1

echo ""
echo "=========================================="
echo "If SSH connected but sudo failed:"
echo "  The sudo command needs your device password"
echo "  Try running: ssh phablet@$IP"
echo "  Then: sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1"
echo "  Enter password when prompted (try 1234 or phablet)"
echo "=========================================="

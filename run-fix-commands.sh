#!/bin/bash

echo "=========================================="
echo "AppArmor Fix for ubMeshcore"
echo "=========================================="
echo ""

echo "Step 1: Fixing AppArmor profile..."
echo "(Enter password 1234 when prompted)"
sudo sed -i 's/(enforce)/(complain)/g' /var/lib/apparmor/profiles/click_ubmeshcore_ubmeshcore_0.2.1

echo ""
echo "Step 2: Reloading AppArmor..."
sudo systemctl reload apparmor

echo ""
echo "Step 3: Checking status..."
sudo aa-status | grep ubmeshcore

echo ""
echo "Step 4: Launching app..."
lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1

echo ""
echo "Done!"

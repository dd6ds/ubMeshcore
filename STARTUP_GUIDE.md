# ubMeshcore Startup & Debugging Guide

## Problem Summary

Your app builds and installs successfully but **does not start**. This is caused by **AppArmor blocking D-Bus access** and potentially **Mir socket connection issues**.

## Immediate Fix: Set AppArmor to Complain Mode

The bluetooth policy group is reserved and causes AppArmor to block the app. You MUST run this command to allow it to start:

```bash
# Connect your Ubuntu Touch device via ADB
adb devices

# Set AppArmor to complain mode for ubmeshcore
adb shell aa-complain click_ubmeshcore_ubmeshcore_0.2.1
```

If you get a permission error, try with sudo:
```bash
adb shell sudo aa-complain click_ubmeshcore_ubmeshcore_0.2.1
```

If sudo asks for a password, the default Ubuntu Touch password is typically **1234** or **phablet**. On some devices, you may need to:
```bash
adb shell su - phablet -c "aa-complain click_ubmeshcore_ubmeshcore_0.2.1"
```

## Verify AppArmor Status

Check if the profile is now in complain mode:
```bash
adb shell aa-status | grep ubmeshcore
```

Expected output:
```
  click_ubmeshcore_ubmeshcore_0.2.1 (complain)
```

If it still shows `(enforce)`, the command didn't work. Try rebooting the device and running the aa-complain command again.

## Launch the App

After setting AppArmor to complain mode:

```bash
# Option 1: Use clickable to launch
clickable --arch arm64

# Option 2: Manually launch via ADB
adb shell lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1

# Option 3: If the app still doesn't appear, try
adb shell click_ubmeshcore_ubmeshcore_0.2.1
```

## Check if App is Running

```bash
# Check if process is running
adb shell ps aux | grep ubmeshcore

# If running but no window appears, check for Mir connection errors
adb shell cat /tmp/ubmeshcore-early.log 2>/dev/null || echo "No early log"
```

## Debugging Startup Issues

### Step 1: Check AppArmor Denials

```bash
# Check for AppArmor denials in dmesg
adb shell dmesg | grep -i apparmor | grep -i ubmeshcore | tail -20

# Check all AppArmor denials
adb shell cat /var/log/syslog | grep -i apparmor | tail -50
```

If you see denials related to `/usr/bin/dbus-launch` or `bluetooth`, you need to adjust the AppArmor profile.

### Step 2: Check Logs

```bash
# Application log
adb shell cat /home/phablet/.local/share/ubmeshcore/ubmeshcore.log 2>/dev/null || echo "No app log"

# Stderr output (if available)
adb shell cat /tmp/ubmeshcore-early.log 2>/dev/null || echo "No early log"

# System log
adb shell journalctl -u ofono | tail -20
adb shell journalctl -u bluetooth | tail -20
```

### Step 3: Test D-Bus Access

```bash
# Check if D-Bus is running
adb shell dbus-launch --version

# Test D-Bus session bus
adb shell dbus-launch --autolaunch=$HOME/.dbus/session-bus/ echo "test"
```

### Step 4: Manual MIR_SOCKET Test

The app sets `MIR_SOCKET=/run/user/32011/mir_socket`. Your device might use a different UID. To find the correct socket:

```bash
# Find running Mir processes
adb shell ps aux | grep mir

# List mir sockets
adb shell ls -la /run/user/*/mir_socket 2>/dev/null

# If you find a different UID, update main.cpp:
# Change: setenv("MIR_SOCKET", "/run/user/32011/mir_socket", 1);
# To:     setenv("MIR_SOCKET", "/run/user/YOUR_UID/mir_socket", 1);
```

## Common Issues and Solutions

### Issue 1: App installs but doesn't start

**Symptoms:**
- `clickable` says "Started: ubmeshcore_ubmeshcore_0.2.1"
- No app appears on device
- No errors in clickable output

**Solution:**
```bash
# This is 99% likely AppArmor blocking the app
adb shell aa-complain click_ubmeshcore_ubmeshcore_0.2.1
adb shell lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1
```

### Issue 2: App starts but crashes immediately

**Symptoms:**
- App window flashes briefly and disappears
- Logs show "segmentation fault" or "aborted"

**Solution:**
```bash
# Check the crash log
adb shell cat /home/phablet/.cache/upstart/application-click-ubmeshcore*.log 2>/dev/null | tail -50

# Or check system logs
adb shell journalctl | grep -i ubmeshcore | tail -20
```

This might indicate a Bluetooth initialization issue. The lazy initialization should prevent this, but if it still happens, the AppArmor complain mode might be allowing it to start but then crashing on D-Bus access.

### Issue 3: App starts but Bluetooth doesn't work

**Symptoms:**
- App loads but scanning doesn't find devices
- Connect button doesn't work

**Solution:**
```bash
# Enable Bluetooth on device
adb shell bluetoothctl power on

# Check Bluetooth status
adb shell bluetoothctl status

# Check if Bluetooth adapter is available
adb shell hciconfig -a

# Restart Bluetooth service
adb shell sudo systemctl restart bluetooth
```

Also ensure AppArmor is in complain mode for full Bluetooth access.

### Issue 4: No incoming messages displayed

**Symptoms:**
- You can send messages
- Other users receive your messages
- You don't see their replies

**Solutions:**

1. **Click the "Refresh" button** in the Messages page (newly added)
2. **Check BluetoothManager logs** for PushMsgWaiting:
   ```bash
   adb shell cat /home/phablet/.local/share/ubmeshcore/ubmeshcore.log | grep -i "PushMsgWaiting\|PUSH_MSG"
   ```
3. **Manually trigger message sync** from the console:
   ```javascript
   BluetoothManager.syncNextMessage()
   ```
4. **Check companion protocol** is receiving messages:
   ```bash
   adb shell cat /home/phablet/.local/share/ubmeshcore/ubmeshcore.log | grep -i "channelMessage\|received"
   ```

## Testing Incoming Messages

1. Connect to a MeshCore node (Pair with a known device)
2. Join a channel (e.g., #test)
3. Have another user send you a message
4. **Click "Refresh"** in the Messages page
5. Check the console output for:
   - `PUSH_MSG_WAITING` (from companion protocol)
   - `PushMsgWaiting received` (from BluetoothManager)
   - `channelMessage` signal emission

If you see `PUSH_MSG_WAITING` but no `channelMessage`, the issue is in the message parsing. If you see neither, the issue is that the node isn't sending the message notification.

## Build & Install Commands

### Clean Build
```bash
cd /home/developer/rust/ubMeshcore
rm -rf build
clickable --arch arm64
```

### Quick Rebuild (after code changes)
```bash
cd /home/developer/rust/ubMeshcore
clickable --arch arm64 --build
```

### Force Reinstall
```bash
cd /home/developer/rust/ubMeshcore
clickable --arch arm64 --install --force
```

### Launch Only (after successful install)
```bash
adb shell lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1
```

## AppArmor Profile Debugging

### Check Current Profile
```bash
adb shell cat /var/lib/apparmor/profiles/click_ubmeshcore_ubmeshcore_0.2.1
```

### Temporarily Disable AppArmor for Testing
```bash
adb shell aa-disable /var/lib/apparmor/profiles/click_ubmeshcore_ubmeshcore_0.2.1
```

Note: This is NOT recommended for production use. Only use for debugging.

### Re-enable After Testing
```bash
adb shell aa-enforce /var/lib/apparmor/profiles/click_ubmeshcore_ubmeshcore_0.2.1
```

## What Changed in This Update

### New Features Added

1. **Contacts Page** (Page 4)
   - Accessible from Node Connection page or toolbar
   - Shows repeaters, nodes, and users
   - Filterable by type (All, Repeaters, Nodes, Users)
   - Search functionality
   - Back button to return to Node Connection page

2. **Refresh Button** in Messages page
   - Calls `BluetoothManager.syncNextMessage()`
   - Helps test incoming message display
   - Useful for debugging

3. **QML Navigation**
   - Contacts button added to Node Connection page
   - Contacts button added to bottom toolbar
   - All back buttons now work correctly

### Bug Fixes

1. **App Startup**
   - Lazy Bluetooth initialization (no early D-Bus access)
   - MIR_SOCKET environment variable set
   - AppArmor complain mode instructions provided

2. **Incoming Message Handling**
   - `syncNextMessage()` exposed to QML as Q_INVOKABLE
   - Debug logging for PushMsgWaiting (0x83)
   - Refresh button for manual polling

## Quick Test Checklist

- [ ] Set AppArmor to complain mode: `adb shell aa-complain click_ubmeshcore_ubmeshcore_0.2.1`
- [ ] Verify: `adb shell aa-status | grep ubmeshcore` shows `(complain)`
- [ ] Launch app: `adb shell lomiri-app-launch ubmeshcore_ubmeshcore_0.2.1`
- [ ] Check process: `adb shell ps aux | grep ubmeshcore`
- [ ] Check logs: `adb shell cat /home/phablet/.local/share/ubmeshcore/ubmeshcore.log`
- [ ] Test Bluetooth scan
- [ ] Connect to a node
- [ ] Join a channel
- [ ] Send a test message
- [ ] Ask someone to send you a message
- [ ] Click "Refresh" to see incoming messages

## If All Else Fails

1. **Reboot your device**
2. **Delete and reinstall the app:**
   ```bash
   adb shell click_ubmeshcore_ubmeshcore_0.2.1 uninstall
   cd /home/developer/rust/ubMeshcore
   rm -rf build
   clickable --arch arm64
   adb shell aa-complain click_ubmeshcore_ubmeshcore_0.2.1
   ```
3. **Check for updated versions** of clickable and Ubuntu Touch

## Useful ADB Commands

```bash
# List all installed click packages
adb shell click list

# Check app info
adb shell click info ubmeshcore

# View app logs
adb shell click logs ubmeshcore

# Check app permissions
adb shell aa-status

# View all running processes
adb shell ps aux

# Check Bluetooth status
adb shell bluetoothctl status

# Restart Ubuntu Touch shell
adb shell restart lomiri
```

---

**Last Updated:** 2026-10-15
**Version:** ubmeshcore 0.2.1

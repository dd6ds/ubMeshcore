# ubMeshcore

A Bluetooth LE companion app for [MeshCore](https://meshcore.co.uk) nodes,
built for Ubuntu Touch.

The app connects to a MeshCore companion radio over the Nordic UART Service,
speaks the MeshCore companion protocol, and lets you read and send messages on
the node's channels.

## Status

| Area | State |
|---|---|
| BLE scan, device list | working |
| GATT connect, NUS discovery, notifications | working |
| `APP_START` handshake, node name | working |
| Channel table read from the node | working |
| Receiving channel messages | working |
| Sending channel messages | implemented, command code unverified |
| Direct (contact) messages | not implemented |
| Rust core (`src/rust`) | builds, not linked (`USE_RUST_CORE=OFF`) |

## Requirements

- [Clickable](https://clickable-ut.dev) 8.7 or newer, with Docker
- An Ubuntu Touch device in developer mode, reachable over ADB
- A MeshCore node running companion-radio firmware

Everything else (Qt 5.12 cross toolchain, cargo) lives in the Clickable
container; nothing needs to be installed on the host.

## Build and install

```bash
clickable build      # produces build/aarch64-linux-gnu/app/*.click
clickable install    # pushes and installs over ADB
clickable logs       # follow the app's output
```

> **Kill the running app before installing.** `clickable install` replaces the
> binary but does not stop a running instance, so the old version keeps running
> and it looks like your changes had no effect:
>
> ```bash
> adb shell pkill -f ubmeshcore && clickable install
> ```

## Layout

```
CMakeLists.txt            Build; USE_RUST_CORE toggles the Rust library
clickable.yaml            arm64 / ubuntu-sdk-20.04, container dependencies
manifest.json             Click metadata (arch and framework patched by Clickable)
ubmeshcore.apparmor       Policy groups, including the reserved "bluetooth" one
qml.qrc                   QML is compiled into the binary, not installed separately

src/main.cpp              Entry point, QML context properties
src/bluetoothmanager.*    Discovery, GATT connection, NUS plumbing
src/companionprotocol.*   MeshCore companion frame encode/decode
src/rust/                 Rust core (ubmeshcore_core), currently unused

qml/main.qml              All pages
qml/AppLabel.qml          Label with an explicit colour
qml/AppField.qml          TextField with an explicit colour and background
```

`qml/NodeManager.qml`, `MessageView.qml` and `SettingsView.qml` are leftovers
from the original scaffold and are not loaded. `MessageView.qml` does not even
parse on Qt 5.12 — it uses `component`, a Qt 5.15 feature.

## Companion protocol notes

Frames carry one leading code byte and no length prefix; each BLE
write/notification is exactly one frame. Codes at or above `0x80` are
unsolicited pushes.

Verified against a live node (`DD6DS-Mobile`):

| Code | Direction | Meaning |
|---|---|---|
| `0x01` | TX | `APP_START` — `01 <ver> <6 reserved> <app name>` |
| `0x1f` | TX | `GET_CHANNEL` — `1f <index>` |
| `0x0a` | TX | `SYNC_NEXT_MESSAGE` |
| `0x03` | TX | `SEND_CHANNEL_TXT_MSG` — **unverified** |
| `0x05` | RX | `SELF_INFO`; node name is the trailing string |
| `0x11` | RX | Channel message |
| `0x12` | RX | Channel table entry |
| `0x80` | RX | Advert push — code plus 32-byte public key |
| `0x83` | RX | Message waiting (1 byte); answer with `SYNC_NEXT_MESSAGE` |
| `0x88` | RX | RX log — `88 <snr×4, int8> <rssi, int8> <raw LoRa packet>` |

Two field layouts worth knowing, because both cost a debugging round:

**`0x12` channel entry** — `<index:1> <name, NUL-terminated> <secret:32>`.
The name field is fixed width and is *not* zeroed by the firmware; channel 1
carried a stray `63` ('c') from a previous name after its NUL. Cut at the first
NUL, never read to the end of the payload.

**`0x11` channel message** — `<snr:1> <reserved:2> <channel_idx:1> <path_len:1>
<txt_type:1> <timestamp:4 LE> <text>`. The text starts at offset 10. Do not
search backwards for the trailing printable run: timestamp bytes such as
`6a cf b9 cb` pass a naive printable test and end up prepended to the message.

The `0x88` RX log is a firehose — the node forwards every LoRa packet it
overhears. It is decoded to a one-line summary and kept out of the UI.

## Platform constraints

- **Ubuntu Touch 20.04 ships Qt 5.12.** `import QtQuick 2.15` and friends make
  the whole file fail to load. 2.12 is the ceiling.
- **The QML QtBluetooth module tops out at 5.2** and has no BLE scanning, and
  there is no `DeviceDiscoveryAgent` QML type. All Bluetooth work happens in
  C++.
- **Qt 5.12 drops late device names.** It only reads the name from BlueZ's
  first event and ignores `PropertiesChanged(Name)`, so names resolved after
  the advertisement never arrive. `BluetoothManager` polls `org.bluez` over
  D-Bus to fill them in; without this the list shows MAC addresses twice.
- **`bluetooth` is a reserved AppArmor policy group.** Sideloading works;
  publishing to the OpenStore would need manual review. `clickable.yaml` sets
  `ignore_review_errors` so the build does not stop on it.
- **`QT_QUICK_CONTROLS_STYLE=Default` is set in `main.cpp`.** Otherwise the
  platform palette renders controls without explicit colours dark-on-dark, and
  text becomes invisible rather than merely ugly. Use `AppLabel` and `AppField`
  rather than bare `Label` and `TextField`.

## Enabling the Rust core

`src/rust` cross-compiles cleanly for `aarch64-unknown-linux-gnu` and exposes a
C ABI (`meshcore_app_new`, `meshcore_process_data`, …). It is not linked yet
because its path dependencies point at `../../../meshcore-info-display`, outside
the project directory, and Clickable only mounts the project into the container.

To use it, build the library with a wider mount and then configure with
`-DUSE_RUST_CORE=ON`:

```bash
docker run --rm -v "$HOME/rust":/w -w /w/ubMeshcore/src/rust \
  clickable/amd64-20.04-arm64:latest \
  cargo build --release --target aarch64-unknown-linux-gnu
```

## Debugging

The connect page shows a live trace of the last companion frames with decoded
code names — usually faster than digging through logs. For the full picture:

```bash
clickable logs
adb shell 'journalctl --user -n 2000 --no-pager | grep -a Companion'
```

## License

See `LICENSE`.

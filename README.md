# ubMeshcore

> [!WARNING]
> **Work in progress (WIP) — vibe coded.** This app was largely written with
> AI assistance and has a lot of bugs. Expect missing features, rough edges and
> things that break. Use it for experimenting, not as your only way to reach
> your MeshCore node.

A Bluetooth LE companion app for [MeshCore](https://meshcore.co.uk) nodes,
built for Ubuntu Touch.

The app connects to a MeshCore companion radio over the Nordic UART Service,
speaks the MeshCore companion protocol, and lets you read and send channel
messages, browse the nodes your radio has heard, and administer repeaters over
the mesh.

## Features

- **Channels** — read and send messages per channel. Each channel shows only
  its own messages; the history is stored on the phone and survives
  disconnects and app restarts. The last opened channel is reopened on start.
- **Reply** — long-press a received channel message for *Reply to …*
  (prefills `@[Sender] `, the mention format MeshCore apps use) or *Reply with
  route* (adds the path the message took, e.g. `@[Markus] 4 hops:
  cc4b→2a67→6228→db02`). A short tap still shows the hop-by-hop path.
- **Add / remove channels** — type a hashtag channel (`#z46` or `z46`) and
  tap *Add*; it is written into the first free slot on the node (secret
  derived from the name, as in every MeshCore client). A name that already
  exists shows *Open* instead. *✕* on a channel row removes it from the node
  after a confirmation, together with its history in the app. Tapping a row
  opens the channel. The page shows how many of the node's slots are used.
- **Unread indicators** — new messages in a channel you are not reading
  show a red count badge on that channel's row ("3 new messages"), a total
  on the *Channels* buttons, and on *‹ Channels* inside a channel for unread
  messages elsewhere. Opening a channel marks it read; counts survive
  restarts.
- **Offline queue** — messages the node cached while the app was away are
  fetched on connect, when the app returns to the foreground, and by a
  once-a-minute safety poll. They keep their original send time (with date if
  older than today), and the Messages page reports how many were fetched.
- **Message path** — each received channel message shows its route
  (`↳ 2 hops · 2a67 › 6228`); tap it for the hop-by-hop path with repeater
  names from the contact list and the SNR/RSSI of the last hop. Messages
  from the node's offline cache show the hop count only (see below).
- **Contacts** — the node's live contact table, built from received adverts
  and refreshed automatically when new adverts arrive. Shows type (repeater,
  companion, room), hops, last heard and an active indicator. Filters: type,
  search, *only active (24 h)*, *only favourites*.
- **Favourites** — star any contact; favourites sort to the top. Stored on the
  phone, so they survive re-syncs and restarts.
- **Repeater / room server admin** — login with password (admin or guest),
  status request (battery, uptime, noise floor, RSSI/SNR, packet counters,
  airtime) and remote CLI commands with quick buttons (`ver`, `clock`,
  `clock sync`, `neighbors`, `get name`, `get radio`, `get tx`, `advert`).
- **Advert** — send your own flood advert from the contacts page.
- **Auto-reconnect** — Lomiri suspends background apps, which drops the BLE
  link; the app reconnects to the last node when it is in the foreground again.

## Status

| Area | State |
|---|---|
| BLE scan, device list | working |
| GATT connect, NUS discovery, notifications | working |
| `APP_START` handshake, node name, clock sync | working |
| Channel table, receiving channel messages | working (verified on a live node) |
| Sending channel messages | working |
| Per-channel view, persistent history | working |
| Offline message queue | working (verified on a live node) |
| Contacts from adverts, favourites | working |
| Message path (hops, route, repeater names) | working (verified on live messages) |
| Unread indicators per channel | working |
| Reply / reply with route (long press) | implemented, not yet tested on the phone |
| Add / remove hashtag channels | implemented, not yet tested on the node; private (random-key) channels not supported yet |
| Repeater login / status / CLI | implemented; status block layout taken from repeater firmware, not the published spec |
| Direct (contact) messages | received and decoded, no UI yet |
| Rust core (`src/rust`) | builds, not linked (`USE_RUST_CORE=OFF`) |

## Requirements

- [Clickable](https://clickable-ut.dev) 8.7 or newer, with Docker
- An Ubuntu Touch device in developer mode, reachable over ADB
  (tested on 24.04-2.x; the click targets the `ubuntu-sdk-20.04` framework,
  which 24.04 still ships)
- A MeshCore node running companion-radio firmware

Everything else (Qt 5.12 cross toolchain, cargo) lives in the Clickable
container; nothing needs to be installed on the host.

## Build and install

The quickest way — build, install and launch on the phone in one go:

```bash
clickable --arch arm64
```

Or step by step:

```bash
clickable build      # produces build/aarch64-linux-gnu/app/*.click
clickable install    # pushes and installs over ADB
clickable launch
clickable logs       # follow the app's output
```

> **Kill the running app before installing.** `clickable install` replaces the
> binary but does not stop a running instance, so the old version keeps running
> and it looks like your changes had no effect. A suspended app ignores a plain
> SIGTERM until it is resumed, so use `-9`:
>
> ```bash
> adb shell pkill -9 ubmeshcore; clickable install && clickable launch
> ```

## Layout

```
CMakeLists.txt            Build; USE_RUST_CORE toggles the Rust library
clickable.yaml            arm64 / ubuntu-sdk-20.04, container dependencies
manifest.json             Click metadata (arch and framework patched by Clickable)
ubmeshcore.apparmor       Policy groups, including the reserved "bluetooth" one
qml.qrc                   QML is compiled into the binary, not installed separately

src/main.cpp              Entry point, QML context properties, resume handling
src/bluetoothmanager.*    Discovery, GATT connection, NUS plumbing, session,
                          contact sync, repeater admin, offline-queue drain
src/companionprotocol.*   MeshCore companion frame encode/decode
src/contactmodel.*        Contact list model (adverts), favourites
src/messagestore.*        Persistent channel message history, last channel
src/rust/                 Rust core (ubmeshcore_core), currently unused

qml/main.qml              Pages: discovery, connect, channels, messages, ...
qml/ContactList.qml       Contacts page with filters and favourites
qml/RepeaterAdmin.qml     Repeater / room server login, status, CLI
qml/AppLabel.qml          Label with an explicit colour
qml/AppField.qml          TextField with an explicit colour and background
```

`qml/NodeManager.qml`, `MessageView.qml` and `SettingsView.qml` are leftovers
from the original scaffold and are not loaded. `MessageView.qml` does not even
parse on Qt 5.12 — it uses `component`, a Qt 5.15 feature. The `config/`,
`database/`, `managers/`, `maps/`, `models/`, `network/`, `qr/` and `utils/`
directories are likewise not part of the build.

## Data on the phone

All paths are inside the app's AppArmor-writable directories:

| Path | Content |
|---|---|
| `~/.local/share/ubmeshcore/channel_messages.jsonl` | Channel history, one JSON object per line, newest 3000 kept |
| `~/.local/share/ubmeshcore/ubmeshcore.log` | App log (also in the journal) |
| `~/.config/ubmeshcore/ubMeshcore.conf` | Favourites, last opened channel |

Confined apps cannot write to `/tmp` or directly to `~/.local/share`; an
earlier version logged there, and its error path then called `fclose(NULL)` and
segfaulted on startup.

## Companion protocol notes

Frames carry one leading code byte and no length prefix; each BLE
write/notification is exactly one frame. Codes at or above `0x80` are
unsolicited pushes. The reference is the
[Companion Radio Protocol](https://docs.meshcore.io/companion_protocol/);
all multi-byte values are little endian.

Commands sent by the app:

| Code | Name | Payload |
|---|---|---|
| `0x01` | `APP_START` | `<ver> <6 reserved> <app name>` |
| `0x02` | `SEND_TXT_MSG` | `<txt_type> <attempt> <ts:4> <pubkey prefix:6> <text>`; `txt_type 1` = remote CLI |
| `0x03` | `SEND_CHANNEL_TXT_MSG` | `<txt_type> <channel_idx> <ts:4> <text>` |
| `0x04` | `GET_CONTACTS` | optional `<since:4>` (last `lastmod`) for incremental sync |
| `0x06` | `SET_DEVICE_TIME` | `<epoch:4>` — sent on every session start |
| `0x07` | `SEND_SELF_ADVERT` | `<1 = flood, 0 = zero-hop>` |
| `0x0a` | `SYNC_NEXT_MESSAGE` | — |
| `0x1a` | `SEND_LOGIN` | `<pub_key:32> <password, max 15>` (never written to the trace) |
| `0x1b` | `SEND_STATUS_REQ` | `<pub_key:32>` |
| `0x16` | `DEVICE_QUERY` | `<app protocol version = 3>` — reply carries `max_channels` |
| `0x1f` | `GET_CHANNEL` | `<index>` — sent for every slot `0 .. max_channels-1` |
| `0x20` | `SET_CHANNEL` | `<index> <name:32, NUL-padded> <secret:16>`; empty name + zero secret deletes (secret never written to the trace) |

Frames received (verified against a live node, `DD6DS-Mobile`, unless noted):

| Code | Meaning |
|---|---|
| `0x02` / `0x03` / `0x04` | Contacts start (`<count:4>`), one contact, end (`<most_recent_lastmod:4>`) |
| `0x05` | `SELF_INFO`; node name is the trailing string |
| `0x06` | `SENT` — `<type> <ack/tag:4> <suggested_timeout:4>` |
| `0x07` / `0x10` | Contact message v1 / v3 (v3 adds `<snr> <reserved:2>` in front) |
| `0x08` / `0x11` | Channel message v1 / v3 — see below |
| `0x0a` | `NO_MORE_MESSAGES` — the node's offline queue is empty |
| `0x0d` | `DEVICE_INFO` — `<fw_ver> [v3+: <max_contacts/2> <max_channels> ...]` |
| `0x12` | Channel table entry |
| `0x80` / `0x81` | Advert / path updated — `<pub_key:32>`; triggers an incremental contact sync |
| `0x83` | Message waiting (1 byte); answer with `SYNC_NEXT_MESSAGE` |
| `0x85` / `0x86` | Login success (`<permissions> <prefix:6> ...`, bit 0 = admin) / fail |
| `0x87` | Status response — `<reserved> <prefix:6> <status block>` |
| `0x88` | RX log — `<snr×4, int8> <rssi, int8> <raw LoRa packet>` |
| `0x8a` | New advert (manual-add mode), same layout as a contact |

Layouts worth knowing, because each cost a debugging round:

**Channel messages come in two layouts, and the node uses the old one.** The
code decides the layout — parse per code, never with one set of offsets:

```
0x08 (v1): <channel_idx:1> <path_len:1> <txt_type:1> <timestamp:4> <text>
0x11 (v3): <snr:1> <reserved:2> <channel_idx:1> <path_len:1> <txt_type:1> <timestamp:4> <text>
```

Parsing `0x08` with the v3 offsets reads a timestamp byte as the channel
(random indices like 191) and cuts the first three characters off the text.
The text is `"<sender name>: <message>"`. Do not search backwards for the
trailing printable run either: timestamp bytes such as `6a cf b9 cb` pass a
naive printable test and end up prepended to the message.

**Message path.** The channel message frame only carries the raw `path_len`
byte — `0bSSCCCCCC`, hop count plus hash size − 1 (so `0x42` = 2 hops with
2-byte hashes), `0xFF` = sent direct. The hop hashes come from the raw packet
in the `0x88` RX log:

```
<header: 0bVVTTTTRR> [transport codes:4 if RR = 0 or 3] <path_len> <path> <payload>
GRP_TXT (TTTT = 5) payload: <channel_hash:1> <mac:2> <ciphertext>
```

The app keeps the last 30 s of `GRP_TXT` packets and matches a decrypted
message to one by channel hash, identical `path_len` byte and ciphertext
length (`ceil((5 + text bytes) / 16) × 16`, used as a tie breaker). The
*oldest* candidate wins — the node keeps the first copy of a flood message —
and later copies of the same message are retired. Each hop hash is a prefix of
a repeater's public key; with 1-byte hashes several contacts can match, and
all candidates are shown. Messages fetched from the offline queue have no RX
log entry, so only the hop count is known.

The channel hash is the first byte of SHA-256 over the 16-byte channel secret
from `0x12` (verified: `#ping` → `0x28`, `#test` → `0xd9`; for hashtag
channels the secret is the first 16 bytes of SHA-256 of the name).

**Channel slots.** The node has `max_channels` slots (1, 8 or 40 depending on
the board). All of them are read on connect, empty ones included, so the app
knows the free slots; empty slots are hidden in the list. A new channel goes
into the first free slot. Message history is keyed by slot index, so adding
into or removing a slot also clears that slot's history and unread count in
the app. Outgoing frames are paced (≥ 40 ms apart): the firmware handles one
command at a time, and reading 40 slots back to back could overrun it.

**`0x12` channel entry** — `<index:1> <name:32> <secret:16>`.
The name field is fixed width and is *not* zeroed by the firmware; channel 1
carried a stray `63` ('c') from a previous name after its NUL. Cut at the first
NUL, never read to the end of the payload.

**`0x03` contact** — `<pub_key:32> <type:1> <flags:1> <out_path_len:int8>
<out_path:64> <name:32, NUL-terminated> <last_advert:4> <lat:int32 ×1e6>
<lon:int32 ×1e6> <lastmod:4>` = 147 bytes. Types: 1 companion, 2 repeater,
3 room, 4 sensor. `out_path_len -1` means no known path (flood). "Last heard"
uses `lastmod`, which the *companion* node stamps when the advert arrives — a
repeater with a wrong clock cannot skew it, which is why the app sets the node
clock on connect.

**Repeater status block** (`0x87`, from the repeater firmware's
`RepeaterStats`): `batt_mv:u16 tx_queue:u16 noise_floor:i16 last_rssi:i16
pkts_recv:u32 pkts_sent:u32 air_time:u32 uptime:u32 sent_flood:u32
sent_direct:u32 recv_flood:u32 recv_direct:u32 errors:u16 last_snr:i16(×4)
direct_dups:u16 flood_dups:u16 rx_air_time:u32`. Fields are read only as far
as the block reaches. Not yet confirmed against a live repeater.

The `0x88` RX log is a firehose — the node forwards every LoRa packet it
overhears. It is decoded to a one-line summary and kept out of the UI.

## Platform constraints

- **Build against Qt 5.12.** The `ubuntu-sdk-20.04` framework ships Qt 5.12,
  even though a 24.04 device runs 5.15. `import QtQuick 2.15` and friends make
  the whole file fail to load; 2.12 is the ceiling. `Connections` handlers use
  the old `onSignal: { ... }` form for the same reason.
- **Do not set `QT_QPA_PLATFORM`, `MIR_SOCKET` or Qt plugin paths in code.**
  `lomiri-app-launch` sets them for the running shell; hard-coding them breaks
  the app on other OS versions.
- **Lomiri suspends background apps.** The BLE link drops, and pushes that
  arrive meanwhile (e.g. `0x83` message waiting) are lost. The app reconnects
  or drains the node's queue on resume and polls the queue once a minute.
- **The QML QtBluetooth module tops out at 5.2** and has no BLE scanning, and
  there is no `DeviceDiscoveryAgent` QML type. All Bluetooth work happens in
  C++.
- **Qt 5.12 drops late device names.** It only reads the name from BlueZ's
  first event and ignores `PropertiesChanged(Name)`, so names resolved after
  the advertisement never arrive. `BluetoothManager` polls `org.bluez` over
  D-Bus to fill them in; without this the list shows MAC addresses twice.
- **`bluetooth` is a reserved AppArmor policy group.** Sideloading works;
  publishing to the OpenStore would need manual review. `clickable.yaml` sets
  `ignore_review_errors` so the build does not stop on it. Removing the group
  does not fix startup problems — it only breaks Bluetooth.
- **`QT_QUICK_CONTROLS_STYLE=Default` is set in `main.cpp`.** Otherwise the
  platform palette renders controls without explicit colours dark-on-dark, and
  text becomes invisible rather than merely ugly. Use `AppLabel` and `AppField`
  rather than bare `Label` and `TextField` (and an `AppLabel` next to a bare
  `CheckBox` instead of its `text`).
- **The adapter is created at startup**, not lazily on the first scan: the
  *Start Scan* button is only enabled once the adapter is known to be ready.

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
adb shell 'journalctl --user -f | grep aa-exec'        # live app output
adb shell 'dmesg | grep ubmeshcore | grep DENIED'      # AppArmor denials
```

QML load errors are written to the journal and to the app log; an app that
"does not start" is almost always a QML error, not AppArmor.

## License

See `LICENSE`.

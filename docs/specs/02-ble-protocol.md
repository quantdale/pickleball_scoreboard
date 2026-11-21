# Spec 02: BLE Protocol

Status: APPROVED (landscape redesign)
Owner: Dale
Consumers: ESP32 firmware, Android app
Depends on: Spec 01 (Manual scoring state)

## 1. Purpose

Defines the wire contract between the Android controller and the ESP32. The app
sends operator inputs and renders whatever state the board reports; the ESP32
never needs to know how the app draws anything.

## 2. Roles

- **ESP32 = BLE peripheral.** Advertises, hosts the GATT service and owns the
  Spec 01 state.
- **Android = BLE central.** Scans, connects, writes commands and subscribes to
  state. It never computes scores or serving state.

## 3. GATT structure

```text
Service UUID:            4fafc201-1fb5-459e-8fcc-c5c9c331914b
Command Characteristic:  beb5483e-36e1-4688-b7f5-ea07361b26a8   (Write, App → ESP32)
Legacy State:            beb5483e-36e1-4688-b7f5-ea07361b26a9   (Notify, ESP32 → App)
Manual State:            beb5483e-36e1-4688-b7f5-ea07361b26ac   (Notify, ESP32 → App)
Device advertised name:  "PickleScore"
```

`...26aa` remains the authenticated maintenance characteristic in OTA builds
(Spec 04 §8). `...26ab`, the portrait extended stream, is retired: landscape
firmware does not expose it and must not reuse the UUID.

The landscape controller subscribes only to Manual State. If a board lacks it,
the board runs portrait firmware: the controller reports that and never sends a
command. Legacy State keeps its six-field format for maintenance tooling and
diagnostics. Each stream has its own CCCD.

## 4. Commands (App → ESP32)

Each command is one ASCII write that the firmware accepts only as an exact match:

| Bytes | Spec 01 input |
|---|---|
| `+L` | `SCORE_UP(LEFT)` |
| `-L` | `SCORE_DOWN(LEFT)` |
| `+R` | `SCORE_UP(RIGHT)` |
| `-R` | `SCORE_DOWN(RIGHT)` |
| `S` | `NEXT_SERVE` |
| `C` | `SWITCH_COURTS` |
| `E` | `END_GAME` |
| `U` | `UNDO` |
| `0D` | `RESET(DOUBLES)` |
| `0S` | `RESET(SINGLES)` |
| `B1` … `B<max>` | Display brightness level (Spec 03 §2a), not a Spec 01 input |
| `KW` `KR` `KY` `KG` `KC` `KB` | Display color (Spec 03 §3), not a Spec 01 input |

No portrait command is valid. A portrait controller's rally bytes `L`/`R` and
three-byte reset (`0LD`, …) are malformed, and portrait firmware treats every
landscape command except `C`, `E` and `U` as malformed. Mixed versions therefore
cannot change scores by misinterpretation.

`B` followed by one digit selects a brightness level. `<max>` is the board's
build-time cap, reported in §5; a level of `0` or above the cap is malformed.
No scoring command starts with `B`. Landscape firmware without brightness
(`court-landscape-001`) and portrait firmware ignore `B` writes as malformed.

`K` followed by one palette letter selects the color of every lit LED. Any
other second byte, lower case, or a longer write is malformed. No scoring or
brightness command starts with `K`, and firmware without color control ignores
`K` writes as malformed.

## 5. State updates (ESP32 → App, Notify)

Both streams use fixed-order, comma-separated ASCII with canonical decimal
scores (no sign, no leading zeros):

```text
Legacy State:  <left>,<right>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>
Manual State:  <left>,<right>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>,<undo>

Example: "7,5,R,2,0,D,1"
  → left 7, right 5, right team serving, server 2, live, Doubles, Undo available
```

- `servingSide`: `L` or `R`. `serverNumber`: `1` or `2` (always `1` in Singles).
- `gameEnded`: `0` or `1`. `gameMode`: `D` or `S`.
- `undo`: `1` when history is available, otherwise `0`.

Scores are 0–99, so the longest Manual State payload (`99,99,R,2,1,D,1`) is 15
bytes and fits the default 20-byte Notify value.

The board notifies a stream when any field that stream carries changes, and on
initial synchronization. An ignored input does not notify.

### 5a. Display setting reports

The Manual State stream also carries the active brightness and color, each as
its own payload:

```text
Brightness:    B,<level>,<maxLevel>      Example: "B,2,4"
Color:         K,<code>                  Example: "K,R"
```

`level` and `maxLevel` are single digits with `1 ≤ level ≤ maxLevel`. The board
sends it after the Manual State payload on initial synchronization and whenever
an accepted `B` command changes the level; a command for the active level does
not notify. It is never sent on Legacy State. A state payload always starts
with a digit and has seven fields, so neither parser accepts the other: Android
2.0.0 ignores brightness reports under §7, and a controller that receives no
report after synchronizing treats the board as lacking brightness control.

The color report follows the brightness report on initial synchronization and
is sent whenever an accepted `K` command changes the color; a command for the
active color does not notify. `code` is one of the six letters in §4. A
controller that receives no color report hides the color choice.

## 6. Connection lifecycle

- The ESP32 advertises as "PickleScore" whenever powered and not connected,
  except during an authenticated maintenance session (Spec 04 §3). BLE activity
  cancels maintenance; accepted commands are never discarded.
- After connect, the ESP32 sends current state only after the central enables
  that stream's CCCD. Re-subscription resends state without changing it.
- Android reports ready only after a successful CCCD write AND one valid Manual
  State payload, in either order. Missing service/characteristics, failed CCCD
  writes, or a 15-second initial-sync timeout close the connection with an
  error. Stale callbacks from a closed GATT never affect a new connection.
- Display settings never gate readiness. Android shows only the
  board-reported brightness and color. It keeps at most one unsent write per
  setting (brightness, color), which yields to every queued scoring write and is
  replaced by a newer target for the same setting; a target equal to the write
  already in flight is not sent again. Taps made while a brightness write
  is outstanding step from that request, within the reported range. Scoring
  writes are never coalesced or reordered among themselves.
- Android sends acknowledged writes through a bounded FIFO with one write in
  flight. Firmware queues each command for its state-owning loop before
  returning ATT success; a full queue returns ATT insufficient resources. An ATT
  acknowledgment means transport completion, not acceptance by the scoring
  rules. A write without response after five seconds closes the session; queued
  commands are never replayed on reconnect. Successive taps are neither
  coalesced nor debounced. Prepared writes and writes without response are not
  supported.
- On disconnect the ESP32 keeps the state and display unchanged and advertises
  again.
- One central connection at a time.

## 7. Error handling

- The ESP32 ignores any command not listed in §4, without state change, history
  or Notify.
- Android ignores any payload that does not exactly match §5 for the stream it
  subscribed to, and waits for the next one.

## 8. Out of scope

Pairing, bonding and encryption (open characteristics are accepted for a
single-court local device); multiple simultaneous centrals; Wi-Fi or internet
score sync; firmware transfer over BLE. Local browser maintenance (Spec 04) is
not a scoring protocol.

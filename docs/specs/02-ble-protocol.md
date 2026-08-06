# Spec 02: BLE Protocol

Status: APPROVED
Owner: Dale
Consumers: ESP32 firmware, Android app
Depends on: Spec 01 (Scoring State Machine)

## 1. Purpose

Defines the exact wire format for communication between the Android app and
the ESP32 over BLE. This is the contract both sides code against — the app
never needs to know how the ESP32 computes state, and the ESP32 never needs
to know how the app renders anything. Each side only needs to know: what
bytes to send, and what bytes it'll receive.

## 2. Roles

- **ESP32 = BLE peripheral (server).** Advertises, holds the GATT service,
  owns the authoritative game state, runs the Spec 01 state machine.
- **Android app = BLE central (client).** Scans, connects, writes commands,
  subscribes to state updates. The app is a "dumb renderer" — it never
  computes scoring logic itself; it only sends button-press commands and
  displays whatever state the ESP32 reports back.

This matches the architecture decision from earlier: ESP32 is the single
source of truth for game state, so the preview in the app and the physical
LED display can never drift out of sync.

## 3. GATT structure

Two separate characteristics under one service — one Write-only for
commands, one Notify-only for state. This keeps each characteristic
single-direction and single-purpose: no ambiguity about which way a given
packet is going, and each side of the code only ever writes to one and
subscribes to the other.

```
Service UUID:            4fafc201-1fb5-459e-8fcc-c5c9c331914b
Command Characteristic:  beb5483e-36e1-4688-b7f5-ea07361b26a8   (Write, App → ESP32)
State Characteristic:    beb5483e-36e1-4688-b7f5-ea07361b26a9   (Notify, ESP32 → App)
Device advertised name:  "PickleScore"
```

Note: State Characteristic UUID above is the Command UUID with its last hex
digit incremented (`...a8` → `...a9`) purely so the two are easy to tell
apart at a glance during debugging. Any valid, unique UUID works — swap
these for freshly generated ones before shipping if preferred, just keep
both firmware and app in sync with whatever is chosen.

## 4. Commands (App → ESP32)

Each command is sent as a **single ASCII byte** — simplest possible format,
easy to debug by eye in a BLE sniffer. Bytes are chosen so no letter is
overloaded with two different meanings (the old handoff protocol reused
`S` for both "side-out" and would have collided with "start/reset" here —
avoided by using a distinct, non-alphabetic byte for Reset).

| Byte (ASCII) | Meaning | Spec 01 input |
|---|---|---|
| `'L'` | Left won the rally | `RALLY_WON_LEFT` |
| `'R'` | Right won the rally | `RALLY_WON_RIGHT` |
| `'U'` | Undo | `UNDO` |
| `'C'` | Switch courts | `SWITCH_COURTS` |
| `'E'` | End game | `END_GAME` |
| `'0'` | Reset (start new game) | see Section 4a — reset needs a parameter |

`'0'` reads unambiguously as "reset to zero" and doesn't collide with any
side/direction letter, which is why it's preferred over reusing a letter
like `S`.

### 4a. Reset needs a parameter (which side is 0-0-2) and a game mode

Per Spec 01 Section 4, starting or resetting a game requires knowing which
side is 0-0-2. Per Spec 01 Section 9a, the state machine also supports a
`gameMode` field (DOUBLES or SINGLES). Reset is therefore a **three-byte**
command:

```
Byte 1: '0'
Byte 2: 'L' or 'R'   — which side starts as 0-0-2
Byte 3: 'D' or 'S'   — DOUBLES or SINGLES
```

Example: app sends `0LD` → reset game, left side is 0-0-2, doubles mode.
Example: app sends `0RS` → reset game, right side is 0-0-2, singles mode.

This is a breaking change to the two-byte format used prior to SINGLES
mode support — both firmware and Android must be updated together, since
a two-byte reset would now be malformed input (missing byte 3).

## 5. State updates (ESP32 → App, via Notify)

Sent by the ESP32 every time the state changes (i.e. after processing any
command from Section 4). The app should not need to request this — it
arrives automatically once subscribed.

Format: a short, fixed-order ASCII string, comma-separated, so it's
human-readable in a BLE debug tool and trivial to parse on both sides.

```
<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>,<gameMode>

Example: "3,5,R,2,0,D"
  → left score 3, right score 5, right side serving, second server,
    game not ended, doubles mode
```

Field encodings:
- `leftScore`, `rightScore`: ASCII decimal integer, no leading zeros, no
  fixed width (e.g. "0", "3", "27" all valid).
- `servingSide`: single character, `L` or `R`.
- `serverNumber`: single character, `1` or `2`.
- `gameEnded`: single character, `0` (false) or `1` (true).
- `gameMode`: single character, `D` (DOUBLES) or `S` (SINGLES) — Spec 01
  Section 9a.

`gameMode` was appended as the sixth field when SINGLES support was built,
so the app always knows which mode the reported state is in. A payload with
any field count other than six — including the old five-field format from
before SINGLES — is malformed input per Section 7.

Confirmed: max BLE notify payload without special MTU negotiation is 20
bytes on the default ATT MTU. This message maxes out around 13–16 bytes even
with large multi-digit scores (e.g. "127,134,R,2,0,D" = 16 bytes), fitting
safely within the default without needing MTU negotiation. Still worth a
real test early in firmware bring-up rather than leaning entirely on this
math, since it costs one manual check.

## 6. Connection lifecycle

- ESP32 advertises continuously as "PickleScore" whenever powered and not
  already connected to a central.
- On app connect: ESP32 immediately sends one state Notify with the current
  state, so the app's preview syncs to whatever the board currently shows
  (important if the app reconnects mid-game, e.g. after being backgrounded).
- On disconnect: ESP32 keeps running/displaying the current state
  unchanged, and resumes advertising so the app can reconnect. The physical
  scoreboard is not dependent on an active BLE connection to continue
  displaying — only to receive new commands.
- Only one central connection supported at a time for v1. [Multiple phones
  controlling simultaneously is out of scope — confirm this is fine, since
  the court owner is the only expected controller per the original vision.]

## 7. Error handling / malformed input

- If ESP32 receives an unrecognized command byte: ignore it, no state
  change, no crash. Do not notify (nothing changed).
- If ESP32 receives `'0'`/reset without a valid three-byte sequence
  (missing byte 2 or 3, or byte 2 not `L`/`R`, or byte 3 not `D`/`S`):
  ignore the whole command, do not reset.
- App should treat any Notify payload that doesn't parse cleanly against
  Section 5's format as "ignore and wait for the next one" rather than
  crashing or showing stale/garbage values.

## 8. Explicitly out of scope for v1

- Pairing/bonding security (BLE pairing PIN, encryption) — using open BLE
  characteristics, acceptable for a single-court, single-controller local
  device. Revisit only if this becomes a problem in practice.
- Multiple simultaneous app connections.
- Any Wi-Fi/internet-based sync path — BLE only, per the original firm
  decision.
- OTA firmware updates over BLE.

## 9. Resolved decisions (formerly open questions)

1. **Characteristic structure**: two separate characteristics (Write-only
   command channel, Notify-only state channel), not one bidirectional
   characteristic. See Section 3.
2. **Command byte naming**: no overloaded letters. Reset uses `'0'` rather
   than reusing an alphabetic letter that could collide with a
   side/direction meaning. See Section 4.
3. **BLE MTU**: confirmed 20-byte default MTU is sufficient for the state
   message format; no MTU negotiation needed for v1. See Section 5.

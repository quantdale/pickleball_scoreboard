## Why

Courtside operators need the P10 display brighter when daylight washes it out,
and back to normal afterwards, without touching scoring. Brightness has been
fixed at 12/255 since bring-up.

## What Changes

- Firmware accepts `B1`…`B<cap>` on the existing command characteristic and
  applies the level in place through the HUB75 driver's OE timing. It reports
  `B,<level>,<cap>` on the Manual State stream and keeps the level in NVS.
- A fixed eight-step ladder (12…128/255, ×1…×16 light) with a conservative,
  uncertified build-time cap of level 4. 12/255 stays the default, and is
  already the driver's dimmest lit output.
- Android 2.1.0 adds a `−` / meter / `+` row to the Board menu. It shows only
  the board-reported level, coalesces rapid taps into at most one pending
  write, and lets every scoring write go first.
- Specs 01–03 updated; new `scripts/check_brightness_levels.py`.
- Not breaking: Android 2.0.0 ignores the new report, and older landscape and
  portrait firmware ignore `B` writes. This extends the ten-command set of the
  unarchived `landscape-manual-controller` change.

## Capabilities

### New Capabilities
- `display-brightness`: operator brightness levels, their BLE command and
  report, firmware limits, persistence and the controller's brightness control.

### Modified Capabilities

## Impact

- Firmware: `display_brightness.h`, `brightness_store.*`, `ble_command_parser.*`,
  `ble_command_ingress.h`, `ble_service.*`, `display_render.*`, `main.cpp`,
  `state_payload.h`; new Unity suite.
- Android: `BoardBrightness.kt`, `BoardCommand.kt`, `GattSession.kt`,
  `BleClient.kt`, `MainActivity.kt`, `ScoreboardScreen.kt`; JVM and Compose tests.
- No new GATT objects or heap allocations; OTA static DRAM +8 B; OTA memory
  floors unchanged.
- Hardware: physical current/thermal acceptance is required before relying on
  any level above 1 (Spec 03 §2a).

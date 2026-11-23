## Why

The owner asked for red LEDs, and court users found Undo rarely used. Wireless
updates from the landscape firmware are refused at `flash-begin` because the
running image leaves too little heap, so every release has fallen back to USB.

## What Changes

- One operator color for every lit LED, chosen from six full-channel colors
  (White default, Red, Yellow, Green, Cyan, Blue) on the Android Board menu,
  applied by the board, reported back and kept across reconnects and restarts.
- The Android controller drops its Undo key. Resume after End Game remains and
  sends Undo once per ended report.
- The firmware Undo ring keeps 32 positions instead of 2,048, returning
  6,048 B of static DRAM to the heap the upload path needs.
- OTA upload hardening: stop the per-close armed Wi-Fi timer leak, revert a
  cancelled install safely, report heap refusals as 503 with whether the
  inactive slot was written, tolerate short receive stalls, and refuse on the
  host when the measured upload demand would cross a device floor.
- Advertising is restarted by the loop when a start failed or never confirmed.
- The activity no longer restarts (and drops BLE) on configuration changes.

## Capabilities

### New Capabilities
- `display-color`: operator LED color, its command, report, persistence and controller selector.
- `ota-upload-headroom`: memory headroom, leak and failure-reporting requirements for wireless updates.

### Modified Capabilities
None in `openspec/specs/`; `docs/specs/01`–`03` are updated directly as the
authoritative behaviour specs.

## Impact

Firmware (`display_color.h`, `display_settings*.h/.cpp`, parser, BLE service,
renderer, `game_state.h`, `ota_maintenance.cpp`), Android 2.2.0 (code 9), the
deployment CLI and docs. Wire protocol additions are ignored by older firmware
and controllers.

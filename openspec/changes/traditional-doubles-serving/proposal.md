## Why

The prior house rule made server 1 unreachable. The owner now requires traditional USA Pickleball side-out scoring.

## What Changes

- Use the official opening second-server exception and hand every doubles side-out to server 1.
- Preserve opening status through Undo, Reset, court switches and End Game, without exposing it on BLE.
- Harden confirmed state, BLE readiness and queue defects; preserve portrait hardware and signed OTA controls.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `scoring-state-machine-tests`: Verify real doubles rotations and opening-state restoration across both implementations.

## Impact

ESP32 scoring and command handling, Android test mirror and BLE flow, deterministic tests, and operator documentation. No firmware deployment or BLE wire change.

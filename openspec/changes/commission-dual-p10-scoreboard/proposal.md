## Why

The owner has accepted two-panel hardware bring-up and requests the actual scoreboard, not another diagnostic. Production still assumes four panels and has a demonstrated initial BLE subscription race.

## What Changes

- **BREAKING (display geometry only):** Replace production display configuration with the accepted two-panel 64×16 GPIO/scan/brightness settings and chain mapping; preserve diagnostic profiles.
- Redesign shared layout for 10×14 score digits and one/two compact serving arrows in outer gutters; update firmware, Android preview, specs and pixel tests together.
- Fix initial/reconnect sync after subscription, Android readiness/write sequencing and firmware callback/loop state ownership without changing UUIDs, command bytes or six-field payloads.
- Build/test firmware and APK; document deployment, rollback and owner-observed E2E gates. Preserve approved scoring semantics, flag nonstandard doubles progression.

## Capabilities

### New Capabilities

- `dual-p10-scoreboard`: Production 64×16 display integration, pixel equivalence and commissioning gates.

### Modified Capabilities

- `ble-protocol`: Add subscription-time state synchronization and readiness requirements; existing reset/payload requirements unchanged.
- `shared-display-assets`: Replace the old 9×7 arrow with the approved compact 5×5 mirrored-source arrow; retain shared-source guarantees and unchanged digit glyphs.

## Impact

Firmware display/BLE adapters, shared display assets, Android renderer/BLE client, tests and specs. No new radio/network service, hardware changes, brightness increase or second-court implementation. Real firmware upload requires a fresh USB-only isolation confirmation.

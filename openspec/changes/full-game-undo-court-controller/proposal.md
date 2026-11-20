## Why

One-step Undo cannot recover a game after several incorrect entries. The Android remote should make the score, server, and two rally actions immediately obvious at courtside.

## What Changes

- Replace one-step Undo with a firmware-owned history without a fixed action count. Undo End and Reset as well as rallies and court switches.
- Add optional Undo status notifications without removing the legacy BLE stream or changing commands.
- Make Singles one continuous centered composition across both panels; preserve exact Doubles pixels.
- Keep the 1.1 controller layout and exact inline LED preview, with 1.2-inspired tactile keys and styling. Ship as Android 1.3.0.

## Capabilities

### New Capabilities

- `court-controller`: Courtside match hierarchy, connection gating, and authoritative Undo availability.

### Modified Capabilities

- `scoring-state-machine-tests`: Verify full history reversal in both modes.
- `shared-display-assets`: Allow Singles glyphs to span the panel seam.
- `ble-protocol`: Add an optional state stream with Undo status.

## Impact

Firmware scoring and GATT, Android Compose and GATT, and host/emulator tests. No changes to display mapping, OTA trust, or hardware.

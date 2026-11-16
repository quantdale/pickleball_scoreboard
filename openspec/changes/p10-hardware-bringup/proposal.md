## Why

No physical LED color test has passed. The owner now requires two independent two-panel scoreboards, but commissioning must start with one panel without changing production behavior.

## What Changes

- Add an isolated PlatformIO HUB75 color-cycle diagnostic under `hardware-tests/p10-bringup/`.
- Explicitly configure the owner's GPIO wiring, one-panel default, low brightness, serial diagnostics, and a separately built two-panel mode.
- Document upload isolation checks, single-panel safety gates, scan-pattern uncertainties, and a deferred architecture migration assessment.
- Do not change scoring, BLE, Android, shared assets, or approved production specs.

## Capabilities

### New Capabilities

- `p10-hardware-bringup`: Standalone LED commissioning firmware and staged physical verification gates.

### Modified Capabilities

None. The long-term two-unit display migration is assessment-only.

## Impact

New diagnostic project and this OpenSpec change only. Reuse existing PlatformIO/Python, Espressif platform, and HUB75 DMA 3.0.15. USB flashing replaces firmware on the selected ESP32 only after owner confirmation; no upload or serial-port opening is authorized before isolation.

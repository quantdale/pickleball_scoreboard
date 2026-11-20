## Why

Court 1 refuses normal scoring before even trying to allocate its first Undo block. The scoring heap check applies an OTA-derived reserve to a different heap-capability total.

## What Changes

- Give the inverse journal a deterministic byte budget, independent of live heap and fragmentation, without eviction.
- Acknowledge well-formed BLE writes only after queue admission; report journal refusals on the existing extended stream.
- Verify the real memory delta in the ESP32 link output and keep the existing OTA resource gates.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `ble-protocol`: specify queue backpressure before transport success.
- `scoring-state-machine-tests`: require reproducible bounded-journal and full-session regressions.

## Impact

Firmware, tests and release evidence only. No UI, display, GATT UUID, OTA trust, partition or deployment change.

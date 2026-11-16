## 1. Contract and preservation

- [x] 1.1 Fetch/audit repository, preserve bring-up work on feature branch, inspect scoring/BLE/rendering and authorize focused migration.
- [x] 1.2 Update display and subscription specifications/shared assets; preserve scoring rules and document outstanding doubles decision.

## 2. Integration

- [x] 2.1 Implement pixel-equivalent 64×16 firmware/Android renderers and accepted two-panel hardware configuration.
- [x] 2.2 Integrate loop-owned authoritative state, state-change rendering and post-subscription BLE sync.
- [x] 2.3 Fix Android readiness, queued writes, disconnect/reconnect cleanup and stale callbacks without wire changes.

## 3. Executed verification

- [x] 3.1 Run scoring/parser/serialization/sync-gate firmware tests and pixel bounds/snapshots, including 10/25/99 and panel mapping.
- [x] 3.2 Run Android unit tests with cross-language pixel equality and BLE readiness/order tests.
- [x] 3.3 Build real ESP32 firmware and installable debug APK; inspect memory/warnings and verify Android runtime where environment allows.
- [x] 3.4 Preserve rollback artifacts, document manual E2E acceptance and known seam/scoring limitations; enumerate target hardware.

## 4. Owner-gated commissioning

- [ ] 4.1 Confirm fresh isolated USB-only flashing setup/target identity before real scoreboard upload.
- [ ] 4.2 Complete owner-observed display/BLE/scoring/reset/reconnect hardware E2E checklist; do not infer pass from host tests.

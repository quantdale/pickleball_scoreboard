## 1. Diagnosis and fix

- [x] 1.1 Reproduce the installed refusal with the real admission code and archived Court 1 heap samples.
- [x] 1.2 Bound the journal and remove gameplay heap admission; preserve scoring and OTA gates.
- [x] 1.3 Tie BLE transport success to queue admission and retain queued work across reconnects.

## 2. Verification and delivery

- [x] 2.1 Verify long-session Undo, heap independence, byte exhaustion, BLE backpressure and snapshots.
- [x] 2.2 Build and audit the ESP32 profiles; test OTA, sign the release candidate and record hashes and measured memory.
- [ ] 2.3 Push to the existing Draft PR without changing Android or installing firmware.
- [ ] 2.4 Separately authorized hardware acceptance: check BLE/LED play, Undo and maintenance/candidate OTA memory margins.

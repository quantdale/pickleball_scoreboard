## 1. OTA headroom (blocking wireless updates)

- [ ] 1.1 Serial-trace one authenticated session on Court 1: per-stage heap from network-ready to login, status and close; find the ~20 KB dip at join/login
- [ ] 1.2 Bench build with Wi-Fi RX buffers reduced; measure network-ready, session and upload-body minimum
- [ ] 1.3 Measure an actual upload transient on 2.2 (bench board, or Court 1 only with exact owner approval); recompute the CLI gate from that evidence
- [ ] 1.4 Decide the HUB75 colour-depth option with the owner (panel re-acceptance)
- [ ] 1.5 One owner-approved install of the headroom build, then one owner-approved wireless write test

## 2. Close reply

- [ ] 2.1 Reproduce with serial and a host packet capture; determine where the 200 is lost
- [ ] 2.2 Fix and show at least 10/10 acknowledged closes

## 3. Windows networking

- [ ] 3.1 Clear or renew the stale 192.168.4.2 lease before use, unelevated where possible
- [ ] 3.2 Boot-window HTTP with advertising on: test coexistence preference or slower advertising, or drop the boot window
- [ ] 3.3 Scan-deadline handling when the AP starts late

## 4. Endurance

- [ ] 4.1 50-cycle maintenance endurance on 2.2 (settled heap slope, timerRecovered, close rate, BLE recovery)
- [ ] 4.2 Log loopTask stack high-water mark

## 5. BLE robustness

- [ ] 5.1 Loop-owned notify through esp_ble_gatts_send_indicate with retry-on-failure (F3, F4)
- [ ] 5.2 Execute Write reply (F6), after checking the activation characteristic
- [ ] 5.3 Android scan timeout and throttling handling

## 6. Recovery and tooling

- [ ] 6.1 Move chunked backup and on-chip verify scripts into tools/ with tests
- [ ] 6.2 Stale device-lock procedure and test
- [ ] 6.3 Plan for replacing the portrait fallback in ota_1

## 7. Physical acceptance (owner)

- [ ] 7.1 Lit panels: 0 ◀ 0, every color, flicker
- [ ] 7.2 Spec 03 §2a current, thermal and image test at levels 1–4 in White

## 8. Housekeeping

- [ ] 8.1 Trim AGENTS.md history into docs/reports links
- [ ] 8.2 Archive or close stale OpenSpec changes
- [ ] 8.3 Lint warnings and baseline cleanup

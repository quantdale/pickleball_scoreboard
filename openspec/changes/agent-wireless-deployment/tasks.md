## 1. Inspect and specify

- [x] 1.1 Inspect actual HTTP/signing/BLE/recovery/network state and define owner-requested security contract.
- [x] 1.2 Extend authoritative Spec04 for activation, verification and disruption authorization.

## 2. Existing firmware CLI

- [x] 2.1 Implement direct HTTP deployment, external registry, locking, signing/build and honest results.
- [x] 2.2 Implement reversible Windows WLAN/radio control, Ethernet/local routing verification and WSL handoff. Restoration/Ethernet exercised; WSL handoff remains implementation-only evidence, and successful live WPA connection is blocked.
- [x] 2.3 Test CLI/network/protocol failure paths with mocks before hardware deployment. 29 tests pass.
- [ ] 2.4 Prove existing-firmware wireless update via CLI with safe maintenance window and serial boot evidence. BLOCKED: Windows WPA authentication stalls before DHCP/HTTP; no legacy wireless upload occurred.

## 3. Authenticated BLE activation

- [x] 3.1 Implement expiring one-use target-bound HMAC BLE activation and Windows client with replay tests. Host policy and real incorrect/replayed proof rejection verified.
- [x] 3.2 Add confirmed health/image identity and safe repeated AP lifecycle; investigate shutdown warning. Fixed demonstrated1KB cleanup stack overflow to4KB; full idle timeout plus three activation/scoring cycles passed strict raw-log crash checks,2.4KB+ stack margin. SDK shutdown warning remains, not claimed fixed; health HTTP/load still blocked by WPA.
- [x] 3.3 Run firmware/signing/pixel/Android regressions and builds; sign application-only releases. 77 firmware,15 signing,42 Android,48 snapshots; production/OTA builds and audited rollback linkage pass.

## 4. Commission and hand off

- [ ] 4.1 Install activation-capable app via existing signed Wi-Fi OTA, without migration/provisioning. Wireless path BLOCKED. Explicit owner-permitted PhaseB USB app-only installation performed, then independently verified stack correction installed as agent-v2-a; no table/metadata/bootloader/NVS/fallback writes. This is NOT a wireless pass.
- [ ] 4.2 Prove two distinguishable BLE-triggered Wi-Fi updates after startup AP timeout, with confirmed health and restored Ethernet/WLAN.
- [ ] 4.3 Exercise unauthorized/replay/interrupted/invalid-signature handling and normal BLE scoring safely; report physical panel/rollback limitations separately. Real BLE incorrect/replay and initial-sync/rally/undo pass; Wi-Fi rejection/interruption, physical rollback and panels remain pending.
- [x] 4.4 Document exact command, results, protected configuration and recovery; preserve evidence and scoped local commits, no push/merge. Report explicitly retains WPA blocker, initial crash/failed runner, corrected stack evidence, compiled-vs-physical rollback distinction and remaining acceptance gates.

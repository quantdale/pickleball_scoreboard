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

## 5. Focused WPA investigation

- [x] 5.1 Preserve Windows drivers/interfaces/BSS/profiles/events before settings changes; reproduce association-success/security-authenticating failure with real RSN capture and restored Ethernet/WLAN. WPA2/CCMP, PMF-not-required, strong signal and100TU beacon verified. wlanreport denied without elevation; independent client/adapter unavailable.
- [x] 5.2 Correct concrete pinned-SDK dynamic-TX cache mismatch (zero cache with feature bit enabled versus Arduino's bounded4), add red/green guard, build/sign agent-wpa1 and verify78 firmware/29 CLI/15 signing tests plus48 snapshots. This is a candidate, not a physically proven WPA fix.
- [x] 5.3 Obtain fresh explicit electrical isolation/application-only install confirmation, verify current device/recovery and evaluate candidate on hardware. Authorized agent-wpa1 app-only write and independent verification passed, all unrelated sectors unchanged. **OTA/AP startup self-check failed**, AP closed; stopped per owner instruction before WPA/DHCP/HTTP or any further flash/restoration. See docs/reports/agent-wpa1-installation.md.
- [ ] 5.4 Prove actual authentication/IP/API, two signed wireless updates and confirmed health; resolve or delimit shutdown warning using measured evidence. See docs/reports/wpa-authentication-investigation.md.

## 6. Focused heap-admission correction

- [x] 6.1 Distinguish current INTERNAL heap from historical minimum and byte-accessible/contiguous heap, inspect original floor and actual SDK allocations/order. Dynamic TX skips static-TX pool; legacy Arduino BTDM retains a15,448-byte unused Classic-only controller region. Original failure's exact rejection stage/current-free value is NOT established.
- [x] 6.2 Boot OTA profiles with BLE-only controller, release only unused Classic RAM before initialization, retain all existing free-heap floors/cache4/security/recovery and add stage/rejection/fragmentation diagnostics with host fail-closed policy tests.
- [x] 6.3 Run final firmware/CLI/signing/Android/pixel/build regressions and independently verify signed agent-wpa2; preserve software-only checkpoint and scoped local commit.86 firmware/29 CLI/15 signing/42 forced-rerun Android tests,48 snapshots and all three profiles/actual rollback linkage pass. Initial wrong signing-test invocation ran0; supported entry point passes15. LSP timed out, not claimed clean. See docs/reports/ota-heap-admission-correction.md.
- [ ] 6.4 Obtain fresh owner electrical isolation and candidate-specific application-only installation authorization; reidentify target/verify recovery, then measure actual before/after heap, AP startup and BLE behavior. No installation/reset/restoration is authorized by software preparation alone.
- [ ] 6.5 Prove timeout plus repeated authenticated BLE maintenance/scoring cycles with settled same-stage heap/block comparisons and no progressive leak, stack failure or unexpected reset; then verify actual Windows WPA/DHCP/pinned HTTP/Ethernet restoration. No hardware pass inferred from host tests.

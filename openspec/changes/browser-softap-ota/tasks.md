## 1. Baseline and feasibility

- [x] 1.1 Verify Git/remote/baseline, actual SDK/partition/image and bootloader rollback evidence; preserve existing branches.
- [x] 1.2 Define explicit local OTA/authentication/gameplay/recovery contract and migration boundary.

## 2. Implementation

- [x] 2.1 Implement tested bounded maintenance policy, signed package validation and streaming inactive-slot transaction.
- [x] 2.2 Implement local credential provisioning, offline signing tools and embedded browser UI.
- [x] 2.3 Integrate bounded SoftAP/HTTP, BLE priority/admission, explicit install and deferred boot confirmation.
- [x] 2.4 Add opt-in partition/build profiles and verify actual OTA application sizes and recovery images.

## 3. Executed verification and handoff

- [x] 3.1 Run failure-injection, signing/container/security tests and existing firmware/Android regressions.
- [x] 3.2 Build distinguishable OTA artifacts, preserve baseline/recovery and document exact size/rollback/security evidence and limitations.
- [ ] 3.3 Commit locally on dedicated feature branch without merge/push; document provisioning and two-update commissioning.

## 4. Owner-gated physical acceptance

- [ ] 4.1 Obtain fresh isolation and explicit partition-migration approval before identifying/flashing/provisioning actual hardware.
- [ ] 4.2 Prove two consecutive wireless updates, rejection/interruption/rollback, real BLE/display and sustained memory stability with owner observation.

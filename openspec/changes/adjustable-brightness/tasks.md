## 1. Investigation

- [x] 1.1 Model the pinned driver's `setBrightnessOE` for this setup and choose a distinct ladder
- [x] 1.2 Record before/after firmware resource usage (static DRAM, flash, OTA slot headroom)

## 2. Firmware

- [x] 2.1 Ladder, cap, command parsing, restore clamp and save debounce (`display_brightness.h`)
- [x] 2.2 Shared FIFO admission and loop dispatch (`applyBoardCommand`), independent of GameState and Undo
- [x] 2.3 Apply through `setBrightness8`; report `B,<level>,<cap>` on Manual State after sync and changes
- [x] 2.4 NVS load before the boot frame and settled save, skipped during maintenance
- [x] 2.5 Unity suite for levels, malformed commands, independence, rapid changes, persistence; ingress test

## 3. Android

- [x] 3.1 Report parser, `B<n>` writes and range-limited stepping
- [x] 3.2 Coalesced, lowest-priority setting slot in the write queue; cleared on disconnect
- [x] 3.3 Board-menu control showing the reported level; "Needs board update" without a report
- [x] 3.4 JVM tests and Compose tests at 569×320, 914×411 and 1280×800 dp

## 4. Specs, verification and release

- [x] 4.1 Update Specs 01–03 and add `scripts/check_brightness_levels.py` to CI
- [x] 4.2 Regression: native, snapshots, scoring build audit, OTA build audit, OTA tool and CLI tests
- [x] 4.3 Signed APK 2.1.0 and in-place upgrade from 2.0.0 on an emulator
- [x] 4.4 Signed OTA candidate `court-brightness-001`, authenticated with the Court 1 key
- [ ] 4.5 Owner-authorized deployment (separate approval; source OTA heap blocker applies)
- [ ] 4.6 Physical current, thermal and image acceptance per Spec 03 §2a

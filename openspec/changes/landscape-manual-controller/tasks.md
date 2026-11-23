## 1. Preserve the portrait release

- [x] 1.1 Push branch `archive/portrait-1.3.1-court-undo-003` and tag `portrait-1.3.1-court-undo-003` at `14f4e40`
- [x] 1.2 Verify a fresh clone of the tag: identical tree, glyph/snapshot checks and 109 native tests pass

## 2. Specifications

- [x] 2.1 Rewrite Spec 01 for manual scoring, serve cycle and ring Undo
- [x] 2.2 Rewrite Spec 02 for the manual command set and Manual State stream
- [x] 2.3 Rewrite Spec 03 for the 64×16 landscape layout and mounts

## 3. Firmware

- [x] 3.1 Manual state, snapshot ring journal and handlers
- [x] 3.2 Exact command parser; Manual State characteristic replaces `...26ab`
- [x] 3.3 Landscape renderer, shared assets and mount transforms
- [x] 3.4 Rewrite scoring, parser, display and protocol suites; shared scoring fixture
- [x] 3.5 Oracle/C++ pixel fixtures, glyph check, MSVC host run, mutation checks
- [x] 3.6 Build `esp32dev`, `ota`, `ota_b`; linked memory and OTA build audits
- [x] 3.7 OTA CLI serial witness accepts the landscape boot position

## 4. Android

- [x] 4.1 `BoardCommand`, Manual State parser and portrait-firmware refusal
- [x] 4.2 Landscape screen per the owner's sketch, serve key and LED preview
- [x] 4.3 JVM tests: commands, parser, shared fixture oracle, pixels
- [x] 4.4 Compose tests on an emulator at 569×320, 914×411 and 1280×800 dp
- [ ] 4.5 Emulated-BLE journey against the firmware-logic virtual board (paused: emulator stopped for host memory)

## 5. Release candidates

- [x] 5.1 Signed APK 2.0.0 (same signer as 1.3.1), lint and verification
- [x] 5.2 Signed `court-landscape-001` OTA package verified against Court 1 trust
- [x] 5.3 Operator guide, release report and AGENTS.md
- [ ] 5.4 Owner-authorized Court 1 OTA, remount, boot witness and courtside acceptance (separate approval)

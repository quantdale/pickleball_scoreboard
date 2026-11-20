# PickleScore 2.0: landscape manual controller

Release candidates for the redesign requested after courtside testing: the board
shows LEFT SCORE | SERVING ARROWS | RIGHT SCORE on two 32×16 P10 panels, and the
Android controller sets scores and the serving arrow directly. Nothing was
installed on Court 1, no maintenance session was opened, and no hardware was
touched. Court 1 last authenticated `court-undo-003` on `ota_0`.

## Preserved portrait release

Branch `archive/portrait-1.3.1-court-undo-003` and annotated tag
`portrait-1.3.1-court-undo-003` point at `14f4e40`, which contains Android 1.3.1
(`19ea29f`) and `court-undo-003` (`c3a3abf`). A fresh clone of the tag from
GitHub had the identical tree (`53c7f4fb…`), and its glyph check, 72 pixel
snapshots and 109 native firmware tests passed.

## Decisions

See [Spec 01](../specs/01-scoring-state-machine.md),
[Spec 02](../specs/02-ble-protocol.md), [Spec 03](../specs/03-display-rendering.md)
and the [OpenSpec design](../../openspec/changes/landscape-manual-controller/design.md).

- **Manual scoring.** `+`/`−` per end (0–99), and the serve key cycles exactly
  L1 → L2 → R1 → R2 (Singles L1 ↔ R1). Boot and Reset are `0–0`, left serving,
  server 1. Score keys never move the arrow, and the arrow never changes a score.
- **Undo kept, never blocking.** Each record is the whole previous position
  (3 bytes), 2,048 of them in the same 6,144 B static journal. When full, the
  oldest drops. The portrait journal refused new actions when full; that would
  have blocked a `+` tap mid-match.
- **End Game / Resume.** While ended, the newest record is always End Game, so
  Resume is Undo. Switch Court, End Game, Reset and Singles keep their portrait
  meaning, adapted to manual serving.
- **Version safety.** New two-byte commands never overlap portrait bytes. A new
  Manual State stream (`...26ac`) lets the 2.0 app refuse portrait boards before
  writing. Legacy State (`...26a9`) is unchanged for the OTA tool, and `...26ab`
  is retired.
- **Mount.** The default is the verified portrait assembly turned clockwise
  (identity mapping). The boot frame `0 ◀ 0` distinguishes it from the two other
  plausible mounts, which are build options. See the
  [operator guide](../landscape-operator-guide.md).

## Verification

| Check | Result |
|---|---|
| Firmware Unity, PlatformIO native (MinGW) | 83/83, no warnings |
| Same suites with MSVC (`run_firmware_host_tests.py`) | 83/83 |
| Mutation checks: serve cycle, ring eviction, server-2 arrows, 99 clamp | All four caught |
| Python oracle vs actual C++ vs Kotlin pixels | 60 states × 1,024 coordinates identical |
| Firmware-recorded 600-step random walk replayed by the Kotlin oracle | Identical at every step |
| `esp32dev`, `ota`, `ota_b` builds; linked memory audit | Pass; OTA static DRAM 68,476 B (same as `court-undo-003`); Undo allocation-free |
| OTA build audit | Partition table and bootloader identical to the commissioned baseline; rollback path and deferred confirmation linked; 440,608 B slot headroom |
| OTA CLI host tests / package tests | 76/76 / 15/15 |
| Android JVM tests | 23/23 |
| Compose on API 35 emulator at 569×320, 914×411, 1280×800 dp | 8/8 each (see note) |
| Release lint | No unbaselined errors |
| OpenSpec `landscape-manual-controller --strict` | Valid |

Compose tests check the sketch's spatial order, 48 dp targets at 1.35× text,
all LED-preview pixels against the shared renderer, and the arrow direction and
count pixel-sampled from the serve key. They also check exact command dispatch,
confirmations, Resume, removal of controls without synchronization, and
unchanged control bounds while 0–32 writes are queued. Review of the captures
found five issues that were fixed and retested: header shift while sending, a
thin `−`, low-contrast "Game over", a squeezed preview on small screens, and
uneven bottom keys. One run failed with "No compose hierarchies found" just
after the emulator display was resized. Two immediate reruns at the same size
passed 8/8.

**Not verified:** the emulated-BLE journey (`e2e/journey.py`) against a virtual
board running the actual firmware scoring code was prepared but not run. The
emulator was stopped by the host for low memory. No physical panel, BLE radio or
ESP32 runtime heap was exercised.

## Artifacts

Session root: `D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261120-claude-landscape/`
(`SHA256SUMS` lists every file below and the screenshots). Built from `52f4cc4`.

| File | SHA-256 |
|---|---|
| `apk/PickleScore-2.0.0.apk` | `f06f7d7a2b21dc4270d18f9ca958e99a8e0a6f9ebc611a1eb3a80f5762a21a60` |
| `candidate/court-landscape-001.pksota` | `afe091087b2f91c064f21417245163fd7e7ab9c56f3f8e3002442ad7e02d46c5` |
| `candidate/firmware.bin` (1,591,008 B) | `922091fa718267db18d2f9aebd909026395010838d73b1fba523831df555d6ba` |

- APK: version code 7, `sensorLandscape`, not debuggable. Signed v2/v3 with the
  same local-distribution certificate as 1.3.1 (`548d4e27…ef06`, labeled Android
  Debug, not a Play signer), so it installs over 1.3.1 as an update.
- OTA package: signed with the Court 1 key, independently authenticated, and
  checked to match Court 1's installed public-key digest. It is a candidate only.
- Screenshots: `screens/{small-569x320,phone-914x411,tablet-1280x800}/`. Board
  pixels and wrong-mount previews: `pixels/`.

## Court 1 (separate authorization required)

1. Authenticate `court-undo-003` / `ota_0` / `VALID` without writing.
2. With approval of this exact package and target `ota_1` (replacing the
   `court-controller-002` fallback), install it and authenticate the running digest
   and `VALID`/confirmed/healthy.
3. Remount per the operator guide and check `0 ◀ 0`, then run the courtside check
   with Android 2.0.0. Do not reinstall to troubleshoot a wrong mount; use the
   guide's table first.

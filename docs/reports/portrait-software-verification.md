# Portrait software verification — 2026-11-19

**Software verified. Not deployed.** Branch: `feat/portrait-scoreboard-controller`,
based on `04c17df` (`feat/agent-wireless-deployment`, Draft PR #1). No Court 1 radio,
serial, power, scoring, flash or metadata operations were performed. The installed
recovery image is still the last independently verified `court1-return-002` / `ota_0`.

## Results

| Check | Result |
| --- | --- |
| Firmware host | 92 Unity tests on MSVC, eight suites. Exact clockwise corners, bijection of all 1,024 coordinates, panel/seam order, score bounds and overflow, arrow mirroring, Switch Courts vs. side-out, End/Reset/Undo; existing scoring, BLE and OTA tests unchanged. MSVC avoids the documented unrelated native MinGW dependency issue. |
| Complete pixels | 72 committed 16×64 snapshots; actual C++ output and Kotlin both match the independent inverse-sampling oracle. Ten glyphs, the shared arrow and 11 layout constants passed drift checks. |
| Embedded builds | `esp32dev`, `ota`, `ota_b`, `ota_budget`, `ota_ownership` and `ota_rollback_test` built. OTA A/B bootloader/partition audit and deferred-confirmation linkage passed. Bench rollback image is not for Court 1. |
| OTA tooling | 38 host tooling tests and 15 real OpenSSL signing/container regression tests passed (disposable keys). No deployment or activation path was invoked. |
| Android host/build | 43 JUnit tests, debug APK assembly, and APK signature verification passed. Lint: zero new errors, two warnings for the intentional phone portrait lock; existing baseline unchanged. |
| Android runtime | Three Compose tests passed via Gradle and twice via direct instrumentation: ordinary 1080×2400/420 dpi (~411×914 dp), compact 720×1280/360 dpi (320×569 dp), the latter at 135% font scale. Tests sampled actual Canvas output, asserted right-column target height and visibility, and exercised rapid rallies, transport vs. Notify feedback, destructive confirmations, ended state, reconnect gating and Disconnect. The production APK was also launched disconnected and checked via accessibility dump and runtime log. |

The initial emulator using SwiftShader disappeared mid-run; the same tests passed
with the repository's previously documented ATD host-GPU/no-Vulkan setup. Ready
and reconnect screenshots use test-supplied BLE states, not a live connection.
The production APK does not include test connection injection.

## Performance and design

The OTA app image is **1,590,736 bytes**, down **448 bytes** from `court1-return-002`;
slot headroom is **440,880 bytes**. Static DRAM **62,188 B** and IRAM **111,102 B**
match the baseline audit. The firmware now streams lit pixels, removing the
per-redraw vector allocation; no new frame, task, driver settings or OTA/BLE
firmware changes. Live heap, watermarks, BLE latency and reliability, and
outdoor readability are not inferred from build sizes or emulator timing.

Android caches pixels by notified state rather than rebuilding them on draw;
there is no continuous renderer. The serialized GATT FIFO permits repeated
legitimate taps. Acknowledgment and score are separate by design. A missing write
response fails the session rather than replaying an uncertain rally. Refero
references and the choice of large right-side actions, isolated live pixels and
status text are recorded in the [design rationale](../../openspec/changes/portrait-scoreboard-controller/design.md).

## Artifacts and candidate identity

Everything below is relative to the external session directory:

```text
D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261119-pi-portrait\
  deliverables/android/picklescore-portrait-debug.apk
  deliverables/firmware/<environment>/firmware.bin, firmware.elf
  deliverables/ota-court1/portrait-cw-001.pksota
  deliverables/ota-court1/verified-candidate.json
  screens/{ordinary,small}-{ready,reconnect}.png
  screens/launcher-small.png
  pixels/{2-3-L2-D,3-2-R2-D,88-99-L2-D,100--1-L2-D}.png
  artifact-manifest.json
```

- APK SHA-256: `56ec2dcd2a74afa0c4b398d1915f25756bb2fd56d8246007b2e772badae3c45b`
- Signed `portrait-cw-001` package SHA-256: `1864b8a42767f8b70f06472a6c3723fe26e2625d037e477fac8d43c7f7104f81`
- Application SHA-256: `b039c3ca6e6e0d4a41077facb21802698c91de5f0e6b477d8ddf164ccadee5dd`
- Planned scope: Court 1, inactive `ota_1`, from `court1-return-002` on `ota_0`.
  The package was prepared with the existing offline pack/verify functions and
  checked against the registered public trust. This is not installation approval.

See the [operator and migration guide](../portrait-operator-guide.md) for mounting,
installation gates and rollback. Illuminated panels, real Android BLE behavior
on the candidate, hardware rollback and sustained runtime margins remain pending.

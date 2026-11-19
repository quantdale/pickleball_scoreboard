# Singles display and Android controller 1.1.0

## Delivered

The maintained controller is the Compose portrait app introduced in PR #2 and
hardened in PR #3, not a separate V2 package. The archived horizontal and earlier
portrait builds both carry `1.0` (version code 1). Version `1.1.0` (code 2) keeps
the left preview, right-side rally buttons, Undo, Switch Courts, End Game,
mode/side Reset, and Board disconnect/maintenance menu. It keeps the subscribed
snapshot gate, acknowledged write FIFO, timeout/no-replay behavior, and now
clears the previous scan or session even if permissions vanish before replacing
it. Android accessibility reads the two-number Singles call without a server digit.

Court 1's installed `doubles-serve-001` uses the same GATT UUIDs, three-byte Reset
and six-field Notify: the controller can be upgraded first. Its Doubles preview
will match the installed firmware; its Singles preview intentionally differs
until the board is updated. See [Spec 03](../specs/03-display-rendering.md) for
zero-based pixel bands. No scoring logic, DMA wiring/mapping, OTA trust, flash
layout or Windows networking recovery changed. Singles follows USA Pickleball
2026 §§4.A, 5.A.1–3 and 6.B.1, verified against the official PDF.

## Artifacts and upgrade

All paths below are relative to `D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261119-pi-singles-final\`.

| File | SHA-256 |
| --- | --- |
| `apk/PickleScore-1.1.0.apk` | `c443e174b6410b4daa06b7009c51327bf54c73e91bc86cd212f210a01c2666d8` |
| `candidate/singles-display-001.pksota` | `e607a7b222761152c8d07a66c308fed741d1a4d0378016683f90cdea87ec3dbf` |
| `candidate/firmware.bin` | `af771f1a7a97e84621c716547147f88911fff68b2016377023a639595e19932b` |

The APK is a **non-debuggable release build** signed with the existing local
legacy Android Debug certificate for an in-place upgrade, not a store release.
Application ID: `com.example.pickleballscoreboard`; Android 8.0+ (API 26), target
35; verified APK Signature Schemes v2 and v3. Signer SHA-256:
`548d4e27a37909013053429dd013c1b3750f2728c149c9255df3e8788407ef06`.
All three archived APKs checked (including the horizontal delivery) have this
same ID and certificate. Emulator replacement of the old horizontal APK via
`adb install -r` kept the app ID, install time, data directory and both granted
Nearby Devices permissions. No owner phone was attached.

On the owner's phone, open the APK and choose **Update** (or, after approval,
run `adb -s <phone-serial> install -r <absolute-apk-path>`). Allow installation
from the chosen trusted source if prompted, then turn that allowance off again.
Do not uninstall first. For a read-only compatibility check with approved ADB,
use `pm path com.example.pickleballscoreboard`, pull its `base.apk` and run
`apksigner verify --print-certs` on it; compare the installed version in
`dumpsys package com.example.pickleballscoreboard` as well. If Android reports
an incompatible update, stop: a different signing identity cannot replace the
same package without a valid signing lineage. Prefer rebuilding with the
actual original key. If that key is unavailable, preserve the old APK and any
app-local data that can be exported, document permissions, and get explicit
approval before uninstall/reinstall; that fallback loses unexported local data
and permissions. Court scores live on the ESP32, not in the APK.

## Offline verification

- 102 firmware Unity tests, including both mode transitions and Undo; production
  `esp32dev`, `ota`, `ota_b` builds passed. No scoring, BLE firmware, boot, or
  hardware adapter source was modified. The renderer still streams lit pixels
  without a heap allocation or additional frame buffer.
- 72 × 1,024 cells match between the independent Python inverse sampler, the
  production C++ renderer and the production Kotlin renderer. The 36 Doubles
  goldens were not changed and also matched the archived accepted-baseline C++
  exporter. The baseline renderer blob is identical at `d67ec8c` and `cea0fc8`.
- 48 JUnit and three API 35 Compose tests passed; exact canvas pixels in both
  modes, semantic score calls, 64 dp rally targets, reset selection, confirmation,
  and reconnect control gating. Debug and release builds succeeded. Lint had
  zero unfiltered errors and two existing fixed-orientation warnings; baseline
  entries remain unchanged. Primary IDE diagnostics were inconclusive.
- The exact signed APK ran against a scripted Spec 02 peripheral through the
  emulator's virtual Bluetooth controller (real Android scan, GATT writes,
  CCCD, Notifies and link teardown; no physical RF). Verified both resets, ordered
  command bytes, score and arrow behavior, End Game/Undo, link loss and resync to
  the same match, normal disconnect, permission denial/regrant, and Bluetooth off.
  The GATT FIFO saturation and no-replay behavior are host-tested; the ESP32's
  real transport/queue under RF load and extreme-score MTU are not requalified.
- 69 OTA CLI and 15 signing/container tests passed. The signed candidate is
  1,590,960 bytes (16 above the accepted image), with 440,656 bytes free in a
  2,031,616-byte slot. Static data/BSS: 62,188 B; instruction RAM: 111,102 B.
  Independent package verification used existing Court 1 public trust and matched
  the embedded bytes. Audit confirmed unchanged bootloader, partition table and
  strong deferred boot confirmation. No new firmware heap/stack measurements,
  rollback execution, device contact, maintenance activation, upload or install.

## Owner acceptance (separate approval)

1. Upgrade the app and connect to idle Court 1. Confirm the initial live Doubles
   preview, disconnect/reconnect, and verify the current score returns unchanged.
2. Approve the exact signed package, idle-court reset and replacement of the
   expected inactive `portrait-order-004` slot. Use the installed OTA runbook;
   require `singles-display-001` / `ota_1` / `VALID` / confirmed / healthy and exact
   image hash. The currently accepted `doubles-serve-001` on `ota_0` remains the
   expected fallback after a successful trial. Do not rerun an uncertain install.
3. Reset Singles Left: expect two zeros and one left arrow on LEDs and app. Rally
   Left: `1 / 0 / ←`. Rally Right: `0 / 1 / →`. Switch Courts: `0 / 1 / ←`.
   Undo, End Game and Reset Doubles; confirm the expected `0 / 0 / 2` and opening
   side. Verify both panels are upright, dark at seam and edges, and readable.

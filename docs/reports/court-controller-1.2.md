# PickleScore 1.2 — release candidate

## What changed

- Full-session Undo is authoritative on the ESP32 and restores every accepted transition, even across End Game and Reset. No fixed history count or silent eviction; if safe RAM is exhausted the board refuses the new action without losing history. Reconnect preserves the journal; reboot clears it. The legacy GATT stream and write commands are unchanged. Only the new stream exposes Undo status.
- The Refero-informed controller shows the court and serving end above paired rally keys, with quick Undo and Switch Courts, a game menu and an exact LED preview. Scores still come only from the board.
- Singles uses 8-row outer margins and 5-row internal gaps. The opponent glyph crosses the seam without clipping. Doubles pixels and the mounting/HUB75 configuration are unchanged. The previous installed firmware always draws the server digit, even in Singles; syncing a new app cannot change it.

The last **authenticated** Court 1 build is [`doubles-serve-001`](court1-doubles-serve-install.md) on `ota_0` (`VALID`), application SHA-256 `aee46f3d25e4069fec3bde571cb2399194cbb49f5176e7dfed5c554f9eca101b`. The archived candidate bytes match that digest; its source renders the server digit unconditionally. This session did not re-query Court 1, open maintenance, install code or alter wiring. See [color and ghosting triage](p10-color-and-ghosting.md) for the separate optical symptoms.

## Verified offline and on the Android emulator

- 106 Unity tests, including 4,200-command unwind in each starting mode and injected history-allocation refusal; 51 Android JVM tests and four Compose instrumentation tests. Debug/release APK builds and Android lint passed. The Compose tests sample actual black/white pixels and exercise taps, confirmation, reconnect, and Undo availability.
- The signed APK discovered and subscribed to the extended stream on an emulator-only GATT peripheral exposing both UUIDs. A Singles Reset, two rallies, three Undo writes and a forced reconnect returned to boot Doubles; the reconnected state kept its history. This did not exercise ESP32 RF.
- 72 full pixel goldens shared by Python, Kotlin, and the C++ renderer. All 36 Doubles goldens match the archived Doubles exporter. An independent mount model and a driver-call spy verify both Singles seam rows are sent to the correct physical columns.
- `esp32dev`, `ota` and `ota_b` builds, OTA boot/partition/link audit, 69 OTA CLI tests and 15 signing/package tests passed. No changes to OTA trust or boot-confirmation policy. Runtime heap/radio margins still require bench verification.

## Artifacts

Local root: `D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261120-pi-full-game-undo-court-controller/`. Screenshots are in `screens/`.

| File | SHA-256 |
|---|---|
| `apk/PickleScore-1.2.0.apk` | `94f73fb8abfe503557f84ebabd4770f640421c947a8248a087b9424a8cc367b2` |
| `candidate/court-controller-002.pksota` | `3cc11b2815cd54ba3a9b0aa26569ccc5539bbfdd5164bdf7f27479fbbb7ca2fe` |
| `candidate/firmware.bin` | `1707f73e2de98ab92d69538ce01ffd7c6dc16e2e47a77f4c660df35856992ffd` |

The APK uses the same local signer as 1.1.0 (not a Play Store release). The OTA package was signed and independently verified against the existing Court 1 public trust. These are candidates, not deployed binaries.

## Court 1 acceptance (separate authorization)

1. With fresh no-write maintenance authorization, authenticate the board, running build, digest, slot and health while idle. If the currently running digest differs from the last known image, stop and reassess.
2. Separately approve the exact signed image and target slot before upload. On reboot authenticate `court-controller-002`, its exact digest and `VALID`/confirmed/healthy status. Preserve the old running image as fallback.
3. Pair the matching app and compare `4 / 4 / ←` and `4 / 4 / →` in Singles, including the middle glyph crossing the seam. Check asymmetric scores, two-digit readability and Doubles server order. Reconnect with history, then undo rallies, side-outs, a court switch, End Game and a mode-changing Reset back to the starting score. Compare the physical LEDs and LED preview. Do not infer lit-panel accuracy from snapshots or deploy a second time to troubleshoot a mismatch.

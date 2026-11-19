# Traditional doubles scoring: correction and offline verification

**Update:** Signed `doubles-serve-001` is now running on Court 1 (`ota_0`),
`VALID`/confirmed/healthy. See the [live deployment evidence](court1-doubles-serve-install.md).
The original correction and verification described below were offline.

## Root cause and correction

The old specification and both implementations set the server digit to 2 at every
full side-out. The firmware reproduced the failure starting from a normal game;
old tests reached server 1 only by forcing a synthetic state. [USA Pickleball
2026](https://usapickleball.org/docs/rules/USAP-Official-Rulebook.pdf) §§4.A,
5.B/5.B.4–6 and 6.B.2 reserve the opening designation of server 2 for the initial
serving rotation. Afterward a side-out gives the opponent server 1, and a rally
loss from server 1 hands service to the partner, without a point. No extra serve
attempt is awarded. Singles immediately sides out after a serving loss; it keeps
wire/display digit 2 solely for compatibility. See [Spec 01](../specs/01-scoring-state-machine.md).

With Left opening, after Left wins a point and Right wins the next rally:

| Event | Former house rule | Corrected portrait score call |
|---|---|---|
| Start | `0–0–2 ←` | `0–0–2 ←` |
| Left wins | `1–0–2 ←` | `1–0–2 ←` |
| Right wins | `0–1–2 →` | `0–1–1 →` |
| Left wins | `1–0–2 ←` | `0–1–2 →` |
| Left wins | `2–0–2 ←` | `1–0–1 ←` |

The internal opening flag and Undo history are per game. Switch Courts moves the
teams without changing service phase; End Game freezes it. Reset starts a new
rotation and Undo restores the exact old phase. The ESP32 owns match state and
counts the same six fields on BLE as before. Android stays a renderer: its test
mirror does not drive gameplay. Neither physical coordinates nor logical glyphs,
layout assets, display drivers, partition table or OTA trust changed.

## Hardening and audit limits

- Reproduced firmware Undo leaking across game instances. Undo now holds one
  local snapshot; fresh initialization clears history in both implementations,
  while Reset still saves history. Guarded score increment at integer maximum
  so a saturated point cannot wrap the score or consume a valid Undo.
- The Android FIFO limit formerly excluded the in-flight write. It now enforces
  its advertised bound; rapid writes remain ordered and are never replayed on
  reconnect. The GATT synchronization timeout is cancelled on Ready/disconnect
  rather than retaining stale GATT handles in the handler. Bluetooth adapter
  readiness inspection is now inside the permission-race exception boundary.
  Score parsing compiles its canonical regex once, not twice per notification.
- Audited the firmware queue/sync, Android GATT access and preview, render bounds,
  OTA admission, signing/readback, authenticated status and boot/rollback path.
  Kept the wire contract: an ATT acknowledgment is not application acceptance;
  under ESP32 queue saturation a command can still be discarded and logged, so
  the app must rely on notified state and never silently replay. No physical RF
  saturation, long-run heap/stack measurements, forced rollback, or assertion of
  leak-free operation. Existing SDK lifecycle uncertainty remains; no speculative
  OTA changes or weakening of authentication.

## Evidence and candidate

Passed: **100 Unity**, **47 Android JUnit**, **3 Android API 35 Compose tests**,
**72 full logical pixel snapshots** (unchanged fixtures), **59 OTA CLI tests**,
**15 signing/container tests**. The host verification includes 4,096 seeded rally
outcomes checked against an independent remaining-server model and a shared
C++/Kotlin command trace covering internal state, public projections, malformed
and rapid input, Undo and Reset. Firmware builds `esp32dev`, `ota`, and `ota_b`
passed. Android app and test APKs built; lint reported zero errors and two
existing portrait-lock warnings. Strict OpenSpec and Git whitespace checks
passed. Active IDE diagnostics were inconclusive, not counted as a clean pass.

The ESP32 candidate is 1,590,944 bytes with 440,672 bytes free in an OTA slot.
The build audit verified the production slot layout, unchanged bootloader and
linked deferred confirmation. Existing Court 1 public trust authenticated the
package and independently verified the embedded application bytes.

| Artifact | SHA-256 |
|---|---|
| `candidate/doubles-serve-001.pksota` | `e4ea6a0279c7074dd22155cf56e316f2276c85ebca322022c6314dffa6188855` |
| `candidate/firmware.bin` | `aee46f3d25e4069fec3bde571cb2399194cbb49f5176e7dfed5c554f9eca101b` |

Artifacts and raw evidence: `D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261119-pi-doubles-serving\`.
The later separately authorized OTA left `portrait-order-004` on `ota_1` as the expected fallback.
That write permission is consumed. Physical scoring acceptance remains a separate step.

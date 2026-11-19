# Portrait orientation correction — offline release evidence

`portrait-r180-002` corrects the owner-observed 180° mismatch relative to
`portrait-cw-001`. The physical adapter maps portrait `(x, y)` to horizontal
`(63 - y, x)` and then applies the same DMA half-block swap. The downstream panel
now shows the top score bands, the seam remains at viewer rows 31/32, and the
bottom arrow keeps its intended viewer-facing direction. Logical pixels, Android
UI and assets, scoring, BLE, DMA settings, wiring, brightness and OTA trust are
unchanged. This candidate was not installed or checked on illuminated panels.

## Verification

- Both new rotation checks failed against the old mapping and passed with the
  correction. The 93 Unity tests cover all 1,024 DMA coordinate pairs, both axes
  of the relative rotation, corner and seam mapping, panel order, an asymmetric
  complete frame, and both arrow directions.
- All 72 existing shared pixel snapshots matched the independent oracle and C++
  renderer; the unchanged Kotlin renderer passed the same fixtures. Android:
  43 JUnit and three Compose instrumentation tests on API 35, debug APK build,
  and lint (two existing portrait-lock warnings, no new errors).
- `esp32dev`, `ota`, `ota_b` and bench-only `ota_rollback_test` built. OTA A/B
  partition/bootloader compatibility and deferred-confirmation audit passed.
  38 CLI tests and 15 offline signing/container tests passed.

## Offline-signed candidate

- Build: `portrait-r180-002` (1,590,768 application bytes).
- Package SHA-256: `549b48945e7e453534881dd2bff04a19b0f7250118280df5335397a0289b224b`
- Image SHA-256: `b8aa04446f1e8b24a99905e714e6635ca2b85f5ae1385e684626c57127e608e0`
- P-256 package signature independently verified with the registered Court 1
  public key. Payload bytes match `firmware.bin`. Both signed predecessor and
  horizontal recovery packages were reverified; no trust or credential changes.

The package, raw application, ELF, proof JSON, and test logs are outside Git in
`D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261119-pi-portrait-r180\`.
Install candidate: `candidate/portrait-r180-002.pksota` (not the alternate or bench build).

## Deployment gates — not authorized here

1. With separate verification-only authorization, authenticate Court 1 and
   verify the exact running build, slot, digest and `VALID` / confirmed / healthy.
   The [prior install](court1-portrait-wireless-install.md) was acknowledged;
   owner-observed portrait pixels do not establish those fields. Expected source
   is `portrait-cw-001` on `ota_1`, SHA-256
   `b039c3ca6e6e0d4a41077facb21802698c91de5f0e6b477d8ddf164ccadee5dd`.
   Stop if the source differs or is unconfirmed.
2. Only after that check and separate owner approval for the exact candidate,
   overwrite of inactive `ota_0`, temporary downtime and score/Undo reset, use
   the pinned OTA workflow once with fresh physical safety confirmation. **Staging
   would replace the on-device horizontal fallback** `court1-return-002`. Keep
   its archived signed package; `ota_1` remains the expected on-device fallback.
   Planned application range: `[0x10000,0x1945F0)`, sector erase range
   `[0x10000,0x195000)`, normal OTA metadata only in `[0xE000,0x10000)`.
3. Independently verify the exact new image, `ota_0` and `VALID`/confirmed/healthy,
   then check BLE sync and verify physical `0 / 0 / 2 / ←`, both arrow directions,
   and panel/preview agreement with the existing APK. Nothing needs rewiring.
   Stop on uncertainty: no second upload, USB fallback, or automatic recovery.

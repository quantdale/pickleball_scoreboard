## Why

PickleScore 2.2 (`court-color-002` on Court 1, Android 2.2.0, Draft PR #8) is
usable for play, but wireless updates still are not dependable. Four approved
attempts on 2026-11-21 stopped safely before upload, so the firmware went on
over USB again. Several smaller reliability gaps and stale project items are
also open. This change collects all of them with their evidence so a fresh
session can fix them one at a time. Evidence is in
`docs/reports/release-2.2.md` and the session folder
`_AgentArtifacts/pickleball_scoreboard/sessions/20261121-claude-brightness/v22/`.

## What Changes

Investigate first, then fix with measurements before and after. **Never lower
a device memory floor or the CLI headroom gate to make an update pass.**

**A. OTA memory headroom (blocking).**
- The authenticated session heap measured 55,512–61,076 B. The CLI gate is
  60,456 B: measured demand above both device floors plus 3 KiB.
- Network-ready heap is 63.7–65.4 KB. The minimum dips to about 39 KB around
  login. Find what allocates about 20 KB at station join or login.
- Candidates, each needing a bench measurement:
  - Wi-Fi `dynamic_rx_buf_num` 8 → 4 and `static_rx_buf_num` 4 → 2.
  - HTTP task stack 8 → 7 KB (measured free 5.5 KB).
  - Free memory held between network-ready and login.
- Bigger option needing owner re-acceptance of the panels: HUB75 colour depth
  8 → 3 bits, about +16 KB.
- Re-measure the upload transient on 2.2 itself (the gate uses older-firmware
  figures). Only then recompute the gate from evidence.

**B. Close reply lost (intermittent).**
- `POST /api/close` timed out or reset in 4 of 4 sessions on `court-color-001`
  and 1 of 2 on `court-color-002`, despite the 500 ms flush.
- Device teardown is always clean. Find whether the server, lwIP linger or the
  Windows client drops it.

**C. Windows networking.**
- After association Windows reuses a cached `192.168.4.2` DHCP lease and port
  80 never answers (twice).
- HTTP never answers in the boot-time window while BLE advertising is still on
  (coexistence suspected).
- The AP once appeared after the CLI's 30 s scan deadline.
- An elevated run is needed for the static fallback.

**D. Endurance and leaks.**
- Rerun the 50-cycle maintenance endurance on 2.2. Confirm that the
  armed-timer fix (`timerRecovered`) ends the earlier −69 B/cycle drift.
  Closed-settled heap measured 114,736–119,884 B.
- Log the `loopTask` stack high-water mark.

**E. BLE robustness (from review, not yet fixed).**
- F3: unsynchronized Arduino peer-map access during disconnect (microsecond
  race).
- F4: a failed notify is never retried (stale phone view). Fix by sending
  through `esp_ble_gatts_send_indicate` from the loop with a dirty flag.
- F6: Execute Write from a non-app central is never answered (30 s link drop).
  Check that the HMAC activation characteristic does not use prepared writes
  first.
- Android: repeated LOW_LATENCY rescans can be throttled silently.

**F. Recovery and tooling.**
- `ota_1` still holds the portrait `court-controller-002`; there is no
  landscape fallback until a successful OTA.
- The COM11 CH340 drops bytes above 115,200 baud. Move the chunked read plus
  on-chip `verify_flash` backup scripts from the session folder into `tools/`
  with tests.
- Document a stale device-lock procedure and test it.

**G. Physical acceptance (owner).**
- Lit panels: the `0 ◀ 0` witness, each palette color, brightness levels 2–4
  current/thermal/image in White (Spec 03 §2a).
- Single-buffer repaint flicker, if visible.

**H. Housekeeping.**
- Trim the historical paragraphs in `AGENTS.md`.
- Archive or close stale in-progress OpenSpec changes (`adjustable-brightness`,
  `traditional-doubles-serving`, `singles-display-android-final`,
  `portrait-scoreboard-controller`, `p10-hardware-bringup`, ...).
- Lint: `ModifierParameter` warnings and stale `lint-baseline.xml` entries.
- AGP 8.5.1 is untested with compileSdk 35.

## Capabilities

### New Capabilities
None yet. Requirements will be added as delta specs once each investigation
settles a design; this proposal records scope and evidence.

### Modified Capabilities
None yet.

## Impact

Firmware (`ota_maintenance.cpp`, Wi-Fi/HTTP config, `ble_service.cpp`),
`tools/` (CLI, Windows networking, backup scripts), Android BLE scanning,
docs and OpenSpec housekeeping. Any hardware write needs the owner's
authorization for that exact transaction and fresh Mode B isolation for USB.

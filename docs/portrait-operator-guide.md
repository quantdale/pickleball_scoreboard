# Portrait PickleScore: operator and migration guide

**Last authenticated firmware: `portrait-r180-002` on `ota_0`.** The [deployment
report](reports/court1-portrait-r180-install.md) records the exact `VALID`, confirmed,
healthy image and BLE recovery from the isolated bench. This is not a fresh live
check. The [new offline-signed candidate](reports/portrait-installed-verification.md)
is not installed. Do not run another OTA or issue scoring commands without fresh approval.

## Operate

1. Open PickleScore, allow Nearby Devices (location on older Android), then tap
   **Scan for PickleScore** and select the intended board. Wait for **Connected / Ready**.
2. Read down: serving score, opponent score, server digit, arrow toward the physical
   serving side. **Rally Left / Right** means the team on that side won the rally,
   not “add one point.” The ESP32 decides whether it was a point or a side-out.
3. **Switch Courts** moves the teams, so only the arrow flips. A full side-out hands
   serve to the other team; the displayed scores exchange places and the arrow flips.
   The approved doubles full side-out still hands serve to server 2.
4. **Undo** restores one transition. **End Game** and **Reset** require confirmation;
   Reset asks for mode and the first serving side. Undo can recover either action.
5. A queued or acknowledged write is not evidence that the score changed. Check the
   live board preview or LEDs. If the link fails, do not retry an uncertain rally:
   reconnect, read the synchronized board and correct only if needed. To disconnect
   for maintenance, use **Board → Disconnect**. Maintenance is separate from the app.

## Mount and verify — separately authorized work

- Leave the assembly and wiring as found. The software reference puts the
  original downstream panel above the controller-nearest panel, viewed from the
  LEDs. Do not infer order from J1/J2 or compensate by moving anything. If the
  wiring or installed power is not already approved, stop and use the separately
  authorized [physical service gates](agent-ota-deployment.md#mode-b--isolated-usb-recovery-and-electrical-service).
- With separate no-write maintenance permission, pin the actual running digest
  and slot over BLE/SoftAP in the installed configuration. Photograph the full
  view from the intended spectator side, including panel seam and a landmark
  identifying physical left. Capture boot-like pixels if already present without
  reboot. Symmetric zeros are not enough to identify the transform.
- With separate idle-court scoring permission: start at `0 / 0 / 2 / ←`; Rally
  Left twice; Rally Right once for side-out, then three more times to score;
  Rally Left once for side-out. Expect `2 / 3 / 2 / ←`. Photograph it, then
  Switch Courts for `2 / 3 / 2 / →`. If digits are upright but the complete
  server/arrow block appears above both scores, suspect a raw block swap; if
  glyphs turn upside-down, arrow flips unexpectedly, or only one half is wrong,
  compare the external `pixels/orientation-2-3-L2-*.png` cards before any further
  candidate is considered. Stop for disagreement; do not try another rotation.
- Once orientation is confirmed, pair the candidate and matching APK. Check two
  dark score-gap rows, dark perimeter and seam on both panels at normal viewing
  distance; test side-out, Undo, End Game, Reset, both modes and a normal BLE
  reconnect. Two-digit/overflow and server 1 are covered offline; ordinary
  scoring under the current house rules cannot enter server 1. Longer load and
  rollback commissioning are separate; prefer a matching approved spare there.
- `ota_1` was not targeted and is the expected `portrait-cw-001` fallback. The
  prior horizontal `court1-return-002` image was replaced in `ota_0`; preserve
  its archived signed package. No further deployment or rollback is authorized.
- Require owner acceptance of illuminated panel order, physical safety, BLE play
  and runtime margins. Check cold-start DMA/BLE heap, maintenance admission and
  transfer stack watermarks, and observe bounded sustained play and reconnects.

## Rollback

Preserve the archived `court1-return-002` recovery image from `ota_0`: SHA-256
`077d888af21625335b6f3790b6b179928f2c1852c0f42b3444e3c0688917a00e`.
Its signed recovery package SHA-256 is
`1d1c56ed20ea1f5f2a0946f27200a40e9ad006274db894f2d7bc3acef95310f4`.
See the [verified baseline report](reports/ota-second-wireless-update.md).

If the portrait trial does not confirm, stop and verify the previous exact
`VALID` image; automatic rollback is linked but still needs real spare-board
acceptance. If the portrait image confirms but fails functional acceptance,
request separate approval for a pinned return to the archived known-good
horizontal image (do not attempt a reset to force rollback of a confirmed image).
Do not retry after uncertain install, write another slot, or perform USB recovery
without a new approved plan. A horizontal recovery image requires separate
acceptance of its mounting and matching legacy APK. Do not use a full-flash
backup for ordinary application rollback.

# Portrait PickleScore: operator and migration guide

**Current verified firmware: `portrait-cw-001` on `ota_1`.** The [verification-only
session](reports/court1-portrait-live-verification.md) authenticated the exact
running image, confirmed it `VALID` and healthy, and verified BLE recovery.
The orientation correction `portrait-r180-002` is signed offline only. Do not
start another OTA or begin physical acceptance of the correction without separate
owner approval.

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

- Keep the assembly and connections as found. The orientation correction is in
  the firmware only: the original right/downstream panel is the top (LED-side
  view), 180° from the earlier assumed portrait orientation. Do not swap the
  panels, ribbon connectors or GPIOs to compensate for `portrait-cw-001`. Use
  the existing [power and isolation gates](reports/court1-final-commissioning.md#1-reconnect-and-power-up)
  for any separately authorized physical work.
- Prefer a provisioned matching spare first. Validate upright `0 / 0 / 2 / ←`,
  rows around the seam dark, then `2 / 3 / 2 / ←` and Switch Courts (same digits, `→`).
  Test a full side-out with unequal scores, 10/25/88/99/overflow, both modes, Undo,
  Reset and End Game. Match the app pixels, use realistic rally bursts and check
  mid-game BLE disconnect/reconnect and post-maintenance initial sync.
- The last verified source is `portrait-cw-001` on `ota_1`; recheck its exact
  image and `VALID`/confirmed/healthy state before any future write. The correction
  would target inactive `ota_0`. **Even staging would replace the on-device
  horizontal fallback `court1-return-002`**, so preserve its signed recovery
  package and obtain explicit approval for that overwrite. If the source differs,
  stop and replan. Use the pinned [OTA procedure](agent-ota-deployment.md), no USB
  fallback or retries.
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

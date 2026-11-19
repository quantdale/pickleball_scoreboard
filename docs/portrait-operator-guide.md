# Portrait PickleScore: operator and migration guide

**Last authenticated firmware: `portrait-order-004` on `ota_1`, `VALID`, confirmed
and healthy with both panels powered.** The [installed OTA](reports/court1-portrait-order-install.md)
corrects the panel-block addressing and retains the score spacing. Await the owner
photo of `0 / 0 / 2 / ←`, with both zeros above the seam. The older
`portrait-installed-003` has the wrong block order; do not install it. Do not
run another OTA or issue scoring test commands without fresh approval.

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

- Leave the assembly and wiring as found. The corrected mapping sends scores
  to raw DMA x32–63 (top) and server/arrow to x0–31 (bottom). Do not infer order
  from J1/J2 or compensate by moving anything. If the
  wiring or installed power is not already approved, stop and use the separately
  authorized [physical service gates](agent-ota-deployment.md#mode-b--isolated-usb-recovery-and-electrical-service).
- With separate no-write maintenance permission, pin the actual running digest
  and slot over BLE/SoftAP in the installed configuration. Photograph the full
  view from the intended spectator side, including panel seam and a landmark
  identifying physical left. Capture boot-like pixels if already present without
  reboot. Symmetric zeros are not enough to identify the transform.
- After a separately authorized installed OTA and exact health confirmation,
  sync the matching portrait Android preview. Confirm `0 / 0 / 2 / ←`, with two
  dark score-gap rows, dark perimeter and dark seam. With idle-court scoring
  permission: Rally Left twice; Rally Right once for side-out, then three times
  to score; Rally Left once for side-out. Photograph `2 / 3 / 2 / ←`. Switch
  Courts and photograph `2 / 3 / 2 / →` from the same side with the seam and
  a physical-left landmark visible. Compare with Android and the reference
  frames. Reset only if the final state is the expected `3,2,R,2,0,D`; disconnect
  normally. Stop on any discrepancy; do not try another rotation or flash.
- Two-digit/overflow and server 1 are covered offline; ordinary scoring under
  the current house rules cannot enter server 1. Longer load and rollback
  commissioning are separate; prefer a matching approved spare there.
- `ota_0` was not targeted and retains `portrait-r180-002` as the expected
  fallback. The OTA replaced `portrait-cw-001` in `ota_1`. Preserve the archived
  signed packages, including horizontal `court1-return-002`. No further
  deployment or rollback is authorized.
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

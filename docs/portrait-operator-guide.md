# Portrait PickleScore: operator and migration guide

**Court 1: `doubles-serve-001` on `ota_0`, `VALID`, confirmed and healthy.**
The owner confirmed the portrait orientation and traditional Doubles play after
[installation](reports/court1-doubles-serve-install.md). `portrait-order-004`
on `ota_1` is the expected inactive fallback. The
[1.1.0 Android upgrade and signed Singles candidate](reports/singles-display-android-final.md)
are ready for separate acceptance; neither has been installed on owner hardware.
The older `portrait-installed-003` has the wrong block order; do not install it.

## Operate

1. Open PickleScore, allow Nearby Devices (location on older Android), then tap
   **Scan for PickleScore** and select the intended board. Wait for **Connected / Ready**.
2. Read down: serving score, opponent score, server digit (Doubles only on the
   matching firmware), and an arrow toward the serving side's physical end.
   **Rally Left / Right** selects the rally winner, not “add one point.”
3. **Switch Courts** swaps physical ends; only the arrow flips. A full side-out
   reverses the two scores and flips the arrow. In traditional Doubles, the opening
   rotation uses server 2; all full side-outs hand serve to server 1.
4. **Undo** restores one transition. **End Game** and **Reset** require confirmation;
   Reset asks for mode and the first serving side. Undo can recover either action.
5. A queued or acknowledged write is not evidence that the score changed. Check the
   live board preview or LEDs. If the link fails, do not retry an uncertain rally:
   reconnect, read the synchronized board and correct only if needed. To disconnect
   for maintenance, use **Board → Disconnect**. Maintenance is separate from the app.

## Mount and verify — separately authorized work

- Leave the assembly and wiring as found. The corrected mapping sends the top
  panel to DMA x32–63 and the bottom panel to x0–31. Do not infer order from
  J1/J2 or compensate by moving anything. If the
  wiring or installed power is not already approved, stop and use the separately
  authorized [physical service gates](agent-ota-deployment.md#mode-b--isolated-usb-recovery-and-electrical-service).
- With separate no-write maintenance permission, pin the actual running digest
  and slot over BLE/SoftAP in the installed configuration. Photograph the full
  view from the intended spectator side, including panel seam and a landmark
  identifying physical left. Capture boot-like pixels if already present without
  reboot. Symmetric zeros are not enough to identify the transform.
- For the separately authorized Singles trial, follow the [owner acceptance steps](reports/singles-display-android-final.md#owner-acceptance-separate-approval).
  For Doubles scenarios, follow [Spec 01](specs/01-scoring-state-machine.md#7-worked-example-doubles-left-opens).
  Stop on a discrepancy; do not redeploy or force rollback without new approval.
- Require owner acceptance of illuminated panel order, physical safety, BLE play
  and runtime margins. Check cold-start DMA/BLE heap, maintenance admission and
  transfer stack watermarks, and observe bounded sustained play and reconnects.

## Rollback

Preserve the archived `court1-return-002` recovery image from `ota_0`: SHA-256
`077d888af21625335b6f3790b6b179928f2c1852c0f42b3444e3c0688917a00e`.
Its signed recovery package SHA-256 is
`1d1c56ed20ea1f5f2a0946f27200a40e9ad006274db894f2d7bc3acef95310f4`.
See the [verified baseline report](reports/ota-second-wireless-update.md).

If the Singles trial does not confirm, stop and verify the previous exact
`VALID` Doubles image. Automatic rollback is linked but still needs real
spare-board acceptance. If the trial confirms but fails functional acceptance,
request separate approval for a pinned return to the archived signed
`doubles-serve-001`; reset cannot roll back a confirmed image. Do not retry
an uncertain install, write another slot, or perform USB recovery without a
new approved plan. Do not use a full-flash backup for ordinary app rollback.

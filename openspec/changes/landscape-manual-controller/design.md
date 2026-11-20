## Context

See proposal.md for motivation. The installed Court 1 board runs portrait
`court-undo-003` (rally scoring, full-session Undo in a 6,144 B static journal).
The OTA CLI's post-install BLE check subscribes to Legacy State (`...26a9`) and
validates its six-field format. The last physical witness showed the portrait
assembly with Panel 1 (raw x32–63) on top, raw x increasing upward.

## Goals / Non-Goals

**Goals:** a controller and board that operators can correct directly; exact
Android/LED agreement; no new heap use; unchanged OTA trust, partitions,
bootloader, rollback and maintenance; safe behavior with mixed versions.

**Non-Goals:** rule enforcement, game-point detection, animations, the deferred
P10 color investigation, any hardware write.

## Decisions

1. **Manual state, not a reinterpreted rally machine.** Each input maps to one
   field change; serving is a four-state cycle. Rally logic would contradict the
   owner's explicit request for manual serving control.
2. **Keep Undo, store whole positions.** Undo is owner-approved (`court-undo-003`)
   and still the only way to reverse Reset or End Game. Each record is a complete
   3-byte position (two scores ≤ 99 and four flags), so Undo is a copy with no
   per-input inverse logic. 2,048 records fill the same 6,144 B journal; the ring
   overwrites the oldest instead of refusing input, because a refused `+` would
   block the operator, while any visible field can always be corrected directly.
   Alternative (refuse when full, as in portrait) rejected for that reason.
3. **Resume = Undo.** While ended, the newest record is always the End Game record
   (play inputs are ignored and Reset clears the ended flag), so Undo resumes
   exactly. The ended screen labels this key Resume; no new command is needed.
4. **Keep Singles.** Mode changes only the serve cycle (two states, one arrow).
   It costs one Reset choice and avoids two dead taps per side change.
5. **Protocol versioning by characteristic.** A new Manual State UUID (`...26ac`)
   lets the 2.0 app detect portrait firmware before writing, while portrait
   1.3.x apps fall back to Legacy State and their rally/reset bytes are ignored.
   New two-byte commands never collide with portrait bytes in either direction.
   `...26ab` is retired and not reused. Legacy State remains for the OTA CLI.
6. **Mount as a build option.** Default mount 1 is the verified portrait assembly
   turned clockwise, which is the identity DMA mapping. Mounts 2 (counter-
   clockwise) and 3 (original bring-up order) are compile-time options. The boot
   frame `0 ◀ 0` distinguishes all three by eye. A runtime setting was rejected as
   new protocol surface for a one-time installation choice.
7. **Layout geometry.** 10×14 digits per end (single digits centered), 6×12 solid
   triangles centered with a 2-column gap, so paired arrows leave the panel seam
   dark. Two-column gaps keep 99s separate from the arrows.
8. **Android.** One `BoardCommand` enum is the only path to the wire. Controls are
   removed (not just disabled) when unsynchronized. Status chip width and bottom
   row height are reserved so queued writes and large text never move controls.

## Risks / Trade-offs

- [Physical mount differs from the default] → Spec 03 §5 witness, documented
  mounts 2/3, operator guide; installing a corrected build needs separate approval.
- [Oldest Undo entries vanish in very long sessions] → 2,048 steps is far beyond a
  match; scores remain directly correctable.
- [Mixed app/firmware versions] → controller refuses portrait boards; portrait
  apps cannot change landscape scores.
- [No physical BLE/LED acceptance in this change] → candidates only; owner acceptance
  on Court 1 after separately authorized OTA.

## Migration Plan

Install Android 2.0.0 over 1.3.1 (same signer, higher version code). With separate
authorization, OTA `court-landscape-001` into the inactive slot (`ota_1`, replacing
the `court-controller-002` fallback; `court-undo-003` remains on `ota_0`). Remount
the panels per the operator guide and check the boot witness. Rollback: OTA back to
the portrait images preserved at tag `portrait-1.3.1-court-undo-003` and reinstall
APK 1.3.1.

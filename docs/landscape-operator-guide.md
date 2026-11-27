# MT Pickle Park: landscape operator guide

The controller shows one team's score at each end of the board with the serving
arrows between them:

```text
LEFT SCORE   ◀ / ◀◀ / ▶ / ▶▶   RIGHT SCORE
```

One arrow is server 1, two arrows are server 2, pointing at the serving team. The
phone sets everything directly; the board does not apply rally rules. Use
Android **2.3.1** with the current `court-hardened-006` firmware. In the Bluetooth
scan, the board still appears as **PickleScore**. Older landscape pairs are listed under Compatibility; the landscape controller
refuses portrait firmware.

## Controls

| Control | Effect |
|---|---|
| `+` / `−` above and below a score | Add or remove one point for that end (0–99) |
| Center arrow key | Next serve: `◀1 → ◀2 → ▶1 → ▶2 → ◀1`; Singles alternates `◀ ↔ ▶` |
| Switch Court | Teams change ends: scores swap and the arrow flips |
| Board → End Game / Resume | Freeze the final score; Resume returns to play |
| Reset | New game at 0–0, left serving, server 1; choose Doubles or Singles |

There is no Undo key (Android 2.2): fix a mistake directly with `+`, `−` or the
arrow key. To catch up a board that missed points, tap the `+` keys until both
scores match the game; the serving arrow is set separately. The phone shows only what the board
reports, so a number changes after the board confirms it.

## Brightness

Use `−` / `+` around **Brightness** in the bottom row. The bars show the level
the board reports, from 1 (the normal 12/255) up to the board's limit, currently
4. Each step is roughly twice, 1.5× or 1.33× the light of the one before; level 1
is already the dimmest setting the panels can show. Brightness never changes the
score or the arrows, and the board remembers it through disconnects and
restarts (after about 3 seconds unchanged). Boards without brightness support
(`court-landscape-001`) do not show the control.

## Color

Tap **LED Color** in the bottom row and pick White (normal), Red, Yellow, Green,
Cyan or Blue. Every lit LED, scores and arrows alike, changes together. The
button swatch and the palette selection update once the board confirms. The board remembers the color like
brightness. A single color such as Red or Blue looks dimmer than White at the
same brightness level. Do not infer electrical approval from an apparent
brightness difference or the software cap. Faint close-range unintended pixel
illumination has been inspected and accepted by the owner as a minor visual
imperfection; see the [hardening report](reports/court1-usb-hardening-006.md)
and [panel fault note](reports/p10-physical-color-isolation.md).

Levels above 1 draw more panel current and are **not yet electrically
certified**: run the [physical acceptance check](specs/03-display-rendering.md#2a-brightness)
before relying on them, and return to level 1 if you see flicker, heat or any
change in colour.

## Mount and verify (separately authorized hardware work)

Installing firmware, opening maintenance and moving panels each need the owner's
authorization under [agent OTA deployment](agent-ota-deployment.md). This guide
does not authorize them.

1. **Mount.** Take the portrait assembly exactly as verified on Court 1 (Panel 1,
   wired to the ESP32, on top) and turn the whole assembly **a quarter turn
   clockwise**, as seen from the front. Panel 1 becomes the right half. Do not
   swap, rotate or re-cable either panel on its own.
2. **Boot witness.** After the board starts, it must show `0  ◀  0`: one zero at
   each outer end and a single arrow in the middle pointing left.
3. **Diagnose a wrong picture** without another install attempt:

   | What you see | Cause | Remedy |
   |---|---|---|
   | `0 ◀ 0` upright | Correct (mount 1) | — |
   | Upside-down digits, arrow pointing right | Turned counter-clockwise | Turn the assembly 180°, or build `PICKLE_LANDSCAPE_MOUNT=2` |
   | Zeros in the middle, arrow split at the edges | Panels in bring-up order | Swap panel positions, or build `PICKLE_LANDSCAPE_MOUNT=3` |

   Changing the mount by build option is a new signed image and needs a new
   authorization; moving the assembly does not.
4. **Courtside check.** With the phone connected: Left `+` ×7, Right `+` ×5 shows
   `7 ◀ 5`; four taps on the arrow key show `◀◀`, `▶`, `▶▶`, `◀`; Switch Court
   shows `5 ▶ 7`. Compare the board with the phone's LED preview at each step.

## Compatibility

| Phone | Board | Result |
|---|---|---|
| 2.3 | `court-hardened-006` or `court-color-001` | Brightness and LED Color on the playing screen |
| 2.3 | `court-brightness-001` / `court-landscape-001` | Only reported settings are shown |
| 2.2 | `court-color-001` | Brightness and color in the Board menu |
| 2.2 | `court-brightness-001` | Normal with brightness; no color row |
| 2.2 | `court-landscape-001` | Normal; Board menu shows "Needs board update" |
| 2.0 | `court-color-001` | Normal; Undo keeps the last 32 steps; settings reports ignored |
| 2.1 | `court-brightness-001` | Normal, with brightness |
| 2.1 | `court-landscape-001` | Normal; Board menu shows "Needs board update" |
| 2.0 | `court-brightness-001` | Normal; no brightness control (the board's report is ignored) |
| 2.0 | landscape | Normal |
| 2.0 | portrait (`court-undo-003` or older) | Phone refuses: "This board runs portrait firmware" |
| 1.3.x | landscape | Connects in legacy mode; rally and reset keys do nothing; Undo, Switch and End still work |

The OTA maintenance tool works with both firmware generations.

## Rollback and recovery

The latest authenticated record has `court-headroom-005b` in `ota_1` and the
valid landscape fallback `court-headroom-005` in `ota_0`. Neither slot holds
portrait firmware. See the [recorded deployment](reports/ota-reliability-followup.md)
and the [recovery procedure](agent-ota-deployment.md#mode-b--isolated-usb-recovery-and-electrical-service).

Portrait recovery is a separate owner-authorized operation, not a slot switch.
The tag `portrait-1.3.1-court-undo-003` preserves source; use the separately
protected and verified recovery artifacts, Android 1.3.1 and the
[portrait mounting guide](portrait-operator-guide.md) if that release is needed.

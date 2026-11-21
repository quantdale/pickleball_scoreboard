# Landscape PickleScore: operator guide

PickleScore 2.0 shows one team's score at each end of the board with the serving
arrows between them:

```text
LEFT SCORE   ◀ / ◀◀ / ▶ / ▶▶   RIGHT SCORE
```

One arrow is server 1, two arrows are server 2, pointing at the serving team. The
phone sets everything directly; the board does not apply rally rules. Use
Android **2.2.0** with board firmware **`court-color-001`**; Android 2.0.0 and
`court-landscape-001` or later still work together. Older portrait versions
refuse or ignore landscape commands (see Compatibility).

## Controls

| Control | Effect |
|---|---|
| `+` / `−` above and below a score | Add or remove one point for that end (0–99) |
| Center arrow key | Next serve: `◀1 → ◀2 → ▶1 → ▶2 → ◀1`; Singles alternates `◀ ↔ ▶` |
| Switch Court | Teams change ends: scores swap and the arrow flips |
| End Game → Resume | Freeze the final score; Resume (one tap) returns to play |
| Reset | New game at 0–0, left serving, server 1; choose Doubles or Singles |

There is no Undo key (Android 2.2): fix a mistake directly with `+`, `−` or the
arrow key. To catch up a board that missed points, tap the `+` keys until both
scores match the game; the serving arrow is set separately. The phone shows only what the board
reports, so a number changes after the board confirms it.

## Brightness

Open **Board** and use `−` / `+` next to **Brightness**. The bars show the level
the board reports, from 1 (the normal 12/255) up to the board's limit, currently
4. Each step is roughly twice, 1.5× or 1.33× the light of the one before; level 1
is already the dimmest setting the panels can show. Brightness never changes the
score or the arrows, and the board remembers it through disconnects and
restarts (after about 3 seconds unchanged). "Needs board update" means the board
runs firmware without brightness control (`court-landscape-001`).

## Color

Open **Board** and tap a swatch next to **Color**: White (normal), Red, Yellow,
Green, Cyan or Blue. Every lit LED, scores and arrows alike, changes together,
and the ring moves once the board confirms. The board remembers the color like
brightness. A single color such as Red or Blue looks dimmer than White at the
same brightness level and draws less current, so raising brightness one step is
fine within the board's limit. A stray pixel in the wrong color is the known
panel fault, not the color setting.

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
| 2.2 | `court-color-001` | Normal, with brightness and color, no Undo key |
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

## Rollback

The portrait release is preserved at Git tag `portrait-1.3.1-court-undo-003`.
`court-undo-003` remains in `ota_0` when `court-landscape-001` is installed into
`ota_1`; returning to it is a separately authorized wireless update, after which
reinstall Android 1.3.1 and return the panels to the portrait mount
([portrait operator guide](portrait-operator-guide.md)).

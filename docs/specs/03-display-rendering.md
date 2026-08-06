# Spec 03: Display Rendering

Status: APPROVED
Owner: Dale
Consumers: ESP32 firmware, Android app
Depends on: Spec 01 (Scoring State Machine), Spec 02 (BLE Protocol)

## 1. Purpose

Defines exactly what pixels light up, where, and in what color, for any given
game state. This is what makes the Android app's preview an exact mirror of
the physical LED panel — both render from this same layout spec, driven by
the same state (Spec 01) received over the same protocol (Spec 02).

This spec is now derived directly from the user's original HTML mockup
(uploaded reference), not guessed. The mockup is a **browser-based visual and
interaction prototype** — it establishes the exact font bitmap, arrow bitmap,
colors, and layout arrangement, but its JavaScript game logic is superseded
by Spec 01 wherever the two disagree (see Section 9, Note A) — Spec 01 is the
authoritative state machine for this project; the mockup predates the 0-0-2
starting-state decision.

## 2. Physical canvas

- 4 panels, 2×2 grid, each panel 32 pixels wide × 16 pixels high.
- Combined canvas: **64 pixels wide × 32 pixels high** (2 panels wide × 2
  panels tall, each contributing 32×16).
- Panel arrangement (from handoff, unchanged):
  ```
  Panel 0 (top-left)     Panel 1 (top-right)
  Panel 2 (bottom-left)  Panel 3 (bottom-right)
  ```
- Coordinate system for this spec: `(0,0)` = top-left pixel of the whole
  64×32 canvas, x increases rightward, y increases downward. All positions
  below are given in this canvas coordinate space, not per-panel.
- The Android app preview should render at the same 64×32 logical grid,
  scaled up for visibility on a phone screen (e.g. each logical pixel drawn
  as an 8×8 or 10×10 block — exact scale is an app-side rendering choice,
  not part of this spec, as long as the 64×32 grid and relative positions
  match).

## 3. Colors

Sourced directly from the mockup's CSS custom properties:

| Element | Mockup value | LED panel equivalent |
|---|---|---|
| Background / unlit pixels | `#000` | Black (`#000000`) — off/no color |
| Lit dot (score digits, arrows) | `#f2f2f2` (near-white), with a glow shadow | White (`#FFFFFF`) — LED panels have no glow/blur, so render lit pixels as flat white |
| Unlit dot (visible but off) | `rgba(255,255,255,0.055)` — a very faint gray dot, visible as a "grid" even when off | Not reproducible on a real LED matrix (no such thing as a dim/gray LED at low duty without PWM); see Section 9 Note B |
| Center divider line | `rgba(255,255,255,0.09)` — a thin, dim vertical line | Approximate as a dim white line if the driver supports intermediate brightness (e.g. `#202020`–`#404040`), otherwise omit or render as a single dim row/column of pixels at reduced PWM duty if the HUB75 driver supports it |

**Important:** the mockup's arrows and digits are rendered as **plain white
dots**, not green. The prior session's handoff (written before this mockup
was shared) described "green server arrows" — that description does not
match the actual reference file. This spec follows the real file: **arrows
are the same white color as the score digits**, not green. Flagging this
correction explicitly since it contradicts earlier project notes.

## 4. Layout regions

64×32 canvas divided into three regions, per the mockup's flex layout
(`.panel` containing `.side` (left) + `.center-line` + `.side` (right), with
`.arrow-center` absolutely positioned above both):

```
┌───────────────────┬───┬───────────────────┐
│   [arrow row,      │   │   [arrow row,      │
│    centered over   │ C │    centered over   │
│    each side]      │ E │    each side]      │
│                    │ N │                    │
│   LEFT SCORE       │ T │   RIGHT SCORE      │
│   (digits)         │ E │   (digits)         │
│                    │ R │                    │
└───────────────────┴───┴───────────────────┘
```

Key structural facts from the mockup:
- The board is split into two equal halves (left/right), each a flex
  column, with a thin center divider between them.
- Within each half, content is vertically stacked: **arrow(s) on top,
  score digits below** — confirmed by the mockup's `.side` being
  `flex-direction: column` and `.arrow-center` sitting at `top: 8%`,
  above the digit content which centers lower in the panel.
- The arrow region is shared/centered across the whole panel width in the
  mockup (`.arrow-center` is centered on the full panel, not per-side), but
  functionally only one side's arrow(s) render at a time (whichever side is
  serving) — so on the 64×32 canvas, render the arrow(s) horizontally
  centered over whichever half is currently serving.
- Digit-to-digit gap (multi-digit scores): mockup uses a small gap between
  glyphs (`.digit-slot` gap) — on the LED grid, use a **1-pixel gap** between
  adjacent digit glyphs, consistent with the "5×7 font, 1px gap" assumption
  already used in prior notes.
- Arrow-to-arrow gap (second server, two arrows shown): mockup uses a
  similar small gap (`.arrow-slot` gap) between multiple arrow glyphs — use
  **1-pixel gap** between the two arrow glyphs on the LED grid as well, for
  consistency.

### 4a. Score digits — exact font (5×7), confirmed

This is the real bitmap font from the mockup, not a placeholder. Each digit
is a 5-wide × 7-tall grid, `1` = lit pixel, `0` = unlit:

```
'0': 01110   '1': 00100   '2': 01110   '3': 11111   '4': 00010
     10001        01100        10001        00010        00110
     10011        00100        00001        00100        01010
     10101        00100        00010        00010        10010
     11001        00100        00100        00001        11111
     10001        00100        01000        10001        00010
     01110        01110        11111        01110        00010

'5': 11111   '6': 00110   '7': 11111   '8': 01110   '9': 01110
     10000        01000        00001        10001        10001
     11110        10000        00010        10001        10001
     00001        11110        00100        01110        01111
     00001        10001        01000        10001        00001
     10001        10001        01000        10001        00010
     01110        01110        01000        01110        01100
```

Multi-digit scores: render each digit's 5×7 glyph left-to-right with a
1-pixel gap between glyphs, per Section 4 above.

### 4b. Server arrow(s) — exact bitmap (7×9... actually 9×7), confirmed

Correction to a detail from the prior handoff: the arrow bitmap in the
actual mockup is **9 pixels wide × 7 pixels tall** (not 7×9 — width and
height were transposed somewhere in the earlier handoff's notes). Confirmed
directly from the mockup's `ARROW_RIGHT` array, which has 7 rows of 9
characters each:

```
ARROW_RIGHT:
000001000
000001100
111111110
111111111
111111110
000001100
000001000
```

`ARROW_LEFT` is this bitmap horizontally mirrored (each row reversed) — the
mockup generates it programmatically (`mirror()` function reversing each
row string) rather than hand-specifying a second bitmap. Firmware/app
implementations should do the same (derive left from right by reversing each
row) rather than maintaining two separate bitmaps that could drift out of
sync.

- Left-serving state → show `ARROW_LEFT`, centered above the left score.
- Right-serving state → show `ARROW_RIGHT`, centered above the right score.
- `serverNumber == 1` → show one arrow glyph.
- `serverNumber == 2` → show two arrow glyphs side by side, both pointing
  the same direction, 1-pixel gap between them (per mockup's `.arrow-slot`
  rendering multiple copies of the same bitmap in a row when `count > 1`).

### 4c. Center divider

- A single thin vertical line at the horizontal midpoint of the canvas
  (x=31 or x=32 of the 64-wide canvas), spanning the vertical extent of the
  score/arrow content — matches the mockup's `.center-line` (`top: 12%,
  bottom: 12%` of the panel, not full-bleed top-to-bottom).
- Rendered dim (see Section 3 note on center-line color) or, if the HUB75
  driver doesn't support intermediate brightness cleanly, as a sparse/dotted
  line (e.g. every other pixel lit) to approximate "dim" using only
  full-on/full-off pixels — see Section 9 Note B for this tradeoff.

## 5. Rendering triggers

- Display (both physical panel and app preview) re-renders any time a new
  state Notify arrives over BLE (Spec 02 Section 5).
- No animation, transition effects, or partial redraws required for v1 —
  full redraw of the affected regions on every state change is acceptable
  given the low update frequency (rallies happen seconds apart, not
  continuously).

## 6. App preview vs. physical panel — required equivalence

- Both must render from the identical state tuple
  (`leftScore, rightScore, servingSide, serverNumber, gameEnded`).
- Both must use the identical logical 64×32 grid and the same relative
  positions for score/arrow/center-line elements (Section 4).
- They are NOT required to use identical rendering technology (ESP32 drives
  real LEDs via HUB75 DMA; Android draws via Canvas/Compose) — only the
  logical output (which pixels are lit, what color) must match.
- Shared display assets exist in `shared/display_assets/`: `font_5x7.json`
  (5×7 digit glyphs), `arrows.json` (9×7 arrow glyphs), and `layout.json`
  (the 64×32 canvas size, score centers, arrow/digit tops, 1-pixel gaps, and
  divider x from Sections 2 and 4). The Android app parses these files at
  runtime; the firmware embeds hand-transcribed copies in
  `src/display_render_logic.cpp` — an approved deviation from this section,
  documented in that file's header comment and drift-checked by
  `scripts/check_glyphs.py`.

## 7. Explicitly out of scope for v1

- Any animation (e.g. flashing on score change, scrolling text).
- Brightness/dimming controls from the app (handoff already sets a fixed
  high brightness in firmware for indoor bright-light conditions).
- Any additional on-screen elements beyond score + arrows + center line
  (e.g. team names, timer, game number) — not requested, not building.

## 9. Resolved decisions and remaining notes

Most prior open questions are now resolved directly from the mockup:

1. **Colors**: resolved. White lit pixels (`#f2f2f2` → `#FFFFFF` on LED),
   black background, dim center line. **Correction**: arrows are white, not
   green — the "green arrows" description in the original project handoff
   did not match the actual reference file. See Section 3.
2. **2-digit score support**: resolved. Design cleanly for 0–99 (1 or 2
   digits per side). Scores of 100+ are an explicitly unhandled edge case —
   no special fallback (e.g. smaller font, truncation, scrolling) is being
   built for it. If a score reaches 3 digits in practice, display behavior
   is undefined/best-effort; not worth engineering for a case this
   unlikely in actual play.
3. **Arrow count for second server**: resolved. Two full arrow glyphs side
   by side, same direction, not a single arrow with a counter — confirmed
   directly from the mockup's `renderArrows(direction, count)` function,
   which literally appends `count` copies of the bitmap. See Section 4b.
4. **Reference file**: resolved — this spec is now built from the real file,
   not description-only guesses.

### Note A — mockup logic vs. Spec 01 (state machine authority)

The mockup's embedded JavaScript (`rallyWonBy`, `undo`, `reset` functions)
implements *similar* side-out scoring to Spec 01, but differs in one
material way: it initializes `serveNumber: 1` and has no equivalent of
Spec 01's 0-0-2-start decision — it models the traditional "first server of
the game only gets one serve" behavior that Spec 01 explicitly removed per
your instruction. **Spec 01 remains authoritative for all game-state logic.**
This mockup is a visual/layout reference only; do not port its `rallyWonBy`/
`reset` logic verbatim into firmware or app code.

The mockup's single-step undo (`history` array capped at length 1, via
`if (history.length > 1) history.shift()`) DOES match Spec 01 Section 5a's
single-step undo decision — that part is consistent and can be used as a
reference implementation pattern.

### Note B — unlit "visible dot grid" doesn't map to real LEDs

The mockup shows a faint dot at every grid position even when unlit (a
`rgba(255,255,255,0.055)` dot), which is a screen/CSS aesthetic choice to
show the dot-matrix grid structure. Real LED matrix pixels are either lit or
fully off — there's no "very faint but visible" state without deliberately
driving a low PWM duty cycle, which is unnecessary complexity for this
project. **Decision for LED implementation: unlit pixels are simply off
(black), full stop** — the app preview may optionally replicate the
faint-grid aesthetic for visual polish since it costs nothing on a phone
screen, but the physical panel will not attempt this, and the two are not
required to match pixel-for-pixel on this one cosmetic point (Section 6's
equivalence requirement applies to lit content — score/arrows/divider — not
to this unlit-grid styling choice).

All open questions for this spec are now resolved. No remaining blockers
before firmware/app implementation begins.

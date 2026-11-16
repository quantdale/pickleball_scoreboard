# Spec 03: Display Rendering

Status: APPROVED — 64×16 migration authorized by owner's scoreboard commissioning task
Owner: Dale
Consumers: ESP32 firmware, Android app
Depends on: Spec 01 (unchanged scoring), Spec 02 (BLE)

## 1. Purpose

Both implementations render the same authoritative ESP32 state as identical logical
pixels. This commissioning revision supersedes the obsolete four-panel 64×32
mockup layout, not the scoring rules. The accepted bring-up profiles remain intact.

## 2. Physical canvas and hardware

Two 32×16 RGB P10 panels form a horizontal **64×16** logical canvas, origin at
top-left, x rightwards, y downwards. Panel 1 is controller-nearest, logical x=0..31;
Panel 2 is downstream, x=32..63. The accepted DMA stream sends the far block first:
logical x<32 maps to raw x+32, otherwise x-32. No within-panel mirroring.
Normal 1/8 scan: eight ABC addresses, upper/lower RGB halves; D/E unassigned.
GPIO R1/G1/B1=25/26/27, R2/G2/B2=14/12/13, A/B/C=23/19/5,
LAT/OE/CLK=4/15/16. SHIFTREG/TYPE138, 8 MHz, clock phase true, brightness **12/255**.
Do not reinterpret physical mounting or increase brightness during commissioning.

## 3. Colors

All score/arrow pixels white (#FFFFFF / RGB565 0xFFFF), all other pixels black.
No divider, glow, gray dot grid, animation or extra decoration. Outer columns
x=0/63, seam x=30..33 and top/bottom rows y=0/15 remain black.

## 4. Layout

### 4a. Scores

The existing `font_5x7.json` font is unchanged. Scale each glyph pixel to a **2×2**
block: digits are 10×14, y=1..14, separated by 2 pixels. Left score center x=18.5;
right center x=44.5. Compute startX = integer(centerX - totalWidth/2 + 0.5).
A single digit occupies x=14..23 / 40..49; two digits x=8..29 / 34..55.
Scores 0..99 are fully readable without overlap. Values outside 0..99 display
**two horizontal dashes** in the two digit slots (y=7..8), not a truncated or
wrapped score. The underlying scoring state is NOT capped or changed.

### 4b. Serve and server number

Only the serving side has arrows. Right arrow is the shared 5×5 bitmap:

```text
00100
00010
11111
00010
00100
```

Derive left by reversing each row. Left arrows start at x=1; right at x=58.
Server 1 shows one arrow at y=5..9. Server 2 shows two vertically stacked arrows
at y=2..6 and y=9..13 (2-pixel gap). Same indicator in singles and doubles:
serverNumber retains its Spec 01 meaning. End-game leaves score/arrows unchanged.

## 5. Rendering and ownership

Show boot state 0-0, left serving, two arrows, DOUBLES. Redraw on an actual state
change, not every 10ms. The Arduino loop owns scoring/undo and drawing; BLE
callbacks queue commands and request post-subscription sync. A full clear and
redraw on a change is sufficient. No extra full-frame framebuffer allocation is
required; a short vector of lit pixels is acceptable. Handle DMA init failure
without drawing through an uninitialized driver.

## 6. Shared assets and equivalence

`shared/display_assets/{font_5x7.json,arrows.json,layout.json}` are the design source
of truth. Android loads them at runtime into a pure pixel renderer used by its
Compose canvas and host tests. Firmware may embed static mirrored tables/constants,
checked by `scripts/check_glyphs.py`. Compare entire pixel sets across firmware,
Android and shared-asset fixtures, including one/two digit scores, arrows and
bounds. Android preview preserves **4:1** aspect ratio. Image snapshots are
software evidence, never proof of physical light output.

## 7. Commissioning limitations and non-goals

Owner accepts bring-up, including a suspected minor edge/seam artifact. Track it
as a limitation; investigate only if normal score readability/operation is affected.
No second court, brightness controls, new scoring rules or new radio features.
Spec 01's full side-out to server 2 is deliberately nonstandard and remains an
outstanding owner gameplay decision before real-match acceptance, not a bug to
silently repair. BLE/display/scoring/reconnect hardware E2E needs owner observation.

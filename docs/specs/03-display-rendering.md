# Spec 03: Display rendering

Status: APPROVED — installed panel-block correction; candidate illumination acceptance pending
Owner: Dale
Consumers: ESP32 firmware and Android controller
Depends on: Spec 01 scoring and Spec 02 BLE (unchanged transitions and wire format)

## 1. Purpose

Firmware and Android render identical, upright **16×64** portrait pixels from the
same ESP32 state. This supersedes the old horizontal presentation. A horizontal
firmware image and portrait APK are wire-compatible but do not match visually.

## 2. Physical canvas and hardware

The **unchanged 64×16 DMA framebuffer** drives two 32×16 RGB P10 panels. With
`portrait-r180-002` authenticated and the state at `0,0,L,2,0,D`, the owner
reports upright `2 / ← / 0 / 0` on the powered display. That build wrote scores
to raw x0–31 and server/arrow to raw x32–63. The installed viewer model therefore
places raw x32–63 above raw x0–31, with the same local axes on each panel. Do not
infer visible order from connector labels or change the physical chain.

For portrait `(x, y)` from the viewer's upper-left:

```text
rawX = 63 - y
rawY = x
```

Top-left maps to `(63,0)`; top-right to `(63,15)`; bottom-left to `(0,0)`;
bottom-right to `(0,15)`. Seam rows 31 and 32 map to raw x32 and x31. Relative
to `portrait-r180-002`, the correction toggles raw x bit 5 only (adds or
subtracts 32). This swaps complete blocks without changing the order of rows
inside a block or reversing left/right. Do not apply the historical horizontal
`physicalDisplayX()` half swap to this portrait mapping. No extra framebuffer,
driver rotation, or Android transform is needed. See the [operator guide](../portrait-operator-guide.md)
for the post-install witness; this model does not constitute candidate acceptance.

Keep normal 1/8 scan (eight ABC addresses, upper/lower RGB halves, D/E unassigned),
SHIFTREG/TYPE138, 8 MHz, clock phase true, and single DMA buffer. GPIO
R1/G1/B1=25/26/27, R2/G2/B2=14/12/13, A/B/C=23/19/5, LAT/OE/CLK=4/15/16.
Brightness remains **12/255**.

## 3. Colors

White lit pixels (#FFFFFF / RGB565 0xFFFF), otherwise black. No animation, grid,
divider or decorative pixels. Outer columns x0–1 and x14–15 and rows y0 and y63
are black in both modes. Physical seam rows y31–32 always stay dark. In
Doubles, rows y15–16 separate the scores, rows y31–35 remain black, and rows
y50–52 separate the server and arrow.

## 4. Layout (inclusive coordinates)

| Element | Doubles rows | Singles rows | Source |
| --- | --- | --- | --- |
| Serving score | 1–14 | 3–16 | `leftScore` if left serves, else `rightScore` |
| Opponent score | 17–30 | 33–46 | Score of the other side |
| Server digit | 36–49 | Not rendered | `serverNumber` (Doubles only) |
| Serving direction | 53–62 | 51–60 | `servingSide` |

Singles keeps intact score glyphs on different panels, with three blank rows at
both outside ends. The receiver score starts immediately below the dark seam;
rows 47–50 separate it from the arrow. An equally spaced, vertically centered
composition with these glyph sizes would put the middle glyph through the seam.
The BLE compatibility field does not draw or reserve any space in Singles.

### 4a. Digits and overflow

The unchanged 5×7 glyphs use 2× vertical expansion. A single digit also uses
2× horizontal expansion (x3–12). Two digits through 99 use 1× horizontal
expansion, with two black columns between them (slots x2–6 and x9–13). Values
outside 0..99 show two horizontal dashes in the two slots at y=bandStart+6/+7.
The underlying score is never capped, wrapped or truncated by the renderer.

### 4b. Arrow and serving order

Draw one centered 5×5 `ARROW_RIGHT` (rows `00100, 00010, 11111, 00010, 00100`),
scaled 2× in both directions (x3–12). Mirror horizontally if left serves. The
third digit represents the server in Doubles only; two arrows are never drawn.
The arrow tracks the serving side's physical end, not the even/odd service
court. Switch Courts swaps the physical side scores and flips the serving side,
so only the arrow changes. Full side-out flips the arrow and reorders the scores
(equal scores look unchanged). End Game holds all pixels.

## 5. Rendering and ownership

Boot displays 0, 0, 2, ←. Draw on boot and on actual state changes only. The
Arduino loop continues to own state, undo and rendering. After clearing the DMA
surface once, the renderer streams bounded lit coordinates directly through the
mount transform. No renderer heap allocation or extra frame is needed. Preserve
existing DMA initialization failure and boot validation behavior.

## 6. Shared assets and equivalence

`shared/display_assets/{font_5x7.json,arrows.json,layout.json}` define the glyphs
and layout. Android loads assets and caches the logical pixel set per reported
state; its Compose canvas preserves **1:4** aspect ratio. Before synchronization
or after disconnect, show no live score. C++ embeds static mirrored definitions
verified by `scripts/check_glyphs.py`. Compare all 1,024 coordinates per fixture
against the independent layout oracle and both actual renderers, including both
arrow directions, both servers and overflow. Emulator images are not physical
light-output proof. See the [portrait operator guide](../portrait-operator-guide.md).

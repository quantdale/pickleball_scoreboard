# Spec 03: Display rendering

Status: APPROVED (landscape redesign) — physical illumination acceptance pending
Owner: Dale
Consumers: ESP32 firmware and Android controller
Depends on: Spec 01 scoring and Spec 02 BLE

## 1. Purpose

Firmware and Android render identical **64×16** landscape pixels from the same
ESP32 state:

```text
LEFT SCORE   |   SERVING ARROWS   |   RIGHT SCORE
```

Each team's score stays on its own physical end no matter who serves. The
arrows in the middle point toward the serving team: one arrow for server 1, two
for server 2. The portrait 16×64 presentation is preserved at tag
`portrait-1.3.1-court-undo-003`.

## 2. Physical canvas and hardware

The unchanged 64×16 DMA framebuffer drives two chained 32×16 P10 panels. Raw
x0–31 reach the far panel; raw x32–63 stay in Panel 1, the panel wired directly
to the ESP32. The last verified portrait witness placed Panel 1 on top with raw
x increasing upward and raw y increasing to the viewer's right.

**Supported mount (default):** rotate that verified portrait assembly a quarter
turn clockwise, as seen from the front. Panel 1 is then the right half and the
mapping is the identity:

```text
rawX = x
rawY = y
```

The physical seam is between x31 and x32. Two other mounts are build options,
selected with `PICKLE_LANDSCAPE_MOUNT`, for an assembly installed differently:

| Value | Mount | Mapping |
|---|---|---|
| 1 (default) | Portrait assembly turned clockwise | `rawX = x`, `rawY = y` |
| 2 | Portrait assembly turned counter-clockwise | `rawX = 63 − x`, `rawY = 15 − y` |
| 3 | Original bring-up order (Panel 1 on the left) | `rawX = x < 32 ? x + 32 : x − 32`, `rawY = y` |

Do not infer the mount from connector labels; use the boot witness in §5.

Keep normal 1/8 scan, SHIFTREG/TYPE138, 8 MHz, clock phase true and a single DMA
buffer. GPIO R1/G1/B1=25/26/27, R2/G2/B2=14/12/13, A/B/C=23/19/5,
LAT/OE/CLK=4/15/16. Brightness remains **12/255**.

## 3. Colors

White lit pixels (#FFFFFF / RGB565 0xFFFF), otherwise black. No animation, grid,
divider or decorative pixels. Columns x0 and x63 and rows y0 and y15 stay black.

## 4. Layout (inclusive coordinates)

| Element | Columns | Rows | Source |
|---|---|---|---|
| Left score | 1–22 | 1–14 | `leftScore` |
| Serving arrows | 25–38 | 2–13 | `servingSide`, `serverNumber` |
| Right score | 41–62 | 1–14 | `rightScore` |

### 4a. Digits

The unchanged 5×7 glyphs are drawn at 2× in both directions (10×14 per digit).
A single digit is centered in its 22-column region (left x7–16, right x47–56).
Two digits fill the region with two black columns between them (left x1–10 and
x13–22). Values outside 0..99 cannot occur (Spec 01 §3); the renderer
defensively draws two dashes at rows 7–8 instead of wrapping or truncating.

### 4b. Serving arrows

`ARROW_RIGHT` is a solid 6×12 triangle (rows `100000, 110000, 111000, 111100,
111110, 111111, 111111, 111110, 111100, 111000, 110000, 100000`), drawn 1:1 on
rows 2–13 and mirrored horizontally when the left team serves. Server 1 draws one
arrow centered on the canvas (x29–34). Server 2 draws two identical arrows at
x25–30 and x33–38, so the panel seam falls in the two-column gap between them.
Singles always draws one arrow. End Game holds all pixels.

## 5. Rendering and ownership

Boot displays `0 ◀ 0` (left 0, one left arrow, right 0). This is also the mount
witness: on a correct mount both zeros sit at the outer ends and one arrow points
left in the middle. Upside-down digits mean mount 2; arrows split at the outer
edges with the zeros in the middle mean mount 3.

Draw on boot and on actual state changes only. The Arduino loop owns state,
history and rendering. After clearing the DMA surface once, the renderer streams
lit coordinates through the mount transform, with no heap allocation or extra
frame buffer. Preserve existing DMA initialization failure and boot validation
behavior.

## 6. Shared assets and equivalence

`shared/display_assets/{font_5x7.json,arrows.json,layout.json}` define the glyphs
and layout. Android parses them at runtime; the firmware embeds static copies
verified by `scripts/check_glyphs.py`. Shared fixtures compare all 1,024
coordinates per state between an independent Python oracle, the actual C++
renderer and the Kotlin renderer, covering both sides, both servers, Singles,
one- and two-digit scores, and the defensive overflow. Emulator and host images
are not physical light-output proof.

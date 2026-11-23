# Spec 03: Display rendering

Status: APPROVED (landscape redesign; §2a brightness; §3 operator color) — physical illumination, brightness and color acceptance pending
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
LAT/OE/CLK=4/15/16. The default brightness is **12/255**; §2a defines the
operator levels.

### 2a. Brightness

The operator steps brightness from the controller (Spec 02 §§4–5). The board
applies a level with the driver's `setBrightness8`, which rewrites only the OE
(output-enable) bits of the single existing DMA buffer. Pixels, clock, scan,
colour depth and refresh rate do not change, nothing is allocated, and nothing is
redrawn.

With this configuration (64-clock rows, latch blanking 2, transition bit 0), the
driver lights each row for an even number of clocks around the row centre. Every
value from 1 to 12 gives the minimum two-clock window, so **12/255 is already
the dimmest lit output** and no dimmer level exists. Each level is a whole
multiple of that light (and, roughly, of the LED current for the same picture):

| Level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| `setBrightness8` | 12 | 16 | 24 | 32 | 48 | 64 | 96 | 128 |
| × level 1 | 1 | 2 | 3 | 4 | 6 | 8 | 12 | 16 |

`scripts/check_brightness_levels.py` checks the firmware table against the
driver arithmetic. The ladder stops at 128/255, half the driver range and well
below the near-255 values the driver documents for ghosting.

- **Default and cap.** Level 1 (12/255) until an operator changes it. The build
  constant `PICKLE_BRIGHTNESS_MAX_LEVEL` caps the levels the board accepts and
  reports; it is **4** (×4) by default. That cap is a conservative choice, **not
  an electrical rating**: no panel or supply current has been measured. Raise it
  only after the physical acceptance test below, and never above 8.
- **Firmware enforcement.** The board ignores any level outside 1…cap, so a
  controller cannot exceed it.
- **Independence.** Brightness is not game state (Spec 01 §2). Undo, End Game,
  Reset, reconnects and maintenance leave it unchanged.
- **Persistence.** The level and the color (§3) are stored as one byte each in
  NVS namespace `pksdisp`, apart from the maintenance credentials. A setting is
  written only after neither has changed for 3 s, only if it differs from the
  stored byte, and never during a maintenance session. Boot restores both
  before the first frame. A missing or invalid level restores level 1, a level
  above the cap restores the cap, and a missing or unknown color restores White.
- **Color and current.** P10 drivers sink a fixed current per lit channel, so at
  any level the LED current scales with the lit channels per pixel: White 3,
  Yellow and Cyan 2, Red, Green and Blue 1. White is the worst case, and the one
  cap therefore covers every color. A single channel also looks dimmer than
  White at the same level (Red roughly a quarter to a fifth, Blue less), so an
  operator who picks Red may want a higher level; the cap does not change.

**Physical acceptance (required before relying on any level above 1, and before
raising the cap).** With the owner's normal protected supply, rated meters
already fitted safely and the worst-case lit picture (`88 ◀◀ 88`), record the
5 V branch current, the panel input voltage, and supply and connector
temperatures in **White** (the worst case, §3) at every level up to the cap, settled for 10 minutes at the top
level. Pass if all stay within the supply, wiring and panel ratings with margin,
and the panels show no flicker, ghosting or new colour fault compared with level
1 in daylight and shade. Then check each palette color once at level 1 for a
uniform, correct color on every lit pixel; the deferred individual-pixel
colour defects are recorded, not re-investigated, here. A failed level lowers the cap. Host tests and emulator
images are not light or current measurements.

## 3. Colors

Every lit pixel, digits and serving arrows alike, uses the one operator color;
all other pixels are black. The palette turns whole RGB channels fully on or
off, so each lit channel gets exactly the bit-plane timing White always used:
no grey levels, no extra framebuffer and no change to pixel coordinates.

| Color | Code | RGB565 | Lit channels |
|---|---|---|---|
| White (default) | `W` | `0xFFFF` | R G B |
| Red | `R` | `0xF800` | R |
| Yellow | `Y` | `0xFFE0` | R G |
| Green | `G` | `0x07E0` | G |
| Cyan | `C` | `0x07FF` | G B |
| Blue | `B` | `0x001F` | B |

Magenta and mixed colors such as orange are left out: magenta is a poor score
color, and mixed colors need partial channels, whose low bit planes the minimum
OE window (§2a) cannot render evenly. The operator changes color from the
controller (Spec 02 §§4–5a); the board repaints the current frame once, the
same way it does for a score change, and the color is independent of game state
like brightness (§2a). No animation, grid, divider or decorative pixels.
Columns x0 and x63 and rows y0 and y15 stay black.

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

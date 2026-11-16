# Final two-panel pixel-boundary test

Profile: **`boundary`**. Existing `single` and `dual` profiles are preserved.
Two 32×16 modules, horizontal logical **64×16**, GPIOs unchanged,
brightness **12/255**, 8 MHz clock, `clkphase=true`, normal 1:8 scan.
Panel 1 is controller-nearest and occupies logical x=0..31; Panel 2 is downstream
and occupies x=32..63. The existing chain-block swap is unchanged.

## Current verification and unresolved strip

Owner reports the old dual profile ran physically, but a narrow green strip
appeared at an edge of Panel 2 during Panel 1 green / Panel 2 white.
This is **not treated as harmless or fixed**.

Audited software: Adafruit GFX `fillRect` iterates x < origin+width and vertical
lines end at y+height-1; the two 32×16 calls do not overlap. The chain map is a
64-column permutation preserving local column number; the library's original-ESP32
FIFO adjacent-sample correction is already applied internally. Normal 1:8 scan
routes y=0..7 to RGB1 and y=8..15 to RGB2 at eight ABC addresses. No off-by-one
was found in these paths. Clock/latch timing, signal integrity and physical
module ordering are not proved by that audit; no speculative pixel shift,
clock-phase change or scan-profile change is applied.

The new renderer clears the entire canvas before EVERY static/sweep frame,
uses direct pixel coordinates (not optimized rectangle/line drawing), and never
emits an out-of-bounds pixel. Its pure coordinate calculations and output sink
are host-tested. Software tests do not emulate analog signals or panel ICs.

**Results so far:** single, dual and boundary ESP32 builds PASS. Host MSVC
`/W4 /WX` compilation PASS; **200,512 checks** cover all 91 frames. Tests verify
92 border pixels and 420 interior pixels per panel, exact seam/isolation,
8 distinct corners, checkerboard parity, full sweeps, clearing and unique mapped
pixels. Injecting a deliberate one-column output shift is rejected with
`Panel 2 is not entirely white: neighboring-color strip detected`. This is a
fault-detector control, NOT identification of the actual hardware root cause.

**Boundary upload and serial verification: PASS (2026-11-15).** After the owner
reconnected the isolated USB-only board, CH340 enumerated on **COM11** at USB
location `1-1`. Esptool identified the same ESP32-D0WD-V3 revision 3.1, MAC
`3c:8a:1f:a1:ba:28`. The previous COM5 USB-enumeration blocker is resolved by the
new connection; no driver reinstall or device disabling was performed.

PlatformIO uploaded the **290,720-byte** boundary application through COM11 at
115200 baud; hashes verified for all four written regions. A 140-second serial
capture after one intentional RTS reset verified initialization at **64×16**,
brightness **12/255**, **184 exact-order frame diagnostics**, two complete
91-frame sequences and the next A/B restart. No error, panic, watchdog, brownout,
or additional reset appeared in the capture. Serial port closed afterward.

Evidence:
`D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261115-pi-p10-boundary-com11/`.
See `chip-identification.log`, `upload-boundary.log`, `serial-timestamped.log`,
`serial-raw.log`, and `serial-verification.json`. Image SHA256:
`f9f194a50dd0eb989f914af49c57222f423764c94ae42741ca52c55d628e06ea`.

This proves boundary firmware execution with the panels isolated. The physical
green strip, exact boundaries, and electrical safety are still unverified.

## Exact repeating visible patterns

All statics below last **5 seconds**. Corners/lines are single pixels wide.
The canvas is cleared to black before every frame; no labels overlay A/B borders.

| Order | Pattern | Panel 1 / x=0..31 | Panel 2 / x=32..63 |
|---|---|---|---|
| 1 | A | Red border, blue interior | Yellow border, cyan interior |
| 2 | B | Green border, purple interior | Red border, yellow interior |
| 3 | C corners + edges | Four corner colors and edge lines below | Four corner colors and edge lines below |
| 4 | C x31 only | White rightmost column | Entirely black |
| 5 | C x32 only | Entirely black | White leftmost column |
| 6 | C exact seam | Red rightmost column | Green leftmost column |
| 7 | C isolation 1 | Entirely green | Entirely black |
| 8 | C isolation 2 | Entirely black | Entirely white |
| 9 | Original reproduction | Entirely green | Entirely white, NO green strip expected |
| 10 | D checkerboard | White where (x+y) is even, otherwise black | Same global pixel parity |
| 11 | D inverse checkerboard | Black where (x+y) is even, otherwise white | Same global pixel parity |

A/B border coordinates: local x=0 or 31, or y=0 or 15. Interiors are local
x=1..30, y=1..14 (Panel 2 global interior x=33..62). Purple is RGB565 `0xF81F`
(full red plus blue, magenta/purple); colors are intentionally unambiguous channels.

C corner colors (top-left, top-right, bottom-left, bottom-right):

| Panel | TL | TR | BL | BR |
|---|---|---|---|---|
| 1 | Red (0,0) | Green (31,0) | Blue (0,15) | White (31,15) |
| 2 | Cyan (32,0) | Purple (63,0) | Yellow (32,15) | Orange (63,15) |

For C corners + edges, between y=1..14 the four full-height edge markers are:
x=0 red, x=31 green, x=32 yellow, x=63 blue. Corner colors override their
endpoints; everything else is black.

After the statics:

- **D column sweep:** exactly one white column on black, x=0 through 63,
  **100 ms per column**, moving through Panel 1 then Panel 2.
- **D row sweep:** exactly one white row spanning both panels on black,
  y=0 through 15, **250 ms per row**.
- Repeat A. One complete repeating sequence lasts **65.4 seconds** after startup.

Persistent strips, duplicated rows/columns, mirrored order, displaced borders,
or gaps/overlaps at x31/x32 are failures to report. Capture a video of two
sequences (~2 minutes 15 seconds) and close-up photos of A/B and reproduction.
Note exactly which edge of Panel 2 has the strip and whether it persists throughout
the static hold or only briefly at a transition. Do not adjust live wiring.

## Build and staged upload

From the repository root:

```bash
export PYTHONIOENCODING=utf-8
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e boundary
```

Before uploading, isolate panel power; with USB removed and DC de-energized,
disconnect ESP32 → Panel 1 J1; leave the inter-panel ribbon connected. Place the
board on nonconductive support, secure disconnected contacts, and connect USB
ONLY, with no external positive supply to the controller. Do not hot-plug HUB75.
Then enumerate and identify the actual ESP32 (currently COM11/MAC
`3c:8a:1f:a1:ba:28`; port numbers can change) before running:

```bash
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e boundary -t upload --upload-port COM11
```

Only tool-confirmed upload counts as success. Capture initialization and all 91
`BOUNDARY frame=...` diagnostics through a sequence restart; serial success does
NOT prove the physical pattern. The agent capture script and successful run are
retained in the evidence directory; later serial resets also require safe isolation.

After serial verification, reconnect signals only with BOTH supplies off and
DC discharged. Keep the ESP32 USB-powered, panels on separately protected,
correctly rated regulated **5V** branches with common ground. If using 12V plus a
buck converter, verify buck output/polarity and continuous current/cooling for
both panels; **never connect 12V to the panels or ESP32**. Retain enclosed mains,
strain-relief/earthing and overcurrent-protection prerequisites from README.md.
Stop before panel power-on if any prerequisite is unknown.

# P10 HUB75 bring-up — start with ONE panel

Standalone diagnostic for an original ESP32-WROOM-32, separate from `firmware/`.
No BLE, Android, scoring, or scoreboard rendering is compiled. This implements the
owner's hardware bring-up request, not a change to approved production Spec 03.

**Current boundary investigation:** the owner reports a possible green strip on
Panel 2. The separate `boundary` profile adds exact borders, eight corner markers,
seam/panel isolation, checkerboards and sweeps. See
[BOUNDARY_TEST.md](BOUNDARY_TEST.md) for exact patterns and the remaining visual
test. The boundary firmware is now uploaded on **COM11**, with a successful
140-second serial capture of two complete sequences. This does NOT establish
that the physical green strip is fixed.

## Configuration and audited pins

`src/main.cpp`: physical module 32×16; brightness **12/255**; each color held for
2 seconds; startup black, then repeating **red → green → blue → white → black**.
Serial: **115200 baud**. Brightness reduces average LED duty, NOT fault current.

| HUB75 signal | ESP32 GPIO |
|---|---:|
| R1 / G1 / B1 | 25 / 26 / 27 |
| R2 / G2 / B2 | 14 / 12 / 13 |
| A / B / C | 23 / 19 / 5 |
| D / E | Not connected (`-1` in software) |
| LAT / OE / CLK | 4 / 15 / 16 |
| GND | ESP32 GND, panel ground, PSU DC negative common |

This matches the owner's supplied map in software, **not measured continuity**.
Do not connect the unlabeled position opposite C without identifying it.
The unused second ground jumper is not an extra supply input. Never send panel
load current through a signal jumper or the ESP32 ground path.

Pinned toolchain: Espressif platform **6.13.0**, Arduino ESP32 **2.0.17**
(PlatformIO package `3.20017.241212+sha.dcc1105b`), Xtensa GCC **8.4.0**.
HUB75 DMA **3.0.15**, Adafruit GFX **1.12.6**, transitive BusIO **1.17.4**.
No new globally installed tools. The existing repository venv supplies PIO 6.1.19.

Installed library headers were inspected: `i2s_pins` order is
`r1,g1,b1,r2,g2,b2,a,b,c,d,e,lat,oe,clk`; config constructor takes
`width,height,chain,pins`. `begin()` returns bool. All pins are explicit.
Configuration: `SHIFTREG`, `TYPE138`, 8 MHz, `clkphase=true`, single buffer.
OE is held inactive during setup; DMA starts with a blank buffer; brightness is
set after `begin()` (the library ignores it before initialization).
Allocation/init failure prints an error and does not enter the color loop.

**Limits:** HUB75 has no readback or panel-presence detection. Firmware runs even
with no panel attached. Software does not guarantee blanking during reset or the
bootloader, when GPIOs can float. GPIO5, GPIO12 and GPIO15 are boot-strapping pins;
in particular an external high on GPIO12 at reset can prevent normal flash boot.
Do not add pull-ups to these pins or expose any ESP32 GPIO/3V3 pin to 5 V.

## Scan-pattern uncertainty — do not claim compatibility yet

The owner now reports **1:8-scan** panels, a passed single-panel color test,
and illumination of both chained panels. The installed library's
`src/ESP32-VirtualMatrixPanel-I2S-DMA.h` explicitly identifies 16-row normal
two-row-parallel panels as 1:8 scan. Its DMA code uses height/2 = **8** row
addresses, RGB1 for y=0..7 and RGB2 for y=8..15; `TYPE138` uses A/B/C as binary
address lines. D/E stay -1. Width 32 × chain 2 gives raw **64×16**.
No four-scan pixel remapping is required for this normal 1:8 profile.

This verifies the library's software handling, not physical row/chip identity.
P10 describes pitch, not scan protocol. The new geometry patterns must still
confirm that these particular panels follow the normal mapping.

The installed library's `examples/VirtualMatrixPanel/README.md` and
`VirtualMatrixPanel.ino` document four-scan panels. A confirmed 32×16 four-scan
panel needs DMA geometry **64×8 per module** and the appropriate
`FOUR_SCAN_16PX_HIGH` pixel remapping (or a panel-specific map), not simply a
chain-length change. Those alternatives are not automatically selected here.
Full-screen colors cannot establish pixel order or correct geometry, even if
all LEDs light. Row/column patterns and chip identification are a later gate.
If rows remain black, colors are wrong, or output flickers, report the exact
result; switch power off before inspecting panel model/IC markings or wiring.
Do not increase brightness or change wiring while powered to troubleshoot.

## Build (safe now)

From the repository root in Git Bash:

```bash
export PYTHONIOENCODING=utf-8
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e single
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e dual
```

`single` is the default; `dual` must be selected explicitly. Images are at
`.pio/build/single/firmware.bin` and `.pio/build/dual/firmware.bin` inside this
project; generated files are ignored. `PYTHONIOENCODING` avoids a Windows
cp1252 dependency-listing error; it is only a shell-local setting.

### Optional editor diagnostics

The real firmware compiler is Xtensa GCC. To let clangd resolve the same target
headers without rejecting GCC-only flags, generate a separate editor database
from the repository root:

```bash
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e dual -t compiledb
firmware/.venv/Scripts/python.exe hardware-tests/p10-bringup/scripts/configure_clangd.py
```

The project-local `.clangd` selects ignored `.pio/clangd/compile_commands.json`.
The script queries the installed compiler for its target/system include paths,
then removes three unsupported options **only in this editor database**.
The original PlatformIO database, build flags and firmware image are unchanged.
Regenerate after changing toolchains or dependencies. Fresh C++ LSP checks and
standalone clangd syntax checks passed after this configuration fix.

## STOP: owner confirmation required before upload or opening serial

As observed on 2026-11-13, **COM5 = USB-SERIAL CH340**, VID:PID `1A86:7523`,
USB location `1-3.1`, Windows driver status OK. Other observed ports
COM3/4/7/8/9/10 are Bluetooth links, not upload candidates. After the owner's
isolation confirmation, esptool identified COM5 as **ESP32-D0WD-V3 revision 3.1**,
MAC `3c:8a:1f:a1:ba:28`, with **4 MB flash**. Re-enumerate/identify before future
uploads rather than assuming a COM number cannot change. Opening serial can
reset the board; keep the physical isolation gate for subsequent flashing.

Owner must confirm all of these before the agent proceeds:

1. PSU is switched off and unplugged from AC through its normal plug/switch;
   no exposed mains terminals are being handled.
2. USB was unplugged before the ESP32-to-Panel-1 J1 signal connector was removed.
   The signal connector remains disconnected and secured against shorts.
3. ESP32 rests on a nonconductive surface (no loose metal underneath).
4. ESP32 is now powered **only by USB**, with no PSU positive connection to it.
5. COM5 corresponds to this board (USB-only unplug/replug enumeration can confirm
   if uncertain; never select a device just because it is the first COM port).

After confirmation, the agent will identify the chip and upload **single**:

```bash
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e single -t upload --upload-port COM5
firmware/.venv/Scripts/pio.exe device monitor --port COM5 --baud 115200
```

### Verified flash and serial execution — 2026-11-13

After owner-confirmed isolation, the command above uploaded `single` successfully
to COM5. Esptool reported **Hash of data verified** for the bootloader, partition
table, boot_app0 and 288,800-byte application. No two-panel firmware was uploaded.

A backup read at 460800 baud encountered corrupt serial data without writing
flash. Retrying at **115200** succeeded; this speed is now explicit for uploads.
The backup covers `0x00000..0x5ffff` (393,216 bytes), including all upload erase
regions through `0x56fff`; it is **not** a full 4 MB flash backup.

A timestamped 35-second PySerial monitor at 115200, with one intentional RTS reset,
captured the boot banner, `HUB75 DMA initialized; logical raw size=32 x 16`, and
**three complete color cycles** at approximately two-second intervals. There
were no additional firmware banners/resets or panic/watchdog/error messages in
that window. Monitor closed afterward, releasing COM5. This proves execution
with the panel isolated, NOT physical panel operation or scan compatibility.

Evidence and the backup are outside the repository at:
`D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261113-pi-p10-flash/`.
Key files: `upload-single.log`, `serial-timestamped.log`, `serial-raw.log`,
`serial-verification.json`, and `pretest-flash-0x00000-0x5ffff.bin`.

The upstream audit fetched `origin/main` at `3ae7ad2e`; local `main` remains at
`a08a30c9`. The two intervening commits only change agent docs/adapters, not
firmware, Android, shared data or behavior specs. No merge/reset was needed and
all uncommitted bring-up work was preserved.

## Later: first physical power-on (NOT authorized by a successful build)

Only after upload/serial verification, perform this staged procedure with the
agent. Stop at any unverified safety prerequisite; do not improvise mains work.

1. Unplug USB and isolate PSU AC. Wait for the PSU output to discharge; a person
   competent with a meter must verify DC is de-energized before handling wiring.
   Do not open the PSU or touch AC terminals/capacitors.
2. With everything off, disconnect **both power and signal from Panel 2**. Remove
   Panel 1 J2 → Panel 2 J1. Panel 1 J2 stays empty. Secure unused harness outputs
   so positive contacts cannot short. Only Panel 1 is commissioned initially.
3. Identify Panel 1 **J1 INPUT** using the PCB labels/arrows. Reconnect the signal
   plug only after pin-by-pin continuity matches the table and each connector's
   labeled GND/color/address positions. Do not trust ribbon stripe, notch, or
   an assumed numbered pinout; photos may be mirrored. If orientation is not
   positively identified, stop and provide clear unpowered connector photos.
4. Confirm panel ground/PSU DC negative/ESP32 GND continuity; verify no 5 V-to-GND
   short and correct panel +5V/GND polarity. Controller stays USB-powered only;
   never connect PSU +5V to 3V3, a GPIO, or an unverified controller supply pin.
5. **Safety gate:** panel DC branch protection must be suitable for the panel,
   wire gauge/length, connectors, harness and upstream wiring, located near the
   source. A 40 A PSU's internal protection does not protect thin branch wires.
   Use properly rated panel power leads, not Dupont jumpers for load current.
   Fuse rating cannot be chosen without these ratings; have a competent person
   check them if unknown. Verify regulated 5 V/polarity (owner previously reported
   5.06 V; not independently measured here). Do not use continuity mode on live DC.
6. **Safety gate:** PSU AC terminals must be enclosed, with appropriate protective
   earthing, strain relief and safe installation verified by a competent person.
   If not verified, do NOT energize it. Low LED brightness does not waive this.
7. With connections secured and both supplies off, arrange accessible normal
   power switches. Only when the above checks are confirmed, energize panel DC
   and USB controller power in close succession without touching wiring. Avoid
   leaving one device powered while the other is off with direct signal wiring:
   GPIO backfeeding/undefined inputs are possible. Do not hot-plug HUB75.
8. Observe at least two complete color cycles. White should be deliberately dim;
   black means LEDs off, not loss of controller power. Record all-panel coverage,
   order, flicker/stripes and a photo/video. Cut both supplies at their normal
   switches immediately for heat, smell, unexpected brightness, or unstable output.
   Let both de-energize before any wiring change.

**Current gate (2026-11-15):** the owner reports the single-panel hardware test
PASSED and both chained panels have illuminated. That is owner-observed evidence,
not agent-observed geometry verification or a certification of electrical safety.
The new dual firmware below is now **uploaded and serial-verified** on the
isolated ESP32. Independent colors, label orientation and pixel ordering still
require the owner's visual verification. No brightness or production changes
were made while flashing.

## Two-panel horizontal diagnostic

Select `dual` explicitly; default `single` retains its five-color cycle.
Two 32×16 1:8-scan modules give a **64×16 horizontal diagnostic canvas**.
Brightness remains **12/255**, with unchanged GPIOs and 8 MHz clock.

Panel 1 means the controller-nearest panel (ESP32 → Panel 1 J1);
Panel 2 is downstream (Panel 1 J2 → Panel 2 J1).
For this test, logical Panel 1 occupies x=0..31 and Panel 2 occupies x=32..63.
To display a continuous left-to-right image, place Panel 1 left and Panel 2 right,
with matching upright orientation; do not rearrange any hardware while powered.

### Chain-block ordering

The raw library stream sends the first 32-pixel block through the near panel to
the far panel; the last block remains in the near panel. The small
`include/horizontal_canvas.h` adapter therefore maps logical x=0..31 to DMA
x=32..63, and logical x=32..63 to DMA x=0..31. It does NOT mirror pixels within
a panel, alter GPIOs, or change scan rate. Compile-time checks cover all 64
columns for uniqueness, bounds and unchanged local x coordinate.
Visible labels and sweeps must confirm this expected serial order/orientation;
if reversed or mirrored, report the result rather than silently swapping cables.

### Expected repeating sequence

| Phase | Expected physical output | Duration |
|---|---|---|
| Full-screen fills | Both panels red, green, blue, white, then black | 2 s per color |
| Independent colors A | Panel 1 red; Panel 2 blue | 2 s |
| Independent colors B | Panel 1 green; Panel 2 white | 2 s |
| Coordinate grid | White `1` on Panel 1, `2` on Panel 2; cyan grid at 8-pixel intervals | 4 s |
| Column sweep | One white vertical column on black, x=0..63, across Panel 1 then Panel 2 | 100 ms per column |
| Row sweep | One white horizontal row across both panels on black, y=0..15 | 250 ms per row |

Grid orientation: top edge red, bottom blue, outer left edge green, outer right
edge white. At the join, Panel 1's right edge (x=31) is yellow and Panel 2's left
edge (x=32) is magenta; the top/bottom corners retain red/blue. Labels overwrite
a small part of the interior grid to remain readable. The row sweep should show
exactly ONE lit row, not two separated rows or reordered bands; the column sweep
should cross the panel join without reversing or duplicating columns.
One repeating sequence lasts approximately **28.4 seconds** after startup.

Each phase/coordinate is logged at 115200 baud. Initialization checks raw
width=64 and height=16 and reports the logical-to-raw map. No brightness increase,
BLE, scoring, Android changes, or 32×32 layout is included.

### Verified dual flash and serial execution — 2026-11-15

The owner reconfirmed USB-only isolation: 12V supply/buck converter unpowered,
both panels unpowered, ESP32 → Panel 1 J1 disconnected, inter-panel ribbon left
connected, and ESP32 on a nonconductive surface. The changed power architecture
is NOT yet electrically validated by software; see the additional gate below.

COM5 again identified the same ESP32-D0WD-V3 revision 3.1,
MAC `3c:8a:1f:a1:ba:28`. The following command **succeeded** at 115200 baud;
esptool verified hashes for all four written regions, including the
**290,464-byte** dual application at `0x10000`:

```bash
firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e dual -t upload --upload-port COM5
```

A 65-second capture after one intentional RTS reset verified one boot/banner,
one successful `64 x 16` initialization at brightness `12/255`, and **two complete
sequences** in exact order: five full fills, both independent half-color patterns,
grid, all columns 0..63, all rows 0..15, then restart. Sequence-complete messages
arrived near 31.423 s and 59.828 s. No additional reset, panic, watchdog, brownout
or error message was observed in that capture. COM5 was closed afterward.

The first capture's line-based verifier falsely failed because timed serial
reads split some lines. The capture tool was fixed to buffer/reassemble complete
lines; the repeated capture PASSED without firmware changes or a second upload.
Both attempts were preserved. This was a monitor parsing issue, not a board reset.

Evidence directory:
`D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261115-pi-p10-dual-flash/`.
See `upload-dual.log`, `serial-timestamped.log`, `serial-raw.log` and
`serial-verification.json`. Firmware SHA256:
`433d30e9523f460657707ca9bab808fe3cb06d4a738e7a784374ca1c3feba22f`.

### Additional physical safety gate: 12V supply + buck converter

The owner's latest setup uses a 12V supply and buck converter rather than the
previously described direct 5V supply. **Never apply 12V to these 5V panels or
the ESP32.** Before reconnecting panel power, have a competent person verify:

- The converter's actual output voltage and polarity at the panel connectors
  are regulated **5V**, within the panel manufacturer's tolerance. The earlier
  5.06V measurement does not establish the output of this changed supply path.
  Test/adjust the converter with panels and ESP32 disconnected; isolate/discharge
  power again before connecting loads. Do not adjust it with panels connected.
- Converter continuous current rating, cooling and thermal limits cover BOTH
  panels, including startup/transient demand. Do not rely on a marketing peak
  rating or brightness 12/255 as a hardware current limiter.
- Correctly rated input/output wiring and connectors, source-side overcurrent
  protection and separately protected 5V panel branches; insulated unused leads;
  common ground verified; no panel load current through signal jumpers/ESP32.
- Enclosed, safely installed mains supply/earthing/strain relief wherever AC is
  present. No exposed live terminals or improvised mains wiring.

If voltage, converter rating, branch protection or enclosure is unknown, **stop
before energizing the panels** and provide the component labels/ratings for review.
Software success cannot certify those properties.

Once these prerequisites are confirmed, unplug USB and isolate/discharge the
12V/buck source before reconnecting ESP32 → Panel 1 J1 in its verified orientation.
Keep Panel 1 J2 → Panel 2 J1, and leave Panel 2 J2 empty. Each panel gets its own
protected 5V branch; the ESP32 remains USB-powered only, with no buck-positive
connection to its power pins. With all connections secure, use safe normal power
controls to energize panel 5V and controller USB in close succession, avoiding
prolonged one-sided powering and never hot-plugging HUB75.

Observe two complete diagnostic sequences (about one minute). Report which panel
shows label 1/2, both independent color pairs, orientation of the grid, and whether
each sweep is a single continuous row/column. Cut both power sources for heat,
smell, unexpected brightness or unstable output; let them de-energize before any
wiring change. Physical geometry is still unverified. Production migration remains
deferred: [MIGRATION.md](MIGRATION.md).

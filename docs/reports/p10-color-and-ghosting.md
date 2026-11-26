# Court 1 P10 color and ghosting triage

> **Owner resolution (`court-hardened-006`):** After Mode B inspection and physical panel verification, the owner accepted the faint close-range unintended pixel illumination as a minor visual imperfection and froze display timing, DMA, rendering, and hardware without modification. See [court1-usb-hardening-006.md](court1-usb-hardening-006.md).

For historical background on the physical timing audit, use the [installed timing audit and isolation test](p10-physical-color-isolation.md). At brightness 12 the suggested 2→3 blanking trial below does not change the source OE window on the pinned driver.

## Finding

No software color defect reproduced. The owner describes four photos with intermittent magenta/cyan in white glyphs and faint red off-pixel glow. The actual photos were not attached to this session, so fixed coordinates, naked-eye visibility and shutter dependence are not verified. No board contact, wiring change or installation took place.

- **Logical frame → driver:** The renderer emits positions only. The production adapter clears once and writes RGB565 `0xFFFF` to every lit pixel. The host driver spy starts from solid red, green and blue and checks every cell after repeated Singles repaints: all on values are full white and all off values are black. The two-panel mount transform is a bijection, including seam columns. This test is not a measurement of DMA signals or light output.
- **Driver → HUB75:** Pinned HUB75 DMA 3.0.15 expands `0xFFFF` to full R/G/B and clears both RGB groups for black. The white channels use the same bitplane values and OE window. The XY update preserves the other scan half and control bits. The single active DMA buffer can show an old/blank/new mixture during a repaint; that is a brief update artifact, not an explanation for the same static pixel losing one channel seconds later. Refresh runs between commands without a software redraw. The current profile is 1/8 scan with ABC, SHIFTREG/TYPE138, 8 MHz, clock phase true, default latch blanking (two clocks), and brightness 12/255. None of those values changed.
- **Conditional diagnosis:** If a long exposure and the unaided eye both show the fault at the same physical LED across different glyphs, the likely failing unit is that panel's RGB channel path (package/solder or local driver output). Magenta loses green; cyan loses red. If instead a row, scan half, or the downstream panel changes together, investigate the common data/CLK/LAT/OE path, inter-panel ribbon and signal ground first. Load-sensitive changes point to 5 V stability/return path. Multiplexing and camera exposure can create apparent flicker/banding; they do not on their own prove a missing RGB channel. Red ghosting that follows the previous/adjacent scan row suggests OE/LAT settling or panel leakage; persistent red in a held black pattern would not be caused by stale logical pixels.

## Smallest useful next check (separate authorization)

1. With no scoring changes, compare a held score by eye and in a steady shot using a long exposure (e.g. 1/15 s) versus the original camera settings. Record panel, row/column and whether the tint persists when the score stops changing. This distinguishes photo-only artifacts and repaint transients without altering the board.
2. If visible by eye, use an isolated spare setup with the same panel type and accepted settings. Hold low-duty R/G/B/white/black fields and a sparse/inverse checkerboard several seconds each; the existing [boundary patterns](../../hardware-tests/p10-bringup/BOUNDARY_TEST.md) are a starting point. Same fixed LED fails on one primary → verify/replace only that panel. Shared rows or downstream panel → check/replace the short ribbon and verify local 5 V/ground and signal levels before replacing panels. De-energize both supplies before touching wiring. A bright solid field requires sufficient known power headroom.
3. Only if faint ghosting is reproduced as scan-correlated in a bench A/B, try one latch-blanking increment (2 → 3) on the spare board at the same clock and brightness. [Upstream guidance](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/134) identifies blanking as panel-dependent. Keep the production settings if readability is unaffected or the trial causes dimming/instability. Do not add per-coordinate corrections or change production timing on photo evidence alone.

## Follow-up on `court-undo-003`

**No physical cause has been confirmed.** The archived image hash matches the
[authenticated Court 1 install](court1-undo-install.md); the display source and
pinned driver were audited again. No contact with Court 1, scoring, probing,
wiring change, or upload occurred. The host rerun passed 109 firmware tests
and 72 complete render snapshots. These verify logical output, not the panel.

- **RGB and scan:** The verified mount transform only relocates pixels. The
  pinned driver expands white to equal full-scale R/G/B, applies the same
  brightness compensation to each, and updates the appropriate RGB triplet
  for one half of the 1/8 scan while preserving the other triplet and ABC,
  LAT and OE. The matching ELF uses a single 16-bit store for the RGB update
  (`dma-pixel-disassembly.txt` in the 1.3.1 session). Refresh runs in DMA
  independently of BLE and scoring. A concurrent repaint can mix old, black
  and new pixels across scan time, but would not explain a persistent single
  missing channel at a static LED after repaint.
- **Timing correction to the earlier report:** `HZ_8M` is the *requested*
  setting, not a measured frequency. On original ESP32, library 3.0.15 uses
  the integer divider branch `freq > 8000000 ? 2 : 4`, with BCK divider 2;
  its source describes the selected low branch as **10 MHz**. The linked
  scan schedule is 135 transfers of 64 clocks for each of eight addresses
  (69,120 clocks per frame at the chosen transition bit). The library's
  estimate is about 116 Hz at 8 MHz; the same schedule would be about 145 Hz
  at 10 MHz. These are source-derived, not scope readings. The driver puts
  LAT at the end of the shift, addresses the previous row during the first
  plane, and uses active-low OE centered away from LAT at brightness 12.
  Neither the OE window nor the refresh estimate selects individual colors.
  [Upstream issue #816](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/816)
  reports white-pixel color artifacts responsive to this divider on a different
  panel. That makes marginal CLK-to-data timing plausible on Court 1, not proven.

**Next smallest discriminating check:** For up to 60 seconds, observe an already
held score by eye and take one ordinary and one steady long-exposure photo
(e.g. 1/15 s). Record the panel and exact physical coordinates, and whether the
colors persist without scoring. No controls, connections, probing or wiring
changes are needed. Use the next branch only with separate authorization:

| Observation | Likely area and next check |
|---|---|
| Same isolated LED loses the same primary by eye on a held score | Panel-local RGB path (LED die, solder or local driver output). On a spare, hold sparse R/G/B/white/black patches over the marked coordinates at the accepted brightness. If the fault follows the suspect module on a verified input, replace that panel. Magenta is missing green, cyan red, and blue can indicate loss of both red and green. |
| Several LEDs change together, or the fault is limited to one scan half or the downstream panel | Shared RGB/clock path, ribbon or reference ground first. With both supplies off, substitute one verified short ribbon on a spare chain. If still present, measure CLK/data setup and hold at the panel input plus OE/LAT and ABC at the affected row. Identify the input IC and compare levels with its limits; do not assume 3.3 V meets the panel's threshold. |
| Correlates with panel load | Check regulated 5 V and return at the panel connector under the normal load; compare near/far connectors. A historical unloaded reading does not rule out dips or return-path drop. Have a qualified operator do live measurements. |
| Only in short-exposure photos, or only on the instant of repaint | Compare a longer exposure and the naked-eye held score. Do not replace a panel or change firmware based on a camera-only observation. |

If and only if a spare reproduces the fault with clean power and cabling and the
scope shows marginal clock timing, a one-variable bench comparison of measured
baseline CLK versus the requested 8 MHz can test the driver lead. Compare the
same unit, pattern, buffer, OE blanking, brightness and cables, with a return to
baseline. No production timing change or signed OTA candidate was prepared.

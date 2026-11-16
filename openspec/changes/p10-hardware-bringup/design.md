## Context

See proposal.md for motivation. Production assumes four 32×16 modules and defaults for GPIOs. Installed DMA library 3.0.15 exposes explicit `i2s_pins` and a boolean `begin()`. The owner reports A/B/C only; exact driver chips and scan pattern remain unverified.

## Goals / Non-Goals

**Goals:** Compile an independent esp32dev diagnostic using existing tools; make GPIOs, startup/failure behavior, upload gates, and evidence explicit.

**Non-Goals:** No production edits, alternate library, native-toolchain repair, live PSU control, or promise of pixel geometry from solid fills.

## Decisions

- Use `hardware-tests/p10-bringup/`, not a production build flag: prevents accidental inclusion of BLE/scoring and preserves the production project.
- Pin the already installed Espressif platform 6.13.0, DMA library 3.0.15, and GFX 1.12.6 instead of upgrading globally.
- Use explicit width=32, height=16, chain=1/2; D/E=-1; SHIFTREG/TYPE138 candidate; 8 MHz clock; brightness 12/255. Hold OE inactive during setup, then use the library's blank initial DMA buffer before starting colors. No second controller power input.
- Preserve `single` unchanged; extend `dual` after the owner's reported single-panel pass. For the owner's 1/8-scan modules, use the library's normal two-row-parallel 32×16 profile (8 ABC addresses), chain 2, raw 64×16; no four-scan remap.
- Expose a horizontal diagnostic canvas with Panel 1 (controller-nearest) on logical x=0..31, Panel 2 on x=32..63. The serial stream shifts the first block into the far panel, so `HorizontalCanvas` swaps 32-pixel DMA blocks without mirroring within a module. Label/grid observation must verify this hardware-order expectation; do not infer mounting from chaining.
- Dual phases repeat five full-screen colors (2 seconds each), Panel 1 red/Panel 2 blue (2 seconds), green/white (2 seconds), coordinate grid (4 seconds), single-column sweep 0..63 (100 ms each) and single-row sweep 0..15 (250 ms each). All phases preserve brightness 12/255; geometry is checked visually, not from serial logs.
- Do not open COM5 before isolation: opening can toggle reset lines and execute unknown installed firmware. Enumeration alone establishes a CH340 adapter, not the board/chip identity.

## Risks / Trade-offs

- [Unverified scan/driver] → Describe 1:8 as a candidate, not a detected property; identify panel before pixel mapping. For 1:4, the library documents doubled DMA width/halved height and four-scan remapping.
- [40 A source and thin wires] → Physical branch protection and conductor/connector ratings must be checked; brightness is not current protection.
- [Boot straps and reset-time floating OE] → Flag GPIO5/12/15, especially GPIO12, and lack of guaranteed reset-time blanking. Never fix by connecting ESP32 pins to 5 V.
- [No hardware feedback] → Upload success, serial execution, and visual pass remain separate gates.

## Pixel-boundary extension

Owner reports the earlier dual profile physically ran, but a green strip appeared at an edge of Panel 2 during green/white. No off-by-one found in the audited half-open GFX rectangles, map permutation or normal 1:8 row routing. Physical symptom cannot be reproduced by software alone; do not apply speculative pixel shifts or timing changes.

Add explicit `boundary` environment rather than replacing `single`/`dual`. A hardware-free per-coordinate sampler and renderer clear the full frame then emit only in-range nonblack pixels, using the existing chain-block adapter. Test the exact same pure C++ renderer on the host with MSVC (not the unrelated broken PlatformIO native toolchain). Exhaustive pixel/mapped-frame assertions include borders, seam/isolation, checkerboard population and every sweep coordinate; a deliberately injected one-column output shift must fail the original green/white isolation check as a detector control, not as a claim of reproducing the hardware root cause.

Boundary statics each hold 5 seconds: A, B, C corners/outer edges, x31 only, x32 only, both seam columns, Panel 1 only, Panel 2 only, original green/white, checkerboard and inverse. Then columns sweep at 100 ms and rows at 250 ms. Preserve GPIOs, brightness 12/255, chain length 2, SHIFTREG/TYPE138, 8 MHz and clock phase true. Runtime logs name each phase. Keep physical strip diagnosis open until these frames are observed.

## Migration Plan

Deploy only after the owner's flash safety confirmation, verify serial with panel isolated, then follow README.md's gated single-panel procedure. Do not restore production firmware blindly: its current GPIO/default and geometry assumptions are not commissioned. Full two-unit migration needs a separate approved change; see MIGRATION.md.

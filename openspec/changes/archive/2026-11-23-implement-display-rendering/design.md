## Context

The ESP32 firmware and Android app both had blank/stub rendering after the BLE wiring change:

- Firmware: `display_render.cpp` had `initDisplay()` wired but `renderState()` was a no-op.
- Android: `ScoreboardScreen.kt` had a 64×32 Canvas with a black background but no drawing logic.
- Shared glyph data already existed in `shared/display_assets/font_5x7.json` and `shared/display_assets/arrows.json`.
- `GameState` (firmware) and `ScoreboardState` (Android) already contain all fields needed for rendering.

This change draws the actual pixels on both platforms per Spec 03.

## Goals / Non-Goals

**Goals:**
- Render the 64×32 scoreboard layout on both platforms: score digits, server arrows, center divider.
- Use the shared glyph data as the source of truth.
- Keep the firmware and Android previews pixel-equivalent for all lit content.
- Make the pixel computation testable without hardware.
- Provide a manual checklist for real-hardware visual verification.

**Non-Goals:**
- SINGLES mode rendering differences (no such distinction in Spec 03).
- Match-point/win detection visuals.
- Animations, transitions, flashing, scrolling, or brightness controls (Spec 03 Section 7).
- Hardware-independent JSON parsing on the ESP32 at runtime (see Decision 1).

## Decisions

### 1. Firmware uses hand-transcribed C++ arrays, not runtime JSON parsing (DEVIATION)

The original requirement in Spec 03 Section 6 calls for both platforms to load glyph data from the shared JSON files at runtime, making `shared/display_assets/*.json` the single source of truth. This change deviates from that requirement for the firmware.

**Decision:** The firmware does **not** parse `shared/display_assets/font_5x7.json` or `arrows.json` at runtime. Instead, the glyph bit patterns were hand-transcribed from those JSON files into `static const` C++ arrays in `display_render_logic.cpp`. The JSON files remain the design-time source of truth, but at build time and runtime the firmware uses the C++ arrays.

**This means there are now two copies of the glyph data:** the JSON files and the C++ arrays. They can drift out of sync if one is edited without updating the other.

**Rationale:** Adding a JSON parser such as ArduinoJson to the ESP32 firmware would introduce a new library dependency, increase RAM/flash usage, and require runtime file I/O or a large PROGMEM string for data that is static at boot. Hand-transcribing the small, fixed glyph tables keeps the firmware simple and avoids those costs.

**Trade-off / known risk:** The single-source-of-truth guarantee from Spec 03 Section 6 is lost for the firmware. Any future glyph change must be applied to both `shared/display_assets/*.json` and `firmware/src/display_render_logic.cpp`. The risk is mitigated by:
- Source comments in `display_render_logic.cpp` explicitly stating the arrays are hand-transcribed copies of the JSON files.
- Unit tests that exercise every digit (0–9) and both arrow directions, so an out-of-sync edit is likely to be caught by test failures.
- The delta spec under `openspec/changes/implement-display-rendering/specs/shared-display-assets/spec.md` formally relaxes the shared-asset requirement for the firmware.

### 2. Pixel computation is separated from hardware drawing

A new function `std::vector<RenderPixel> computeRenderedPixels(const GameState&)` in `display_render_logic.cpp` computes the complete set of lit pixels for a state. `display_render.cpp` only clears the panel and calls `dma_display.drawPixel()` for each computed pixel.

**Rationale:** Makes the rendering logic unit-testable on the host without linking the HUB75 library or running on ESP32 hardware. It also isolates the drawing API so future panel driver changes do not affect layout logic.

### 3. Layout coordinates are derived from the mockup structure

Spec 03 describes the layout in relative terms (two halves, arrow row at top ~8%, center divider at 12% inset). Exact pixel coordinates were chosen as follows:

- Canvas: 64×32 (Spec 03 Section 2).
- Left half center: x = 15.5; right half center: x = 47.5.
- Arrow top: y = 3 (≈8% of 32).
- Digit top: y = 15 (centered below the arrow block).
- Center divider: x = 32, dotted from y = 4 to y = 28 in steps of 4.
- Digit glyphs: 5×7 with 1-pixel gap.
- Arrow glyphs: 9×7 with 1-pixel gap; two arrows for server number 2.

**Rationale:** These values satisfy the mockup's structural description and produce a balanced, readable layout. They are the same on both platforms.

### 4. Colors are white-on-black with a sparse divider

Lit pixels are RGB565 `0xFFFF` (white). The background is cleared to black. The center divider is rendered as sparse white pixels rather than a dim gray because the LED panel is full-on/full-off per pixel; the Android preview uses the same sparse pattern for pixel-equivalence.

**Rationale:** Matches Spec 03 Section 3 and the correction that arrows are white, not green. The sparse divider approximates "dim" without requiring PWM duty-cycle complexity.

### 5. Android loads JSON from assets at runtime

`DisplayRenderer.kt` opens `font_5x7.json` and `arrows.json` from Android assets. The Gradle build is configured to treat `shared/display_assets` as an additional assets source directory, so the JSON files are not duplicated inside `android/app/src/main/assets`.

**Rationale:** Keeps the shared JSON files as the single source of truth for the Android side, as originally intended.

### 6. Test strategy focuses on pixel sets, not drawn output

Firmware tests call `computeRenderedPixels()` and assert the presence/absence of specific pixels for known states. Android tests are not added in this change because the rendering is tightly coupled to Compose Canvas `DrawScope`; extracting a pure pixel-computation function would require an additional layer. If future changes need Android unit tests, the same pattern can be applied.

**Rationale:** The firmware is the most valuable place to lock down pixel math because the physical panel is hard to automate. The Android preview can be verified visually against the firmware tests' expected pixel sets.

## Risks / Trade-offs

- **[Risk] Exact coordinate choices are not numerically specified in Spec 03.** → Mitigation: coordinates are consistent across both implementations and match the mockup's described proportions. The manual checklist will catch any visual imbalance once hardware is available.
- **[Risk] Firmware glyph arrays drift from shared JSON.** → Mitigation: explicit source comments and unit tests covering every digit and arrow direction.
- **[Risk] Scores of 100+ overflow the half-width.** → Mitigation: explicitly accepted per Spec 03 Section 9; the renderer simply draws all digits and they may overlap or extend past the divider. No special fallback is built.
- **[Risk] Android asset source-set path is relative and could break if the module moves.** → Mitigation: path is `../../shared/display_assets` from `android/app/build.gradle.kts`; it is simple and unlikely to change.

## Open Questions

None. All Spec 03 layout and color questions were resolved from the mockup before implementation began.

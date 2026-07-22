## 1. Firmware pure-logic pixel computation

- [x] 1.1 Add `firmware/include/display_render_logic.h` with `RenderPixel` struct and `computeRenderedPixels(const GameState&)` declaration.
- [x] 1.2 Add `firmware/src/display_render_logic.cpp` containing:
  - Embedded 5×7 digit glyph arrays mirroring `shared/display_assets/font_5x7.json`.
  - Embedded 9×7 `ARROW_RIGHT` array mirroring `shared/display_assets/arrows.json`.
  - Runtime mirroring helper to derive `ARROW_LEFT` from `ARROW_RIGHT`.
  - Layout helpers for digit/arrow/center-divider placement.
  - `computeRenderedPixels()` returning all lit pixels for a state.
- [x] 1.3 Refactor `firmware/src/display_render.cpp` to call `computeRenderedPixels()` and draw each pixel via `dma_display.drawPixel()`.
- [x] 1.4 Verify `firmware/src/display_render.cpp` compiles for `esp32dev` with `.venv/Scripts/pio.exe run -e esp32dev`.

## 2. Firmware unit tests for pixel computation

- [x] 2.1 Add `firmware/test/test_display_render/test_display_render.cpp` using the existing Unity/native test pattern.
- [x] 2.2 Test 0-0-2 left: both scores show '0', two left arrows, divider present.
- [x] 2.3 Test left serving with server number 1: single left arrow.
- [x] 2.4 Test right serving with server number 1 and non-zero scores: right arrow and both digits rendered.
- [x] 2.5 Test double-digit scores and two right arrows.
- [x] 2.6 Test end-game state still renders scores/arrows/divider.
- [x] 2.7 Test that all computed pixels are within the 64×32 canvas bounds, even for large scores.
- [x] 2.8 Update `firmware/platformio.ini` to include `display_render_logic.cpp` in the native `build_src_filter`.

## 3. Android display renderer

- [x] 3.1 Add `android/app/src/main/java/com/example/pickleballscoreboard/ui/DisplayRenderer.kt` that:
  - Loads `font_5x7.json` and `arrows.json` from Android assets at initialization.
  - Parses glyph rows into bit-packed integers.
  - Provides a `DrawScope.render(state: ScoreboardState)` extension.
  - Draws digits, arrows, and center divider with the same layout as firmware.
  - Derives `ARROW_LEFT` by reversing `ARROW_RIGHT` rows at runtime.
- [x] 3.2 Update `android/app/src/main/java/com/example/pickleballscoreboard/ui/ScoreboardScreen.kt` to instantiate `DisplayRenderer` and call `render()` inside the preview Canvas.
- [x] 3.3 Update `android/app/build.gradle.kts` to add `sourceSets["main"].assets.srcDirs("../../shared/display_assets")`.

## 4. Android build verification

- [ ] 4.1 Run `./gradlew assembleDebug` in `android/` and fix any compile errors.
- [ ] 4.2 Visually inspect the Compose preview for a few representative states (0-0-2, double digits, both server numbers, both sides).

## 5. Manual end-to-end verification plan

- [ ] 5.1 Write `openspec/changes/implement-display-rendering/manual-e2e-checklist.md` and execute it with real hardware once assembled:
  - Flash firmware and confirm the panel boots to 0-0-2 left serving (two left arrows, two zeros, divider).
  - Run app, connect to `"PickleScore"`, and confirm preview matches panel.
  - Score a rally on each side and confirm digits update on both.
  - Trigger side-out to server 1 and confirm a single arrow on the new serving side.
  - Reach double-digit scores (e.g., 10, 25) and confirm both digits render without overlap.
  - Tap Switch Courts and confirm scores and arrows swap symmetrically.
  - Tap End Game and confirm display freezes on the final state.
  - Reset to both possible starting sides and confirm 0-0-2 renders correctly.

## 6. Final validation

- [x] 6.1 Run `pio run -e esp32dev` in `firmware/` successfully.
- [ ] 6.2 Run firmware native tests with `.venv/Scripts/pio.exe test -e native` successfully (blocked on missing `g++` in this Windows environment; run on host).
- [ ] 6.3 Run `./gradlew assembleDebug` in `android/` successfully (run on host with JDK).
- [x] 6.4 Review spec comments in modified files for accuracy and completeness.

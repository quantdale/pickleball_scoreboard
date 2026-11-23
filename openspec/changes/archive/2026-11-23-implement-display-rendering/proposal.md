## Why

The scoring state machine (Spec 01) and BLE wiring (Spec 02) are complete and tested. Both sides still render a blank or stub canvas. This change implements the actual 64×32 pixel rendering defined in Spec 03 so the Android preview matches the physical LED panel.

## What Changes

- **Firmware (`firmware/src/display_render.cpp`, new `firmware/src/display_render_logic.cpp` / `firmware/include/display_render_logic.h`)**
  - Render the 64×32 canvas per Spec 03 Section 2 using the already-wired HUB75 DMA display.
  - Draw score digits with the 5×7 font from Spec 03 Section 4a, arrows with the 9×7 bitmap from Section 4b, and a sparse center divider per Section 4c.
  - Use white lit pixels on a black background per Spec 03 Section 3 (arrows are white, not green).
  - Derive `ARROW_LEFT` at runtime by reversing each row of `ARROW_RIGHT`; no second hand-authored bitmap.
  - Support 1–2 digit scores cleanly; 100+ is explicitly unhandled and will overflow/best-effort render per Spec 03 Section 9.
  - Refactor pixel computation into a separate, hardware-agnostic unit so it can be tested without the HUB75 library or real hardware.
  - **Deviation from the original ideal:** the firmware uses embedded C++ glyph arrays that mirror `shared/display_assets/*.json` rather than parsing JSON at runtime. This avoids adding an embedded JSON parser dependency and keeps the code simple; see the Design section for the rationale and trade-off.

- **Android (`android/app/src/main/java/com/example/pickleballscoreboard/ui/DisplayRenderer.kt`, `ui/ScoreboardScreen.kt`, `app/build.gradle.kts`)**
  - Implement a Compose Canvas renderer that draws the same 64×32 logical grid.
  - Load glyph data at runtime from the shared `shared/display_assets/*.json` files via Android assets.
  - Share the JSON glyph files with firmware by adding `shared/display_assets` as an Android assets source set.
  - Render from the `ScoreboardState` passed into `ScoreboardScreen` (which comes from BLE Notify payloads, not the local state machine).
  - Match the firmware layout, colors, digit-count handling, and arrow mirroring rules.

- **Firmware pure-logic unit tests (`firmware/test/test_display_render/test_display_render.cpp`)**
  - Test `computeRenderedPixels(const GameState&)` against expected pixel sets for representative states.
  - Cover 0-0-2 left, single-server arrows, right-serving states, double-digit scores, end-game rendering, and canvas bounds.

- **Manual end-to-end checklist**
  - Provide a written checklist for visually verifying that the physical panel and the app preview match for a range of states once the LED hardware is assembled.

## Capabilities

### New Capabilities

- `display-rendering`: The 64×32 canvas layout, glyph definitions, colors, and rendering triggers defined in `docs/specs/03-display-rendering.md`. This OpenSpec capability spec captures the same requirements in delta format; the authoritative source of truth remains `docs/specs/03-display-rendering.md`.

### Modified Capabilities

None. This change implements existing approved requirements without changing them.

## Impact

- `firmware/src/display_render.cpp` — now uses the pure-logic function to drive HUB75 drawing.
- `firmware/src/display_render_logic.cpp` (new) — hardware-agnostic pixel computation.
- `firmware/include/display_render_logic.h` (new) — testable interface.
- `firmware/test/test_display_render/test_display_render.cpp` (new) — native unit tests.
- `firmware/platformio.ini` — includes `display_render_logic.cpp` in the native test source filter.
- `android/app/src/main/java/com/example/pickleballscoreboard/ui/DisplayRenderer.kt` (new) — asset-loading Canvas renderer.
- `android/app/src/main/java/com/example/pickleballscoreboard/ui/ScoreboardScreen.kt` — wires the renderer into the preview Canvas.
- `android/app/build.gradle.kts` — adds `shared/display_assets` as an assets source set.

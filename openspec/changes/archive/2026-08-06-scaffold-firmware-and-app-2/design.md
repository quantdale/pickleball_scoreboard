## Context

The approved specs in `docs/specs/` define the complete behavior for the pickleball scoreboard:

- Spec 01 (`01-scoring-state-machine.md`) defines the game state, inputs, and transition rules.
- Spec 02 (`02-ble-protocol.md`) defines the BLE GATT service, command bytes, and state notification format.
- Spec 03 (`03-display-rendering.md`) defines the 64×32 logical canvas, 5×7 font, 9×7 arrow bitmap, and layout.

This change only scaffolds the monorepo so that both targets can be built before any logic is filled in.

## Goals / Non-Goals

**Goals:**

- Provide a buildable PlatformIO firmware project for ESP32-WROOM-32 with the correct file structure and empty stub functions matching Spec 01/02/03 signatures.
- Provide a buildable Android Studio Gradle project using Kotlin + Jetpack Compose with the UI skeleton matching Spec 01 inputs and Spec 03 canvas size.
- Provide shared JSON asset files for the digit font and arrow bitmap so firmware and app can source identical glyph data rather than hard-coding duplicates.

**Non-Goals:**

- Implementing the state machine transition logic (Spec 01 Sections 4–5).
- Implementing BLE command parsing or state notifications (Spec 02 Sections 4–5).
- Implementing actual pixel drawing on the LED panel or app preview (Spec 03 Sections 3–4).
- Adding tests, CI, deployment, or OTA update support.
- Supporting SINGLES mode or match-point/win logic.

## Decisions

1. **Firmware toolchain: PlatformIO with `framework = arduino`.** Rationale: widely used for ESP32 hobby projects, good IDE support, and straightforward BLE libraries (`BLEDevice.h`). The target board is `esp32dev` / ESP32-WROOM-32 as requested.
2. **Android toolchain: Gradle with Kotlin and Jetpack Compose.** Rationale: Compose Canvas makes it easy to render a scaled 64×32 logical grid, and the modern Android BLE APIs are surfaced cleanly in Kotlin.
3. **Shared assets as JSON, not code-generated constants.** Rationale: Spec 03 Section 6 recommends a shared, versioned layout data file. JSON is readable by both C++ (firmware) and Kotlin (app) with minimal parsing, preventing glyph drift between the two renderers.
4. **Arrow left is derived from arrow right at runtime.** Rationale: Spec 03 Section 4b explicitly states `ARROW_LEFT` is the mirrored `ARROW_RIGHT` bitmap; we will author only `arrows.json` containing the right arrow and derive left by reversing each row.
5. **Reset command requires a parameter; stub will accept but not process it yet.** Rationale: Spec 02 Section 4a defines reset as a two-byte command (`0L`/`0R`). The stub BLE service must expose the signature that can receive two bytes, even though the state-machine logic is not implemented in this change.
6. **LED panel driver: `ESP32-HUB75-MatrixPanel-I2S-DMA` by mrfaptastic.** Rationale: this driver was already selected in an earlier project session and is appropriate for driving the 64×32 HUB75 LED panel over I2S DMA. In PlatformIO the package is registered as `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display` and depends on `adafruit/Adafruit GFX Library`. The firmware display stub includes `<ESP32-HUB75-MatrixPanel-I2S-DMA.h>`, initializes a `MatrixPanel_I2S_DMA` instance, and exposes a 64×32 canvas interface, but does not perform any actual drawing in this change.

## Risks / Trade-offs

- [Risk] The approved specs live in `docs/specs/` rather than under `openspec/specs/`, so there is no formal OpenSpec capability spec to link from the proposal. → Mitigation: explicitly reference the three `docs/specs/` files by path and section number in all planning artifacts and code comments.
- [Risk] PlatformIO and Gradle build files can become stale if not exercised. → Mitigation: include a successful build verification as the final task in this change.
- [Risk] The shared JSON files may need schema tweaks once parsing code is written. → Mitigation: keep the JSON structure flat and obvious; document the format in code comments near the parsers in the follow-up change.

## Open Questions / Ambiguities to Resolve

1. **Android package name and SDK levels are not specified.** This change will use conventional defaults (`com.example.pickleballscoreboard`, `minSdk = 26`, a recent stable `targetSdk`) unless the user provides values before implementation begins.
2. **Exact pixel coordinates for layout elements are not fully defined in Spec 03.** Spec 03 Section 4 describes the relative layout (arrows above digits, center divider, 1-pixel glyph gaps) but does not give explicit canvas coordinates for digit/arrow vertical placement or horizontal centering. The stubs will reserve named constants for these values; the follow-up rendering change must pin them down before drawing.
3. **Server-number behavior under normal DOUBLES play.** Spec 01 Section 5 notes that, because every game starts at `serverNumber = 2` and every side-out hands off directly to `serverNumber = 2`, the "second server" visual state may never appear during ordinary play. The scaffolding still includes `serverNumber` and the transient `serverNumber = 1` path (Spec 01 Case B1) so the state machine is complete, but this is worth validating during play-testing.

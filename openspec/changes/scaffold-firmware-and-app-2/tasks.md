## 1. Shared display assets

- [x] 1.1 Create `shared/display_assets/font_5x7.json` containing the digit bitmaps from Spec 03 Section 4a (`0`–`9`, 5×7, `1` = lit).
- [x] 1.2 Create `shared/display_assets/arrows.json` containing the `ARROW_RIGHT` bitmap from Spec 03 Section 4b (9×7), with no second bitmap — left arrow is derived at runtime by mirroring each row.

## 2. Firmware scaffolding

- [x] 2.1 Create `firmware/platformio.ini` targeting `esp32dev` (ESP32-WROOM-32) with Arduino framework and add `mrfaptastic/ESP32 HUB75 LED MATRIX PANEL DMA Display` (plus its `Adafruit GFX Library` dependency) to `lib_deps`.
- [x] 2.2 Create `firmware/src/main.cpp` that includes the three module headers and wires them together with TODO stubs.
- [x] 2.3 Create `firmware/include/game_state.h` and `firmware/src/game_state.cpp` with the empty `GameState` struct and function signatures matching Spec 01 Sections 2–5 (no logic).
- [x] 2.4 Create `firmware/include/ble_service.h` and `firmware/src/ble_service.cpp` with GATT setup matching Spec 02 Section 3 (two characteristics: command write, state notify), advertised name "PickleScore", and no command handling yet.
- [x] 2.5 Create `firmware/include/display_render.h` and `firmware/src/display_render.cpp` with a stub renderer matching Spec 03's 64×32 canvas size. Include the `ESP32-HUB75-MatrixPanel-I2S-DMA` header, declare a `MatrixPanel_I2S_DMA` instance, and expose an `initDisplay()` / `renderState()` stub interface with no drawing implementation.

## 3. Android app scaffolding

- [x] 3.1 Create `android/` as an Android Studio Gradle project with Kotlin + Jetpack Compose enabled.
- [x] 3.2 Create the BLE client stub matching Spec 02: scan for "PickleScore", connect, subscribe to state notify, write command bytes, with no-op handlers.
- [x] 3.3 Create the main Compose screen with the six control buttons from Spec 01 Section 3 (`rally-won-left`, `rally-won-right`, `undo`, `switch-courts`, `end-game`, `reset`) wired to no-op handlers.
- [x] 3.4 Create a Compose preview Canvas sized to Spec 03's 64×32 logical grid (scaled for visibility) with no rendering yet.

## 4. Validation

- [x] 4.1 Verify the firmware project compiles with `pio run` (or equivalent) without errors.
- [ ] 4.2 Verify the Android project builds with Gradle (`./gradlew assembleDebug`) without errors.

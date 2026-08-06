# shared-display-assets Specification

## Purpose
Defines the shared glyph bitmap assets (digit font and server arrow) used by the firmware and the Android app, with the JSON files as the single source of truth.

## Requirements
### Requirement: Digit font asset file
The system SHALL provide a JSON file at `shared/display_assets/font_5x7.json` containing the 5×7 bitmap glyphs for digits `0` through `9` as defined in Spec 03 Section 4a.

#### Scenario: Firmware embeds mirrored font arrays
- **WHEN** the firmware builds
- **THEN** it SHALL compile the ten digit glyphs as static arrays mirroring `shared/display_assets/font_5x7.json`, each glyph a 7-row array of 5 bits with a set bit denoting a lit pixel. The arrays are hand-transcribed copies that SHALL be kept in sync with the JSON; the deviation from Spec 03 Section 6 is documented in the header of `firmware/src/display_render_logic.cpp`.

#### Scenario: App parses font
- **WHEN** the Android app loads `shared/display_assets/font_5x7.json`
- **THEN** it SHALL obtain the same ten digit glyphs, each represented as a 7-row array of 5-character strings where `'1'` denotes a lit pixel and `'0'` denotes an unlit pixel.

### Requirement: Arrow bitmap asset file
The system SHALL provide a JSON file at `shared/display_assets/arrows.json` containing the `ARROW_RIGHT` bitmap from Spec 03 Section 4b; the `ARROW_LEFT` bitmap SHALL be derived at runtime by horizontally mirroring each row.

#### Scenario: Firmware embeds mirrored arrow bitmap
- **WHEN** the firmware builds
- **THEN** it SHALL compile the `ARROW_RIGHT` bitmap as a static array mirroring `shared/display_assets/arrows.json` (hand-transcribed copy kept in sync with the JSON; deviation documented in the header of `firmware/src/display_render_logic.cpp`) and SHALL derive `ARROW_LEFT` by reversing each row.

#### Scenario: App derives left arrow
- **WHEN** the Android app loads `shared/display_assets/arrows.json`
- **THEN** it SHALL obtain the `ARROW_RIGHT` bitmap as a 7-row array of 9-character strings and SHALL derive `ARROW_LEFT` by reversing each row string.

### Requirement: Shared asset source of truth
The digit and arrow JSON files in `shared/display_assets/` SHALL be the single source of truth for glyph bitmaps. The Android app SHALL load these files at runtime. The firmware MAY embed the glyph data as compiled static arrays that mirror the JSON files, provided the arrays are kept in sync with the JSON and the relationship is documented in source comments (see the deviation note in the header of `firmware/src/display_render_logic.cpp`).

#### Scenario: Updating a glyph
- **WHEN** a glyph bitmap in `shared/display_assets/` is changed
- **THEN** the JSON file and the firmware's mirrored static arrays SHALL be updated together, and the Android app SHALL reflect the updated glyph at runtime without source-code edits.

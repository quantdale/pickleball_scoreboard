# shared-display-assets Specification

## Purpose
TBD - created by archiving change scaffold-firmware-and-app. Update Purpose after archive.
## Requirements
### Requirement: Digit font asset file
The system SHALL provide a JSON file at `shared/display_assets/font_5x7.json` containing the 5×7 bitmap glyphs for digits `0` through `9` as defined in Spec 03 Section 4a.

#### Scenario: Firmware parses font
- **WHEN** the firmware loads `shared/display_assets/font_5x7.json`
- **THEN** it SHALL obtain ten digit glyphs, each represented as a 7-row array of 5-character strings where `'1'` denotes a lit pixel and `'0'` denotes an unlit pixel.

#### Scenario: App parses font
- **WHEN** the Android app loads `shared/display_assets/font_5x7.json`
- **THEN** it SHALL obtain the same ten digit glyphs with the same encoding as the firmware.

### Requirement: Arrow bitmap asset file
The system SHALL provide a JSON file at `shared/display_assets/arrows.json` containing the `ARROW_RIGHT` bitmap from Spec 03 Section 4b; the `ARROW_LEFT` bitmap SHALL be derived at runtime by horizontally mirroring each row.

#### Scenario: Firmware derives left arrow
- **WHEN** the firmware loads `shared/display_assets/arrows.json`
- **THEN** it SHALL obtain the `ARROW_RIGHT` bitmap as a 7-row array of 9-character strings and SHALL derive `ARROW_LEFT` by reversing each row string.

#### Scenario: App derives left arrow
- **WHEN** the Android app loads `shared/display_assets/arrows.json`
- **THEN** it SHALL obtain the same `ARROW_RIGHT` bitmap and SHALL derive `ARROW_LEFT` by reversing each row string.

### Requirement: Shared asset source of truth
The digit and arrow JSON files SHALL be the single source of truth for glyph bitmaps used by both the firmware and the Android app; neither target SHALL maintain an independent hard-coded copy of the glyph data.

#### Scenario: Updating a glyph
- **WHEN** a glyph bitmap in `shared/display_assets/` is changed
- **THEN** both the firmware and the app builds SHALL reflect the updated glyph without requiring source-code edits in either target.


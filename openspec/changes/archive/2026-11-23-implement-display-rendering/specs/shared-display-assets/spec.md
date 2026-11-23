# shared-display-assets Specification

## MODIFIED Requirements

### Requirement: Shared asset source of truth

The digit and arrow JSON files in `shared/display_assets/` SHALL remain the source-of-truth definition of the glyph bitmaps. The Android app SHALL load these files at runtime. The firmware MAY embed the glyph data as compiled static arrays that mirror the JSON files, provided the arrays are kept in sync with the JSON and the relationship is documented in source comments.

#### Scenario: Android loads JSON at runtime

- **WHEN** the Android app builds
- **THEN** it SHALL bundle `shared/display_assets/font_5x7.json` and `shared/display_assets/arrows.json` as assets and parse them at runtime.

#### Scenario: Firmware embeds mirrored glyph arrays

- **WHEN** the firmware builds
- **THEN** it MAY compile the glyph bitmaps into static arrays rather than parsing JSON at runtime, as long as a source comment states that the arrays mirror `shared/display_assets/*.json`.

#### Scenario: Glyph change requires both files to be updated

- **GIVEN** a decision to change a glyph bitmap
- **WHEN** the change is made
- **THEN** both `shared/display_assets/*.json` and any firmware embedded arrays SHALL be updated together.

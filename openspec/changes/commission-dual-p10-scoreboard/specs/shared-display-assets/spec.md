## MODIFIED Requirements

### Requirement: Arrow bitmap asset file
The system SHALL provide a JSON file at `shared/display_assets/arrows.json` containing the 5×5 `ARROW_RIGHT` bitmap from Spec 03 Section 4b; the `ARROW_LEFT` bitmap SHALL be derived at runtime by horizontally mirroring each row.

#### Scenario: Firmware embeds mirrored arrow bitmap
- **WHEN** the firmware builds
- **THEN** it SHALL compile the `ARROW_RIGHT` bitmap as a static array mirroring `shared/display_assets/arrows.json` (hand-transcribed copy kept in sync with the JSON; deviation documented in the header of `firmware/src/display_render_logic.cpp`) and SHALL derive `ARROW_LEFT` by reversing each row.

#### Scenario: App derives left arrow
- **WHEN** the Android app loads `shared/display_assets/arrows.json`
- **THEN** it SHALL obtain the `ARROW_RIGHT` bitmap as a 5-row array of 5-character strings and SHALL derive `ARROW_LEFT` by reversing each row string.

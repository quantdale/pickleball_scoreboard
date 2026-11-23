## MODIFIED Requirements

### Requirement: Arrow bitmap asset file
The system SHALL provide a JSON file at `shared/display_assets/arrows.json` containing the 6×12 solid `ARROW_RIGHT` triangle from Spec 03 Section 4b; the left-pointing arrow SHALL be derived by horizontally mirroring each row.

#### Scenario: Firmware embeds mirrored arrow bitmap
- **WHEN** the firmware builds
- **THEN** it SHALL compile `ARROW_RIGHT` as a static 12-row array mirroring `shared/display_assets/arrows.json` (checked by `scripts/check_glyphs.py`) and SHALL derive the left arrow by reversing each row.

#### Scenario: App derives left arrow
- **WHEN** the Android app loads `shared/display_assets/arrows.json`
- **THEN** it SHALL obtain a 12-row array of 6-character strings and SHALL derive the left arrow by reversing each row string.

## ADDED Requirements

### Requirement: Landscape layout asset
`shared/display_assets/layout.json` SHALL define the 64×16 landscape canvas, the two 22-column score regions and the arrow position and gap of Spec 03 Section 4, and the firmware constants SHALL match it.

#### Scenario: Pixel equivalence across implementations
- **WHEN** the shared fixtures are rendered by the independent Python oracle, the firmware C++ renderer and the Android renderer
- **THEN** all 1,024 coordinates of every fixture state are identical, covering both sides, both servers, Singles, one- and two-digit scores and the defensive overflow

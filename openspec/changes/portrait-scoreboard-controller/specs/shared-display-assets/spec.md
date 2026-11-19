# Shared display assets delta

## MODIFIED Requirements

### Requirement: Arrow bitmap asset file

The system SHALL provide the 5×5 `ARROW_RIGHT` bitmap in `shared/display_assets/arrows.json`, reflect it for left serve, and draw one arrow under the server digit regardless of server number.

#### Scenario: Firmware embeds mirrored arrow bitmap

- **WHEN** firmware builds
- **THEN** its static bitmap SHALL mirror the shared asset and left serve SHALL reflect the bitmap horizontally.

#### Scenario: App derives left arrow

- **WHEN** the app renders left serve
- **THEN** it SHALL reflect the shared bitmap horizontally.

## ADDED Requirements

### Requirement: Upright portrait layout on the installed mount

The firmware and application SHALL render identical 16×64 pixels: serving score, opponent score, server digit, arrow. On the actual mount, the original right/downstream panel SHALL be at the top with the seam between rows 31 and 32. The 180° correction relative to `portrait-cw-001` SHALL apply only to physical output.

#### Scenario: Switch courts versus side-out

- **WHEN** courts switch
- **THEN** only arrow direction SHALL change.

- **WHEN** a full side-out transfers service
- **THEN** the two scores SHALL change presentation order and the arrow SHALL reverse.

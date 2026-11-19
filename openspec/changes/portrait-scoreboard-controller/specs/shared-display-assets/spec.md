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

The firmware and application SHALL render identical 16×64 pixels: serving score, opponent score, server digit, arrow. On Court 1's installed mount, viewer rows 0–31 SHALL use raw DMA x63–32 and rows 32–63 SHALL use x31–0. Raw DMA y SHALL equal viewer x. This exchanges the previous r180 build's two blocks only at the physical boundary. Renderers SHALL leave the perimeter, score-gap rows 15–16 and seam rows 31–32 unlit without shrinking the 14-row glyphs.

#### Scenario: Startup with two zeros

- **WHEN** the board and preview display the startup score
- **THEN** the zeros SHALL occupy rows 1–14 and 17–30, above server 2 and one leftward arrow.

#### Scenario: Switch courts versus side-out

- **WHEN** courts switch
- **THEN** only arrow direction SHALL change.

- **WHEN** a full side-out transfers service
- **THEN** the two scores SHALL change presentation order and the arrow SHALL reverse.

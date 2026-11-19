## ADDED Requirements

### Requirement: Mode-aware portrait layout
The display and controller preview SHALL use the authoritative reported mode to select the 16×64 composition defined in Spec 03.

#### Scenario: Singles projection
- **WHEN** the state reports Singles
- **THEN** render serving score, receiving score and one physical serving-end arrow in dedicated centered bands, leaving the perimeter and seam dark and ignoring the server compatibility digit.

#### Scenario: Doubles projection
- **WHEN** the state reports Doubles
- **THEN** preserve the accepted doubles score, opponent, server digit and arrow pixel output.

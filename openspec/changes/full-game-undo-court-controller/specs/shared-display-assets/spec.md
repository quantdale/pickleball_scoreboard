## ADDED Requirements

### Requirement: Continuous Singles portrait layout
The Singles display SHALL position the two scores and single direction arrow as one evenly spaced composition on the 16×64 canvas. An opponent score SHALL be allowed to span the two panels and shall not be clipped at the seam. Doubles rendering remains pixel-identical.

#### Scenario: Opponent glyph crosses the seam
- **WHEN** Singles renders two fours
- **THEN** the opponent glyph spans the center seam, neither display shows the server number, and the arrow remains below both scores

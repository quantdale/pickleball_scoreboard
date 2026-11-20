## ADDED Requirements

### Requirement: Undo capacity is independent of gameplay heap state
The firmware SHALL record normal gameplay in a bounded dedicated journal without asking the general heap to grow it. It SHALL retain every accepted inverse record until undone or rebooted.

#### Scenario: Full-session stress with intermittent reconnects and mode changes
- **WHEN** at least 4,200 mixed state-changing commands exercise both game modes, End Game, Reset, rallies and court switches across simulated reconnects
- **THEN** the board accepts all actions, and repeated Undo restores every prior position including the internal opening status

#### Scenario: Next inverse record cannot fit
- **WHEN** an action would exceed journal capacity
- **THEN** scores, flags and existing history remain unchanged and the extended stream reports the refusal; Undo can regain journal space

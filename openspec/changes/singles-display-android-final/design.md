# Mode-aware portrait display

## Context
The installed DMA mapping and glyphs are already accepted in doubles. The sixth BLE field already selects Singles or Doubles.

## Goals / Non-Goals
Change singles pixels only. Retain state, mode transitions, queue protocol, DMA mapping and signed OTA trust.

## Decisions
Singles uses y3–16, y33–46 and y51–60: paired outer margins of three, intact panels, no tight score/arrow join. Doubles coordinates stay frozen. Firmware streams pixels; Android reads the same layout from JSON. The inverse sampler and baseline snapshots check the two implementations.

## Risks / Trade-offs
The seam makes score-to-score spacing larger than score-to-arrow spacing. Actual hardware appearance and owner phone signing must be verified separately.

## Why

The owner-approved clockwise mounting requires upright, serving-team-first scores on a 16×64 surface and a phone remote that can be operated with one hand.

## What Changes

- **BREAKING (presentation only):** Replace the horizontal score and arrow layout with serving score, opponent score, server digit and one directional arrow.
- Place the live pixels on the left of the Android remote and protected, right-thumb controls on the right.
- Keep scoring transitions, the six-field BLE protocol, DMA configuration and OTA lifecycle intact. Pair portrait firmware and APK for matching pixels.

## Capabilities

### New Capabilities

- `portrait-controller`: Authority-aware portrait gameplay controls and command feedback.

### Modified Capabilities

- `shared-display-assets`: Portrait bands and one mirrored bottom arrow, shared by firmware and preview.

## Impact

Display renderer, preview, controller, shared fixtures, operator documentation and offline build outputs. No Court 1 installation or provisioning changes.

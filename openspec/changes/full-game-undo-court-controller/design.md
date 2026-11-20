# Design

## Context

The ESP32 alone owns scoring. Preserve the six-field legacy stream and the existing portrait pixel mapping.

## Goals / Non-Goals

Full reversal of accepted actions back to boot, including Reset. No persistence across reboot and no deployment to hardware.

## Decisions

- Keep a segmented in-RAM inverse journal: one byte per typical action, plus two scores for Reset. Unlike a ring, it never evicts old actions. If growth cannot safely allocate memory, refuse the new action and keep existing history intact. Pop empty blocks immediately.
- Use a separately subscribable extended state characteristic to keep older Android builds and operator tools working. The new controller prefers it, with legacy fallback on discovery. GATT acknowledgments still do not predict scoring.
- Android 1.3 follows the approved 1.1 composition: inline portrait preview on the left, one action per row on the right, and connection and Board controls above. No court diagram, paired rally row, or preview modal.
- Styling only: [Brilliant probability exercise](https://refero.design/screens/cf1e5091-af40-4c51-8f06-9dee0549c3f8) informs clear framing and color hierarchy; [Duolingo feedback](https://refero.design/screens/0307c5b0-172f-457f-b1bf-dc897ebf13a6) informs rounded, raised buttons and short press feedback. Keep the 1.2 paper, teal, lemon and aqua palette and heavy button typography. Do not copy brand assets, progress bars or reward flow.
- Check the installed APK bytes and version, not just the Gradle result. Board shows the version from the same Gradle metadata that sets the manifest.

- Balance Singles on the full portrait canvas: 8-row outer margins, 5-row gaps, and a doubled scanline spanning rows 31�32 in the receiver score. Preserve the mount mapping and Doubles geometry.

## Risks / Trade-offs

Finite RAM is unavoidable. Keep a free internal heap reserve; reject rather than silently truncate history. No flash wear or changes to OTA admission checks. Verify both GATT versions and deployment profiles without touching hardware.

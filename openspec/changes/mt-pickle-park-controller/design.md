## Context

This follows the owner-approved landscape controller. Screen space for scores and the large score/serve targets is more important than showing every settable value in full.

## Decisions

- Reuse the bottom control row: Switch Court and Reset flank a brightness stepper and a labeled color picker. Use a small palette opened directly from the row rather than permanently crowding it with six targets.
- Keep End Game confirmation and one-shot Resume under Board. Keep existing command callbacks and use only reported settings for feedback and capability visibility (Spec 02 §§5a, 6).
- Use the mountain-and-ball portion of the supplied logo for clarity on small surfaces. Fit it inside the adaptive launcher safe region; use the app name in text where space permits.

## Release verification

Use only a session-owned emulator, the archived 2.2.1 APK and the existing release signer. No physical device or firmware operation is part of the rollout.

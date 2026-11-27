## Why

The owner wants the controller to carry the MT Pickle Park identity and make panel adjustments accessible during a match without disrupting gameplay.

## What Changes

- Brand the Android controller and launcher as MT Pickle Park using the official logo; release as Android 2.3.0.
- Move brightness and LED color access onto the main landscape screen. Keep Switch Court and Reset in place, and move End Game/Resume to the Board menu.

## Capabilities

### New Capabilities

- `landscape-controller-ui`: Defines the Android branding and location of landscape controller actions.

### Modified Capabilities

None. Specs 01–04 still govern scoring, BLE, display settings and maintenance.

## Impact

Android resources, Compose controller, tests and operator documentation only. No changes to package identity, signing, BLE name or protocol, physical display, firmware or OTA.

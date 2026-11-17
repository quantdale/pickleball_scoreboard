## Why

The owner explicitly requests a single authorized agent command that builds, signs and deploys firmware wirelessly while retaining Ethernet internet, without browser or Wi-Fi interaction. The existing signed local OTA service is installed but does not provide automated transport or post-window activation.

## What Changes

- Add a Python deployment CLI using the existing exact HTTP interface, protected external device configuration, bounded Windows WLAN management and structured deployment evidence.
- Add a dedicated replay-resistant HMAC challenge/response BLE maintenance characteristic, preserving scoring commands and requiring explicit volatile-game-loss authorization.
- Expose application boot confirmation and health through local OTA information; support repeated Wi-Fi startup/cleanup without USB.
- Commission existing firmware first, then application-only signed OTA updates; never silently fall back to USB flashing.

## Capabilities

### New Capabilities
- `agent-wireless-deployment`: authorized deployment lifecycle, Windows network restoration, cryptographic activation and honest health/recovery reporting.

### Modified Capabilities

None. Existing scoring UUIDs/payloads/rules and signed package format remain unchanged. Authoritative Spec04 receives the owner-requested maintenance extension.

## Impact

Python tools/tests, Windows WLAN APIs, optional Bleak Windows dependency, firmware BLE/OTA integration and Spec04. Existing owner secrets/recovery files remain outside Git. Physical serial actions require still-valid isolated-USB confirmation; physical LED acceptance remains pending with disconnected panels.

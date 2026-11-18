# Agent-operated firmware deployment

**Court 1: both signed OTA images independently reached `VALID` on opposite slots.** The [second wireless OTA report](reports/ota-second-wireless-update.md#ap-readiness-gated-verification) records the exact running `court1-return-002` hash, `ota_0`, and confirmed health. Waiting for a fresh exact BSSID preceded the successful one-shot association. Maintenance close was accepted and the link dropped; radio shutdown and post-close BLE recovery still need confirmation. Both deployment approvals are spent. No more uploads, installs, rollback or recovery writes are authorized.

Upload alone can erase the inactive application. The hardened CLI requires an exact pre-signed package and a confirmed source. The [earlier endurance run](reports/ota-lifecycle-endurance.md) retained a real HTTP-login failure and unexplained heap loss. Production and enclosure acceptance are open.

## Commands

Use Windows Python from the repository root (Git Bash or Windows Terminal). Dependencies are confined to the existing firmware venv:

```bash
firmware/.venv/Scripts/python.exe -m pip install -r tools/requirements-ota.txt

# Inspect Ethernet route/TLS internet, WLAN adapter/radio, target pins and public signing trust.
firmware/.venv/Scripts/python.exe tools/ota_deploy.py preflight --device court1 --output <new-external-session>/preflight

# Run firmware tests, glyph/pixel snapshots, build with an explicit unique ID, sign and independently verify.
firmware/.venv/Scripts/python.exe tools/ota_deploy.py build --device court1 --build-id court1-release-001 --output <new-external-session>/release

# ONLY after owner approval of the exact signed package, source, destination and score reset:
firmware/.venv/Scripts/python.exe tools/ota_deploy.py deploy --device court1 \
  --package <external-release>/release.pksota --expect-package-sha256 <package-sha256> \
  --expect-source-build <currently-approved-build> --expect-source-slot <ota_0-or-ota_1> \
  --expect-source-sha256 <currently-approved-image-sha256> \
  --authorize-score-reset --output <new-external-session>/deployment
```

`--dry-run` prevents activation/connection/upload; with `--build` it still tests/builds/signs. Live deploy never builds or signs. `status` authenticates on the pinned AP and closes the maintenance session; `verify --package ...` also requires the expected image/version and confirmed health. Both require fresh `--authorize-score-reset` because activating maintenance can interrupt play; 0-0 is never treated as idle. `--activation ble` is the default. `--activation existing` uses an already-open window. `--activation usb --port <verified-port> --confirm-isolated-usb-only` is restricted to commissioning status/verification, never deployment. It requires fresh electrical confirmation, identifies MCU/MAC/security/flash size, resets non-destructively and witnesses serial boot. **The CLI has no USB flashing, migration, provisioning or erase fallback.**

Output directories must be new and outside the repository. Exit0 means the requested operation succeeded, not necessarily deployment: inspect `deploymentSuccess`, `preflightSuccess`, `buildSuccess`, `statusSuccess`, `stage`, `signedPackage`, `transfer`, `installAccepted`, `rebootVerified`, `healthEvidence`, network restoration and failure code. Build/preflight never set deployment success. Unknown install transport outcome is flagged and never automatically retried. An update reporting the wrong version/slot, rollback or missing confirmation is not success.

For WSL, the entrypoint delegates the entire operation to this repository's Windows venv using `wslpath`. Supply paths under Windows-accessible mounts. Windows manages WLAN/BLE; WSL networking is never used as a substitute. This delegation is implemented but not physically exercised in this session.

## Protected device registry

Default: **`%USERPROFILE%\.picklescore\devices.json`**, or `PICKLESCORE_DEVICES`, or `--registry <external-path>`. The registry on this host is installed with owner/SYSTEM-only directory ACL. It references the existing protected owner directory; secrets were not regenerated/copied into Git.

Schema (paths/identities, not secret values):

```json
{
  "devices": {
    "court1": {
      "mac": "3c:8a:1f:a1:ba:28",
      "bleAddress": "3c:8a:1f:a1:ba:2a",
      "ssid": "PickleScore-Court1",
      "bssid": "3c:8a:1f:a1:ba:29",
      "httpDeviceId": "PickleScore-28baa11f8a3c",
      "ownerCredentials": "<protected-owner-directory>/owner-credentials.json",
      "signingPublicKey": "<protected-owner-directory>/signing-public.pem",
      "signingPrivateKey": "<offline-signing-authority>/signing-private.pem",
      "wifiDescription": "<exact Windows WLAN adapter description>"
    }
  }
}
```

The base, AP and Bluetooth MACs are distinct on this ESP32. Challenge verification binds to the **base** MAC. Legacy HTTP device ID uses the Arduino byte-reversed base MAC; pin it explicitly, do not select by device name alone. New devices must use their own verified identities/trust, not these Court1 pins. Registry and owner files must remain outside Git and secure backups are still the owner's responsibility.

A package-only deployment needs the owner maintenance credentials and public verifier, not the private signing key. The `build` operation needs separate signing authority. Existing firmware enforces the provisioned P-256 key; maintenance HMAC is independent of signing authority. Losing credentials/key can require physical recovery; no backdoor is provided.

## Network and protocol

The CLI snapshots the chosen Windows WLAN adapter/profile and per-PHY software radio state. It uses the Native WLAN API, an in-memory manual temporary WPA2/AES profile, bounded scan/connect/DHCP waits and exact SSID/BSSID checks. It sends no password arguments and creates no persistent credential profile. HTTP sockets explicitly bind to the WLAN IPv4 address, with exact `Host: 192.168.4.1`, required Origin, bearer and rotated nonce. No proxies, redirects, cookies, browser or outside firmware downloads are involved.

Ethernet must own the route to1.1.1.1; an explicitly source-bound TLS request with certificate verification must succeed before/during/after switching. No adapter metrics/default routes/DNS are modified. AP access requires an on-link192.168.4.0/24 route on the pinned WLAN adapter. Always attempt previous-profile/radio restoration; failure prevents deployment success. A Windows mutex prevents different devices/registries racing the same adapter; a per-device exclusive lock also prevents concurrent deployments. A process crash can leave the device lock; inspect its PID and evidence before explicitly removing that stale lock, never remove a live deployment's lock.

The actual existing routes are `GET /api/info`, `POST /api/session` with `X-Admin-Key`, `GET /api/status`, `POST /api/upload` raw `.pksota` with single-use `X-CSRF-Token`, `POST /api/install`, `POST /api/close`. Status refresh obtains the current nonce before each authenticated mutation. Upload acknowledgement and verification of staged build and exact image extent are tracked separately. Staging does not select/reboot. After install, the tool reconnects with a bounded deadline and requires opposite slot, expected compiled ID, exact image SHA256 and `bootState=valid`, `confirmed=true`, `healthy=true`. An acknowledgement of `/api/close` is not proof of device-side teardown. A source with `UNDEFINED` or legacy boot metadata cannot be used for a live deployment with this CLI.

## BLE maintenance authorization

Spec04 §8 defines characteristic `...26aa` on the existing scoring service. It transfers only an expiring challenge and HMAC proof, **not firmware**. The challenge includes version, canonical target MAC and32 random bytes; proof binds an explicit volatile-match-loss flag. The HMAC key is the provisioned SHA256 maintenance-secret digest; the signing private key is unrelated. Every attempt consumes the challenge; disconnect/new connection invalidates it; five failures lock activation for60 seconds. A proof is not retried after uncertain transmission. Only pre-proof discovery/service failures get a bounded fresh-connection retry.

Callbacks authorize/queue only. The Arduino loop starts a fresh180-second AP after requester disconnect and no intervening scoring activity/cleanup. The normal600-second authenticated/120-second transfer limits remain. Android/scoring occupancy is refused, not forcibly disconnected. Scoring bytes/UUIDs/CCCD sync/command queue,64×16 rendering, GPIOs and brightness12 remain unchanged. Earlier Wi-Fi shutdowns logged SDK warnings; the second OTA measured one successful full source-side transfer under load. Candidate reactivation, teardown and long-duration acceptance remain pending.

## Safety and recovery

Keep USB access until wireless success/recovery tests pass. Every serial reset/flash requires the electrical setup to remain explicitly confirmed: panels unpowered/discharged, HUB75 disconnected while unpowered, no external5V, ESP32 laptopUSB-only. No assumption from COM-port presence is allowed. No partition/bootloader/provisioning repetition was needed for this feature. Two **explicitly disclosed application-only USB installs** were used during PhaseB, never represented as OTA.

Before the first wireless update, the installed image was `agent-wpa2`/`ota_0`, initial USB metadataUNDEFINED/confirmed=false. Its pre-update whole-device recovery181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f is preserved in two protected copies and physically digest/standard-verified; this is transparently reconstructed, not falsely described as a new full read. See the latest hardware report. Earlier PhaseB agent-v2-a and failed agent-wpa1 records remain historical. Original complete pre-OTA backup remains separate from credential-containing current-layout backups. Full restoration is a separate destructive operation needing explicit approval and fresh isolation; do not run it automatically on a deployment failure. Automatic rollback is compiled/audited but not physically exercised. Panels remain disconnected, so lit-panel acceptance is pending.

## Instruction for future agents

> Build and sign first. Record the exact package digest, confirmed source image, destination slot, recovery and expected downtime. Only after explicit owner approval for that transaction and score reset, run the pinned `deploy` command above. Do not retry an uncertain install or fall back to USB flashing. Preserve the installed baseline unless approval says otherwise. Healthy boot is not proof of physical panels, BLE play, durability or rollback.

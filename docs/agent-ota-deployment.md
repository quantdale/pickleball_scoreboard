# Agent-operated firmware deployment

**One controlled signed wireless deployment passed; production acceptance is open.** Court 1 now runs `agent-life2-budget` on `ota_1` with the exact image hash and `VALID`/confirmed/healthy. The original inactive fallback was overwritten as explicitly authorized; no further deployment, rollback, or recovery write is authorized. See the [first wireless OTA report](reports/ota-first-wireless-update.md). The following is historical pre-update evidence.

**Earlier connectivity checkpoint:** Owner-authorized app-only `agent-wpa2` installation restores heap admission without reducing floors. Full timeout/BLE/scoring and four real Windows WPA2/CCMP/DHCP/admin-HTTP sessions pass with restored networking. Extended endurance completed50 recorded cycles but retained a real HTTP-login transport failure and showed continuing heap loss:120,420→116,364 pre-network bytes, only128 recovered after300s closed. SDK timer/resource attribution and actual firmware transfer/VALID health/load/rollback acceptance remain pending. Signed `agent-ota1` is authentic but not lifecycle-cleared; source-only diagnostic `agent-life1` is prepared, not installed and not a leak fix. **Another update, inactive-slot erase/write, staging or OTA metadata change is not authorized; resume is not permission.** See [latest endurance, exact diagnostic package/command and required approval](reports/ota-lifecycle-endurance.md), [earlier hardware installation results](reports/agent-wpa2-hardware-verification.md) and [historical implementation results](reports/agent-wireless-deployment-verification.md). Do not seal or claim the one-command update workflow works until actual signed updates pass.

## Commands

Use Windows Python from the repository root (Git Bash or Windows Terminal). Dependencies are confined to the existing firmware venv:

```bash
firmware/.venv/Scripts/python.exe -m pip install -r tools/requirements-ota.txt

# Inspect Ethernet route/TLS internet, WLAN adapter/radio, target pins and public signing trust.
firmware/.venv/Scripts/python.exe tools/ota_deploy.py preflight --device court1 --output <new-external-session>/preflight

# Run firmware tests, glyph/pixel snapshots, build with an explicit unique ID, sign and independently verify.
firmware/.venv/Scripts/python.exe tools/ota_deploy.py build --device court1 --build-id court1-release-001 --output <new-external-session>/release

# ONLY after an explicit owner deployment request authorizing volatile score loss:
firmware/.venv/Scripts/python.exe tools/ota_deploy.py deploy --device court1 --build --build-id court1-release-002 --authorize-score-reset --output <new-external-session>/deployment

# Reuse a separately signed package; never pass a raw .bin or full-flash image.
firmware/.venv/Scripts/python.exe tools/ota_deploy.py deploy --device court1 --package <external-release>/release.pksota --authorize-score-reset --output <new-external-session>/deployment
```

`--dry-run` prevents activation/connection/upload; with `--build` it still tests/builds/signs. `status` activates maintenance and reads pinned info; `verify --package ...` requires the expected image/version and confirmed health. These operations also require `--authorize-score-reset` because maintenance can interrupt play;0-0 is never treated as idle. `--activation ble` is the default. `--activation existing` uses an already-open window. `--activation usb --port COM11 --confirm-isolated-usb-only` is commissioning-only and requires fresh electrical confirmation. It identifies MCU/MAC/security/flash size, resets non-destructively and witnesses serial boot. **The CLI has no USB flashing, migration, provisioning or erase fallback.**

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

The actual existing routes are `GET /api/info`, `POST /api/session` with `X-Admin-Key`, `GET /api/status`, `POST /api/upload` raw `.pksota` with single-use `X-CSRF-Token`, `POST /api/install`, `POST /api/close`. Status refresh obtains the current nonce before every mutation. Staging does not select/reboot. After install, the tool reconnects with a bounded deadline and requires opposite slot, expected compiled ID, exact image SHA256 and `bootState=valid`, `confirmed=true`, `healthy=true`. Legacy images need isolated serial confirmation; a version string alone is insufficient. Current initial USB-installed app has UNDEFINED metadata, not a pending trial, so no false confirmation is inferred.

## BLE maintenance authorization

Spec04 §8 defines characteristic `...26aa` on the existing scoring service. It transfers only an expiring challenge and HMAC proof, **not firmware**. The challenge includes version, canonical target MAC and32 random bytes; proof binds an explicit volatile-match-loss flag. The HMAC key is the provisioned SHA256 maintenance-secret digest; the signing private key is unrelated. Every attempt consumes the challenge; disconnect/new connection invalidates it; five failures lock activation for60 seconds. A proof is not retried after uncertain transmission. Only pre-proof discovery/service failures get a bounded fresh-connection retry.

Callbacks authorize/queue only. The Arduino loop starts a fresh180-second AP after requester disconnect and no intervening scoring activity/cleanup. The normal600-second authenticated/120-second transfer limits remain. Android/scoring occupancy is refused, not forcibly disconnected. Scoring bytes/UUIDs/CCCD sync/command queue,64×16 rendering, GPIOs and brightness12 remain unchanged. Wi-Fi shutdown still logs SDK timeouts; repeated reactivation/scoring worked with the corrected cleanup stack, but transfer-load and long-duration acceptance remain pending.

## Safety and recovery

Keep USB access until wireless success/recovery tests pass. Every serial reset/flash requires the electrical setup to remain explicitly confirmed: panels unpowered/discharged, HUB75 disconnected while unpowered, no external5V, ESP32 laptopUSB-only. No assumption from COM-port presence is allowed. No partition/bootloader/provisioning repetition was needed for this feature. Two **explicitly disclosed application-only USB installs** were used during PhaseB, never represented as OTA.

Before the first wireless update, the installed image was `agent-wpa2`/`ota_0`, initial USB metadataUNDEFINED/confirmed=false. Its pre-update whole-device recovery181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f is preserved in two protected copies and physically digest/standard-verified; this is transparently reconstructed, not falsely described as a new full read. See the latest hardware report. Earlier PhaseB agent-v2-a and failed agent-wpa1 records remain historical. Original complete pre-OTA backup remains separate from credential-containing current-layout backups. Full restoration is a separate destructive operation needing explicit approval and fresh isolation; do not run it automatically on a deployment failure. Automatic rollback is compiled/audited but not physically exercised. Panels remain disconnected, so lit-panel acceptance is pending.

## Instruction for future agents

> On `feat/agent-wireless-deployment`, make the requested firmware change, run regressions and use `tools/ota_deploy.py deploy --device court1 --build --build-id <new-unique-ID> --authorize-score-reset --output <new-external-session>` only after I explicitly authorize deployment and score loss. Verify opposite slot, exact running image and confirmed health, restore networking and report evidence. WPA connectivity works; the later50-cycle campaign instead found persistent heap loss and retained one HTTP-login transport failure. Keep the SDK timer/resource-attribution and transfer-load gates open. Neither authentic agent-ota1 nor diagnostic agent-life1 is lifecycle-cleared. The latter requires explicit permission for a first controlled Wi-Fi diagnostic installation, inactive-slot erase/write and normal OTA metadata/trial/confirmation; boot VALID alone does not establish lifecycle stability. Never use USB flashing as an OTA fallback or deploy either prepared package merely on resume.

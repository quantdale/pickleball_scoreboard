# Agent-operated firmware deployment

**Court 1: `court-undo-003` on `ota_0`, `VALID`/confirmed/healthy.** The [signed wireless install](reports/court1-undo-install.md) verified full flash readback, the exact running digest, post-boot memory, maintenance closure, BLE initial sync and host restoration. `court-controller-002` on `ota_1` is the expected fallback. The one-write authorization is consumed; do not rerun.

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
  --confirm-installed-ready --authorize-write-and-fallback --authorize-score-reset --verify-ble-recovery \
  --output <new-external-session>/deployment
```

`--dry-run` prevents activation/connection/upload; with `--build` it still tests/builds/signs. Live deploy never builds or signs. `status` authenticates on the pinned AP and closes the maintenance session; `verify --package ...` also requires the expected image/version and confirmed health. Both require fresh `--authorize-score-reset` (interruption/volatile-state consent) and `--confirm-installed-ready` for wireless use; 0-0 is never treated as idle. Neither sends upload/install or resets the board when activated over BLE. `deploy` additionally requires `--authorize-write-and-fallback` for the exact pinned transaction. `--activation ble` is the default. `--activation existing` uses an already-open window. `--activation usb --port <verified-port> --confirm-isolated-usb-only` is restricted to commissioning status/verification, never deployment. It requires fresh electrical confirmation, identifies MCU/MAC/security/flash size, resets non-destructively and witnesses serial boot. **The CLI has no USB flashing, migration, provisioning or erase fallback.**

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

The CLI snapshots the chosen Windows WLAN adapter/profile and per-PHY software radio state. It waits up to 30 seconds for a fresh exact SSID/BSSID, then makes one native association using an in-memory WPA2/CCMP profile and the exact BSSID. The whole WLAN readiness budget is 60 seconds (within the 180-second unauthenticated AP window). It prefers a DHCP `/24` lease. Windows IP Helper reads addresses and the selected route/source directly, without starting PowerShell in the poll loop. A lease wait of up to 15 seconds after association leaves time for fallback. APIPA never counts as a lease. If DHCP has not delivered a usable address, the CLI can use a temporary IPv4 address on the pinned Wi-Fi adapter. It checks local overlap, skips known neighbors, waits for Windows duplicate-address detection, and requires the selected on-link `/24` route, source-bound TCP port 80 and the expected ARP MAC. These checks do not substitute for admin HTTP authentication, and the CLI rechecks the link, route/source and peer before HTTP. `wifiAttempts` records the address, route and failure stages; `networkPath` identifies `dhcp` or `temporary-static`. It sends no password arguments and creates no persistent credential profile. HTTP binds to the validated WLAN address, with exact `Host: 192.168.4.1`, required Origin, bearer and rotated nonce. No proxies, redirects, cookies, browser or outside firmware downloads are involved.

Ethernet must own the route to1.1.1.1; an explicitly source-bound TLS request with certificate verification must succeed before/during/after switching. No adapter metrics/default routes/DNS or persistent IPv4 settings are modified. The static path adds only a temporary unicast address and its implicit on-link routes. On an ordinary unelevated run, the CLI continues to poll for DHCP but reports `static_permission_required` if no lease arrives. To make fallback available, start the same pinned command in a Windows Terminal opened with **Run as administrator** and approve the ordinary UAC prompt before BLE activation. Do not change group membership, disable UAC/firewall or change network policy. Temporary addresses have a five-minute OS lifetime as a backstop for forced process termination; normal cleanup deletes and checks the address/routes immediately, checks DHCP/DAD policy, restores the saved profile/radios and verifies Ethernet. AP access requires an on-link192.168.4.0/24 route on the pinned WLAN adapter. Always attempt previous-profile/radio restoration; failure prevents deployment success. A Windows mutex prevents different devices/registries racing the same adapter; a per-device exclusive lock also prevents concurrent deployments. A process crash can leave the device lock; inspect its PID and evidence before explicitly removing that stale lock, never remove a live deployment's lock.

The actual existing routes are `GET /api/info`, `POST /api/session` with `X-Admin-Key`, `GET /api/status`, `POST /api/upload` raw `.pksota` with single-use `X-CSRF-Token`, `POST /api/install`, `POST /api/close`. Status refresh obtains the current nonce before each authenticated mutation. Upload acknowledgement and verification of staged build and exact image extent are tracked separately. Staging does not select/reboot. After install, the tool reconnects with a bounded deadline and requires opposite slot, expected compiled ID, exact image SHA256 and `bootState=valid`, `confirmed=true`, `healthy=true`. An acknowledgement of `/api/close` is not proof of device-side teardown. A source with `UNDEFINED` or legacy boot metadata cannot be used for a live deployment with this CLI.

## BLE maintenance authorization

Spec04 §8 defines characteristic `...26aa` on the existing scoring service. It transfers only an expiring challenge and HMAC proof, **not firmware**. The challenge includes version, canonical target MAC and32 random bytes; proof binds an explicit volatile-match-loss flag. The HMAC key is the provisioned SHA256 maintenance-secret digest; the signing private key is unrelated. Every attempt consumes the challenge; disconnect/new connection invalidates it; five failures lock activation for60 seconds. A proof is not retried after uncertain transmission. Each BLE lookup first scans actively for the exact pinned address. After a clean miss the CLI makes one eight-second passive lookup; it never connects using a cached address alone or the display name. Existing pre-proof GATT service/read recovery is bounded; a scan error or absence is not evidence of occupancy or hardware failure.

Callbacks authorize/queue only. The Arduino loop starts a fresh180-second AP after requester disconnect and no intervening scoring activity/cleanup. The normal600-second authenticated/120-second transfer limits remain. Android/scoring occupancy is refused, not forcibly disconnected. Scoring bytes/UUIDs/CCCD sync/command queue, 64×16 DMA configuration, GPIOs and brightness 12 remain unchanged. Earlier Wi-Fi shutdowns logged SDK warnings; the second OTA measured one successful full source-side transfer under load. Candidate reactivation, teardown and long-duration acceptance remain pending.

## Mode A — installed wireless (no physical service)

At courtside, leave both P10 panels and the ESP32 connected and powered by the normal approved installed supplies, common ground and unchanged wiring. Do not open the enclosure or attach USB. Confirm idle court/no match, Android voluntarily disconnected via **Board → Disconnect**, stable power, nearby BLE/Wi-Fi and a recovery plan with protected signed archives and safe service access. Do not use this mode to commission an unknown power system. Stop for wiring/power/temperature concerns or marginal radio coverage; do not move live connections.

1. Retain the owner's current-session operating attestations and exact transaction authorization; ask again only if contradicted or materially changed. An authorized deploy independently checks the approved source within its authenticated session; do not open a redundant verification window. For a separately authorized read-only check, use `verify` with the signed running package and source slot. If the live source is not exactly `VALID`/confirmed/healthy, stop before writing.
2. With explicit authorization for the exact device, source, package, inactive-slot overwrite, reboot and loss of scores/Undo, run one pinned `deploy`. The CLI gates upload on authenticated idle phase and memory evidence; firmware enforces unchanged heap floors again before erase. New status telemetry requires no serial console. The old association is released after install acknowledgement (or uncertain response); bounded radio reconnects are for observation only, never repeat a write.
3. Require exact candidate build/hash/opposite slot and `VALID`/confirmed/healthy in a fresh authenticated session. `--verify-ble-recovery` requires maintenance close acknowledged, Windows observing the pinned link down, restoration, and a BLE initial-state notification followed by normal disconnect, without any scoring write. Preserve evidence at pre-upload, staged readback and candidate status where supported. Close acknowledgement is not independently verified teardown. Check real panels and RF in the final location; the software cannot attest stable power or temperatures.

### Recovering a pre-write failure

Association and TCP/80 readiness do not prove an HTTP response. The CLI records request, response-header or response-body transport failures with their exception class and errno, without logging headers or bodies. It makes at most one additional unauthenticated `GET /api/info` after a transport failure on the same validated link, rechecking the link guard each time. HTTP rejection, invalid JSON, identity mismatch, and every POST are not retried by this mechanism. If BLE is absent on the first recovery scan, do not infer a brick or activate maintenance again: check for the exact BLE address separately and prove initial subscription and disconnect, then authenticate the source with a distinct no-write verification before seeking fresh write approval.

Use `result.json` to distinguish missing/APIPA DHCP, elevation denial, route conflict, lost/changed AP, TCP failure and HTTP rejection. Fix the evidenced problem, not the guards. If a bounded new maintenance attempt was authorized and upload definitely was not attempted, verify WLAN restoration and Ethernet. Use authenticated close if available; otherwise wait for the device's full unauthenticated window (180 seconds, plus startup/cleanup margin) and confirm pinned BLE initial sync before reactivating. If HTTP login was sent but its outcome is unknown, the authenticated window may be active: do not assume the shorter expiry. Use the appropriate full deadline or independently verify closure. Never retry if any upload might have started. Once an install might have been sent, only observe authenticated running state; do not replay it, use USB or force rollback.

### Court 1: one verification-only Windows networking test

**Historical command for the pre-install `portrait-r180-002` source.** It passed before the [panel-order OTA](reports/court1-portrait-order-install.md). Do not rerun it against the current image. Any new verify requires separate owner approval, fresh Mode A readiness confirmation and current package/slot pins. Both panels stay on their normal installed power. Windows Git Bash example:

```bash
firmware/.venv/Scripts/python.exe tools/ota_deploy.py verify --device court1 \
  --package ../_AgentArtifacts/pickleball_scoreboard/sessions/20261119-pi-portrait-r180/candidate/portrait-r180-002.pksota \
  --expect-package-sha256 549b48945e7e453534881dd2bff04a19b0f7250118280df5335397a0289b224b \
  --expect-source-slot ota_0 --activation ble --confirm-installed-ready \
  --authorize-score-reset --verify-ble-recovery --output <new-external-session>/verify
```

This pins the **running** image, not `portrait-order-004`. One BLE maintenance attempt and at most one Wi-Fi join; no upload, install, reset, or scoring writes. Require `statusSuccess`, the source's exact authenticated digest and VALID health, close acknowledgment and Windows observing the link down, WLAN restoration, Ethernet TLS, and a pinned BLE connection that receives the initial state and disconnects. Link loss is not an independent RF shutdown measurement. If failure is before authentication, report that close was unavailable and BLE recovery was not checked; do not reactivate or force a reboot. Stop after the test either way. See [the failure report](reports/court1-windows-dhcp-route.md).

## Mode B — isolated USB, recovery and electrical service

Do not seal until installed OTA and safe recovery access are confirmed. Every serial operation requires fresh explicit isolation: panels unpowered/discharged, HUB75 disconnected while unpowered, no external positive 5 V, and ESP32 powered only by the verified USB path. Verify the MCU/flash identity and separately authorize each destructive action. Physical inspection, rewiring, or supply changes also require safe power-down/discharge and separate authorization. No assumption from COM-port presence is allowed. No partition/bootloader/provisioning repetition was needed for this feature. Two **explicitly disclosed application-only USB installs** were used during PhaseB, never represented as OTA.

Before the first wireless update, the installed image was `agent-wpa2`/`ota_0`, initial USB metadataUNDEFINED/confirmed=false. Its pre-update whole-device recovery181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f is preserved in two protected copies and physically digest/standard-verified; this is transparently reconstructed, not falsely described as a new full read. See the latest hardware report. Earlier PhaseB agent-v2-a and failed agent-wpa1 records remain historical. Original complete pre-OTA backup remains separate from credential-containing current-layout backups. Full restoration is a separate destructive operation needing explicit approval and fresh isolation; do not run it automatically on a deployment failure. Automatic rollback is compiled/audited but not physically exercised. Those were isolated runs, so they do not prove installed electrical or RF acceptance. Missing trust, ineligible/corrupt slots or bootloader, sustained RF loss, power/electrical faults, or inability to confirm a running wireless-capable build can require separately authorized physical service. If the exact previous VALID image returns, record rollback; no automatic retries, writes or USB fallback.

## Instruction for future agents

> Build and sign first. Record the exact package digest, confirmed source image, destination slot, recovery and expected downtime. Only after explicit owner approval for that transaction and score reset, run the pinned `deploy` command above. Do not retry an uncertain install or fall back to USB flashing. Preserve the installed baseline unless approval says otherwise. Healthy boot is not proof of physical panels, BLE play, durability or rollback.

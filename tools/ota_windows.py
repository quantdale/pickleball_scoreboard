"""Windows WLAN adapter for Spec04 §8. Secrets stay in native API memory.

No explicit route/metric changes, saved-profile overwrites, password arguments or XML files.
"""

import ctypes as C
import http.client
import json
import socket
import ssl
import subprocess
import time
import uuid
from datetime import datetime, timezone
from ipaddress import IPv4Address, IPv4Network
from xml.sax.saxutils import escape

from ota_winip import IpHelper

D = C.c_uint32


class Guid(C.Structure):
    _fields_ = [("data", C.c_ubyte * 16)]


class Interface(C.Structure):
    _fields_ = [("guid", Guid), ("description", C.c_wchar * 256), ("state", D)]


class Radio(C.Structure):
    _fields_ = [("index", D), ("software", D), ("hardware", D)]


class Radios(C.Structure):
    _fields_ = [("count", D), ("radios", Radio * 64)]


class Ssid(C.Structure):
    _fields_ = [("length", D), ("value", C.c_ubyte * 32)]


class Association(C.Structure):
    _fields_ = [
        ("ssid", Ssid),
        ("bssType", D),
        ("bssid", C.c_ubyte * 6),
        ("phy", D),
        ("phyIndex", D),
        ("signal", D),
        ("rx", D),
        ("tx", D),
    ]


class Security(C.Structure):
    _fields_ = [("enabled", D), ("oneX", D), ("auth", D), ("cipher", D)]


class Connection(C.Structure):
    _fields_ = [
        ("state", D),
        ("mode", D),
        ("profile", C.c_wchar * 256),
        ("association", Association),
        ("security", Security),
    ]


class BssEntry(C.Structure):
    _fields_ = [("ssid", Ssid), ("phyId", D), ("bssid", C.c_ubyte * 6), ("bssType", D), ("phyType", D),
                ("rssi", C.c_int32), ("quality", D), ("inReg", C.c_ubyte), ("beacon", C.c_uint16),
                ("timestamp", C.c_uint64), ("hostTimestamp", C.c_uint64), ("capabilities", C.c_uint16),
                ("frequency", D), ("ratesLength", D), ("rates", C.c_uint16 * 126),
                ("ieOffset", D), ("ieSize", D)]


class BssidList(C.Structure):
    _fields_ = [("type", C.c_ubyte), ("revision", C.c_ubyte), ("size", C.c_uint16),
                ("entries", D), ("total", D), ("mac", C.c_ubyte * 6)]


class ConnectParameters(C.Structure):
    _fields_ = [
        ("mode", D),
        ("profile", C.c_wchar_p),
        ("ssid", C.c_void_p),
        ("bssidList", C.c_void_p),
        ("bssType", D),
        ("flags", D),
    ]


AP_ADDRESS = "192.168.4.1"
AP_NETWORK = IPv4Network("192.168.4.0/24")
ON_LINK = str(IPv4Address(0))
# Spec 04 §8: bounded DHCP preference and a small high-address candidate pool.
DHCP_WAIT = 15
STATIC_ADDRESSES = ("192.168.4.210", "192.168.4.211", "192.168.4.212")
# Spec 04 §8: rescan until the AP appears. Scans at 0/8/17/24 s found the AP 0.5 s after
# the 8 or 17 s scan and once missed it, so discovery measured the schedule, not the AP.
BSS_SCAN_INTERVAL = 4
BSS_WAIT = 40


class WifiError(RuntimeError):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def powershell(script, *, timeout=25):
    result = subprocess.run(
        [
            "powershell.exe",
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            "$ErrorActionPreference='Stop'; " + script,
        ],
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )
    if result.returncode:
        raise RuntimeError("Windows network inspection failed (PowerShell)")
    try:
        return json.loads(result.stdout)
    except (ValueError, TypeError) as error:
        raise RuntimeError("Invalid Windows network inspection response") from error


def route_query(remote):
    # Spec 04 §8: inspect Windows' actual selection, not an interface-constrained lookup.
    if remote not in ("1.1.1.1", AP_ADDRESS):
        raise ValueError("Unsupported route target")
    return (
        f"Find-NetRoute -RemoteIPAddress '{remote}' -ErrorAction SilentlyContinue | ForEach-Object {{ "
        "$row=$_; switch ($row.CimClass.CimClassName) { "
        "'MSFT_NetIPAddress' { @{kind='address'; index=$row.InterfaceIndex; address=$row.IPAddress} } "
        "'MSFT_NetRoute' { @{kind='route'; index=$row.InterfaceIndex; "
        "nextHop=$row.NextHop; prefix=$row.DestinationPrefix} } } }"
    )


def selected_route(rows):
    addresses = [row for row in rows if row.get("kind") == "address"]
    routes = [row for row in rows if row.get("kind") == "route"]
    if len(addresses) != 1 or len(routes) != 1:
        return None
    addr, selected = addresses[0], routes[0]
    if (type(addr.get("index")) is not int or type(selected.get("index")) is not int
            or not all(isinstance(field, str) and field for field in
                       (addr.get("address"), selected.get("prefix"), selected.get("nextHop")))):
        return None
    return {"index": addr["index"], "routeIndex": selected["index"], "address": addr["address"],
            "prefix": selected["prefix"], "nextHop": selected["nextHop"]}


def route(remote):
    result = selected_route(powershell(f"ConvertTo-Json -InputObject @({route_query(remote)}) -Compress"))
    if result is None:
        raise RuntimeError("Windows did not select a single IPv4 source and route")
    return result


def ipv4_policy(index, timeout):
    # Once before adding a static address and during cleanup.
    return powershell(
        f"$i={int(index)}; $cfg=Get-NetIPInterface -InterfaceIndex $i -AddressFamily IPv4; "
        "$dns=Get-DnsClientServerAddress -InterfaceIndex $i -AddressFamily IPv4; "
        "@{dhcp=[int]$cfg.Dhcp; dad=$cfg.DadTransmits; dns=@($dns.ServerAddresses)} | ConvertTo-Json -Compress",
        timeout=timeout,
    )


def preferred_address(row, origin):
    address = IPv4Address(row["address"])
    return (
        address in AP_NETWORK
        and address not in (AP_NETWORK.network_address, AP_NETWORK.broadcast_address, IPv4Address(AP_ADDRESS))
        and row["prefixLength"] == AP_NETWORK.prefixlen
        and row["origin"] == origin
        and row["state"] == "Preferred"
        and row["skipAsSource"] is False
    )


def local_readiness(snapshot, index, *, static_address=None):
    addresses = snapshot["addresses"]
    eligible = {row["address"] for row in addresses
                if preferred_address(row, "Manual" if static_address else "Dhcp")
                and (not static_address or row["address"] == static_address)}
    if not eligible:
        if static_address:
            return ("static_duplicate" if any(row["address"] == static_address and row["state"] == "Duplicate"
                                              for row in addresses) else "static_not_ready"), None
        if any(IPv4Address(row["address"]).is_link_local for row in addresses):
            return "dhcp_apipa", None
        return "dhcp_not_ready", None
    route = selected_route(snapshot["selection"])
    if route and (route["index"] != index or route["routeIndex"] != index):
        return "route_wrong_interface", None
    if (not route or route["prefix"] != str(AP_NETWORK) or route["nextHop"] != ON_LINK
            or not any(row["index"] == index and row["prefix"] == str(AP_NETWORK)
                       and row["nextHop"] == ON_LINK for row in snapshot["routes"])):
        return "route_not_on_link", None
    if route["address"] not in eligible:
        return "route_wrong_source", None
    if any(row["index"] != index and row["prefix"] != "0.0.0.0/0"
           and IPv4Address(AP_ADDRESS) in IPv4Network(row["prefix"]) for row in snapshot["routes"]):
        return "route_conflict", None
    return "route_ready", route["address"]


def ethernet_probe():
    r = route("1.1.1.1")
    adapters = powershell(
        "Get-NetAdapter -Physical | Select-Object ifIndex,MediaType,Status | ConvertTo-Json -Compress"
    )
    if isinstance(adapters, dict):
        adapters = [adapters]
    if r["routeIndex"] != r["index"] or not any(
        a["ifIndex"] == r["index"] and a["MediaType"] == "802.3" and a["Status"] == "Up"
        for a in adapters
    ):
        raise RuntimeError(
            "Ethernet is not the active internet route; refusing network change"
        )
    # TLS certificate verified, numeric public endpoint, socket explicitly source-bound.
    connection = http.client.HTTPSConnection(
        "1.1.1.1",
        timeout=8,
        source_address=(r["address"], 0),
        context=ssl.create_default_context(),
    )
    try:
        connection.request("HEAD", "/")
        response = connection.getresponse()
        if response.status >= 500:
            raise RuntimeError("Ethernet internet probe failed")
        response.read(4096)
    finally:
        connection.close()
    return {
        "interfaceIndex": r["index"],
        "sourceAddress": r["address"],
        "tlsInternetVerified": True,
    }


class WindowsWifi:
    """Reversible one-adapter WLAN session. Call restore even if connect raises."""

    def __init__(self, description=None):
        self.dll = C.WinDLL("wlanapi.dll")
        H, P, W = C.c_void_p, C.c_void_p, C.c_wchar_p
        signatures = {
            "WlanOpenHandle": [D, P, C.POINTER(D), C.POINTER(H)],
            "WlanCloseHandle": [H, P],
            "WlanEnumInterfaces": [H, P, C.POINTER(P)],
            "WlanFreeMemory": [P],
            "WlanQueryInterface": [
                H,
                C.POINTER(Guid),
                D,
                P,
                C.POINTER(D),
                C.POINTER(P),
                C.POINTER(D),
            ],
            "WlanSetInterface": [H, C.POINTER(Guid), D, D, P, P],
            "WlanSetProfile": [H, C.POINTER(Guid), D, W, W, D, P, C.POINTER(D)],
            "WlanConnect": [H, C.POINTER(Guid), C.POINTER(ConnectParameters), P],
            "WlanDisconnect": [H, C.POINTER(Guid), P],
            "WlanScan": [H, C.POINTER(Guid), P, P, P],
            "WlanGetNetworkBssList": [H, C.POINTER(Guid), P, D, C.c_int32, P, C.POINTER(P)],
            "WlanDeleteProfile": [H, C.POINTER(Guid), W, P],
        }
        for name, args in signatures.items():
            fn = getattr(self.dll, name)
            fn.argtypes = args
            fn.restype = D
        self.handle = H()
        version = D()
        self.check(
            self.dll.WlanOpenHandle(2, None, C.byref(version), C.byref(self.handle)),
            "open WLAN",
        )
        self.guid = Guid()
        self.temporary = None
        self.profile_xml = None
        self.static_address = None
        self.static_policy = None
        self.saved = None
        self.saved_radios = []
        self.attempts = []
        pointer = P()
        try:
            self.check(
                self.dll.WlanEnumInterfaces(self.handle, None, C.byref(pointer)),
                "enumerate WLAN",
            )
            assert pointer.value is not None
            count = C.cast(pointer, C.POINTER(D))[0]
            items = (Interface * count).from_address(pointer.value + 8)
            selected = [
                item
                for item in items
                if not description or description == item.description
            ]
            if len(selected) != 1:
                raise RuntimeError(
                    "Wi-Fi adapter missing/ambiguous; configure wifiDescription"
                )
            self.guid = Guid.from_buffer_copy(selected[0].guid)
            self.description = selected[0].description
        except BaseException:
            self.dll.WlanCloseHandle(self.handle, None)
            raise
        finally:
            if pointer.value:
                self.dll.WlanFreeMemory(pointer)
        try:
            self.saved = self.connection()
            radios = self.inspect_interface(4, Radios)
            self.saved_radios = [
                (r.index, r.software, r.hardware) for r in radios.radios[: radios.count]
            ]
            self.ip = IpHelper()
            self.luid, self.index = self.ip.adapter(self.guid)
        except BaseException:
            self.dll.WlanCloseHandle(self.handle, None)
            raise

    @staticmethod
    def check(code, action):
        if code:
            raise RuntimeError(
                f"Windows WLAN {action} failed ({code}); check adapter/radio/location permissions"
            )

    def inspect_interface(self, opcode, kind):
        size, value_type, pointer = D(), D(), C.c_void_p()
        self.check(
            self.dll.WlanQueryInterface(
                self.handle,
                C.byref(self.guid),
                opcode,
                None,
                C.byref(size),
                C.byref(pointer),
                C.byref(value_type),
            ),
            "query",
        )
        try:
            if size.value < C.sizeof(kind):
                raise RuntimeError("Unexpected WLAN response size")
            return kind.from_buffer_copy(C.string_at(pointer, C.sizeof(kind)))
        finally:
            self.dll.WlanFreeMemory(pointer)

    def connection(self):
        try:
            value = self.inspect_interface(7, Connection)
        except RuntimeError as error:
            if "(5023)" in str(error):
                return None  # ERROR_INVALID_STATE: disconnected
            raise
        if (
            value.state != 1
        ):  # wlan_interface_state_connected; ignore transient associations
            return None
        return {
            "profile": value.profile,
            "ssid": bytes(
                value.association.ssid.value[: value.association.ssid.length]
            ).decode("utf-8"),
            "bssid": ":".join(f"{b:02x}" for b in value.association.bssid),
            "auth": value.security.auth,
            "cipher": value.security.cipher,
        }

    def set_radio(self, index, state):
        value = Radio(index, state, 0)
        self.check(
            self.dll.WlanSetInterface(
                self.handle,
                C.byref(self.guid),
                4,
                C.sizeof(value),
                C.byref(value),
                None,
            ),
            "set radio",
        )

    def connect_profile(self, profile, *, bssid=None):
        transient = profile == self.temporary and self.profile_xml is not None
        desired = None
        if bssid is not None:
            # Spec 04 §8: pin at native association as well as before HTTP.
            mac = (C.c_ubyte * 6).from_buffer_copy(bytes.fromhex(bssid.replace(":", "")))
            desired = BssidList(0x80, 1, C.sizeof(BssidList), 1, 1, mac)
        params = ConnectParameters(
            1 if transient else 0,
            self.profile_xml if transient else profile,
            None,
            C.cast(C.byref(desired), C.c_void_p) if desired is not None else None,
            1,
            0,
        )
        self.check(
            self.dll.WlanConnect(
                self.handle, C.byref(self.guid), C.byref(params), None
            ),
            "connect",
        )

    def fresh_bss(self, ssid, bssid, since):
        pointer = C.c_void_p()
        self.check(self.dll.WlanGetNetworkBssList(self.handle, C.byref(self.guid), None, 3, False,
                                                None, C.byref(pointer)), "read BSS cache")
        try:
            if not pointer.value:
                raise WifiError("wifi_inspection_failed", "Windows returned a null BSS cache")
            size, count = C.cast(pointer, C.POINTER(D))[:2]
            if 8 + count * C.sizeof(BssEntry) > size:
                raise WifiError("wifi_inspection_failed", "Unexpected Windows BSS cache layout")
            now = 116444736000000000 + time.time_ns() // 100
            for row in (BssEntry * count).from_address(pointer.value + 8):
                if (bytes(row.ssid.value[:row.ssid.length]) == ssid.encode()
                        and bytes(row.bssid).hex(":") == bssid and row.bssType == 1
                        and row.capabilities & 16 and since <= row.hostTimestamp <= now
                        and now - row.hostTimestamp <= 20 * 10_000_000):
                    return {"ssid": ssid, "bssid": bssid, "rssiDbm": row.rssi}
            return None
        finally:
            self.dll.WlanFreeMemory(pointer)

    def wait_for_bss(self, attempt, since, began, deadline):
        # Refresh discovery while the AP starts after BLE activation; still one association.
        scans = attempt["scans"] = []
        while (remaining := deadline - time.monotonic()) > 0:
            elapsed = time.monotonic() - began
            row = self.fresh_bss(attempt["expectedSsid"], attempt["expectedBssid"], since)
            if row is not None:
                attempt["bss"] = {**row, "seconds": round(elapsed, 3)}
                return
            if not scans or elapsed - scans[-1]["seconds"] >= BSS_SCAN_INTERVAL:
                code = self.dll.WlanScan(self.handle, C.byref(self.guid), None, None, None)
                scans.append({"seconds": round(elapsed, 3), "returnCode": code})
            time.sleep(min(.5, remaining))
        raise WifiError("wifi_bss_not_seen", "No fresh exact SSID/BSSID seen before the association deadline")

    def connect(self, ssid, password, expected_bssid, timeout=70):
        # Spec 04 §8: reserve up to 40 s for AP discovery, then one association with DHCP,
        # route and TCP checks in the remaining 30 s of the budget (AP window 180 s).
        began = time.monotonic()
        # WLAN host timestamps are UTC FILETIME (100 ns since 1601).
        since = 116444736000000000 + time.time_ns() // 100
        deadline = began + timeout
        expected_bssid = expected_bssid.lower() if expected_bssid else None
        attempt = {"interfaceIndex": self.index, "expectedSsid": ssid, "expectedBssid": expected_bssid,
                   "startedUtc": datetime.now(timezone.utc).isoformat(), "checks": [], "tcp80Reachable": None}
        self.attempts.append(attempt)
        if not expected_bssid:
            raise WifiError("wifi_identity", "An exact BSSID pin is required")
        if self.temporary is None:
            for index, software, hardware in self.saved_radios:
                if hardware != 1:
                    raise RuntimeError("Wi-Fi hardware radio is off")
                if software != 1:
                    self.set_radio(index, 1)
            self.temporary = "PickleScore-Agent-" + uuid.uuid4().hex
            # Native temporary-profile mode: secret XML stays in memory, never saved.
            self.profile_xml = (
                f'<?xml version="1.0"?><WLANProfile xmlns="http://www.microsoft.com/networking/WLAN/profile/v1">'
                f"<name>{self.temporary}</name><SSIDConfig><SSID><hex>{ssid.encode().hex()}</hex>"
                f"<name>{escape(ssid)}</name></SSID></SSIDConfig><connectionType>ESS</connectionType>"
                "<connectionMode>manual</connectionMode><MSM><security><authEncryption>"
                "<authentication>WPA2PSK</authentication><encryption>AES</encryption><useOneX>false</useOneX>"
                "</authEncryption><sharedKey><keyType>passPhrase</keyType><protected>false</protected>"
                f"<keyMaterial>{escape(password)}</keyMaterial></sharedKey></security></MSM></WLANProfile>"
            )
        self.wait_for_bss(attempt, since, began, min(began + BSS_WAIT, deadline))
        if time.monotonic() >= deadline:
            raise WifiError("wifi_association_timeout", "No WLAN association time remains")
        attempt["associationRequestedSeconds"] = round(time.monotonic() - began, 3)
        self.connect_profile(self.temporary, bssid=expected_bssid)
        return self.wait_ready(attempt, began, deadline)

    def wait_ready(self, attempt, began, deadline):
        stage = "wifi_association_timeout"
        dhcp_deadline = None
        candidates = iter(STATIC_ADDRESSES)
        static_unavailable = False
        while time.monotonic() < deadline:
            now = time.monotonic()
            check = {"seconds": round(now - began, 3)}
            attempt["checks"].append(check)
            try:
                current = self.connection()
                if current is not None:
                    check["observed"] = {key: current[key] for key in ("ssid", "bssid", "auth", "cipher")}
                if current is None or current == self.saved:
                    # The old saved link may still be visible while WlanConnect is pending.
                    stage = "wifi_link_lost" if "associatedSeconds" in attempt else "wifi_association_timeout"
                    if stage == "wifi_link_lost":
                        raise WifiError(stage, "Pinned WLAN association dropped during readiness checks")
                else:
                    if (current["ssid"] != attempt["expectedSsid"] or current["bssid"] != attempt["expectedBssid"].lower()
                            or current["profile"] != self.temporary):
                        raise WifiError("wifi_identity", "Unexpected SSID, BSSID or WLAN profile; no HTTP sent")
                    if (current["auth"], current["cipher"]) != (7, 4):
                        raise WifiError("wifi_security", "Pinned WLAN link is not WPA2-PSK/CCMP")
                    attempt.setdefault("associatedSeconds", check["seconds"])
                    if dhcp_deadline is None:
                        dhcp_deadline = min(now + DHCP_WAIT, deadline - 12)
                    stage, source = self.check_ipv4(check)
                    if (not static_unavailable and self.static_address is None and now >= dhcp_deadline
                            and deadline - now >= 10 and stage in ("dhcp_apipa", "dhcp_not_ready")):
                        attempt["dhcpResult"] = stage
                        static_unavailable = not self.start_static(candidates, current, deadline, attempt)
                    if stage == "static_duplicate":
                        self.remove_static()
                        self.start_static(candidates, current, deadline, attempt)
                    if source is not None:
                        stage = self.check_tcp(check, source, current, deadline)
                        if "tcp80Reachable" in check:
                            attempt["tcp80Reachable"] = check["tcp80Reachable"]
                        if stage == "ready":
                            self.address = source
                            self.current = current
                            attempt["networkPath"] = "temporary-static" if self.static_address else "dhcp"
                            return {"interfaceIndex": self.index, "sourceAddress": source,
                                    "networkPath": attempt["networkPath"], **check["observed"]}
            except WifiError as error:
                stage = error.code
                raise
            finally:
                check.update(result=stage, elapsedSeconds=round(time.monotonic() - now, 3))
            time.sleep(min(.5, max(0, deadline - time.monotonic())))
        if static_unavailable and stage in ("dhcp_apipa", "dhcp_not_ready"):
            stage = "static_permission_required"
        message = ("No DHCP lease; temporary IPv4 requires a normally elevated Windows terminal"
                   if stage == "static_permission_required" else stage)
        raise WifiError(stage, f"WLAN readiness ended after {time.monotonic() - began:.1f}s: {message}")

    def snapshot(self):
        return {"addresses": [row for row in self.ip.addresses() if row["luid"] == self.luid],
                "routes": [row for row in self.ip.routes() if IPv4Address(AP_ADDRESS) in IPv4Network(row["prefix"])],
                "selection": self.ip.selection(AP_ADDRESS)}

    def check_ipv4(self, check):
        started = time.monotonic()
        try:
            check["ipv4"] = self.snapshot()
            result, source = local_readiness(check["ipv4"], self.index, static_address=self.static_address)
            check["ipv4Result"] = result
            return result, source
        except (RuntimeError, OSError, ValueError, TypeError, KeyError) as error:
            check["inspectionError"] = type(error).__name__
            raise WifiError("wifi_inspection_failed", "Windows IPv4 inspection failed") from error
        finally:
            check["ipv4ElapsedSeconds"] = round(time.monotonic() - started, 3)

    def start_static(self, candidates, current, deadline, attempt):
        static = attempt.setdefault("staticAttempts", [])
        addresses, routes = self.ip.addresses(), self.ip.routes()
        if (any(IPv4Address(row["address"]) in AP_NETWORK for row in addresses)
                or any((subnet := IPv4Network(row["prefix"])).prefixlen and subnet.overlaps(AP_NETWORK)
                       for row in routes)):
            raise WifiError("static_route_conflict", "Existing local IPv4 or route overlaps the maintenance subnet")
        if not C.windll.shell32.IsUserAnAdmin():
            static.append({"result": "permission-required"})
            return False  # DHCP may still succeed before the overall deadline.
        if self.static_policy is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise WifiError("static_deadline", "No budget remains to inspect static IPv4 policy")
            self.static_policy = ipv4_policy(self.index, min(remaining, 6))
            if not self.static_policy["dad"]:
                raise WifiError("static_dad_disabled", "Windows duplicate address detection is disabled")
        for address in candidates:
            if time.monotonic() >= deadline:
                raise WifiError("static_deadline", "No budget remains to assign a temporary address")
            if self.connection() != current:
                raise WifiError("wifi_identity", "Pinned AP changed before temporary IPv4 assignment")
            if self.ip.neighbor(address, self.luid, self.index):
                static.append({"address": address, "result": "cached-neighbor"})
                continue
            # Record the exact cleanup key before the native call, even if interrupted
            # between kernel success and Python resuming. Never delete a reported duplicate.
            self.static_address = address
            code = self.ip.add(address, self.luid, self.index, AP_NETWORK.prefixlen)
            static.append({"address": address, "returnCode": code})
            if code:
                self.static_address = None
            if code == 5010:
                continue
            if code == 5:
                return False
            if code:
                raise WifiError("static_assignment_failed", f"Windows temporary IPv4 assignment failed ({code})")
            return True
        raise WifiError("static_address_conflict", "No safe temporary IPv4 address remains")

    def remove_static(self):
        address = self.static_address
        if address is None:
            return
        # Check policy while still on the AP (unconfigured DNS on disconnect can differ).
        try:
            # DNS learned via DHCP may legitimately change when the radio disconnects.
            policy = ipv4_policy(self.index, 6)
            initial = self.static_policy
            if initial is None or any(policy[key] != initial[key] for key in ("dhcp", "dad")):
                raise WifiError("static_policy_changed", "Windows DHCP or DAD policy changed")
        finally:
            self.ip.delete(address, self.luid, self.index)
            if (any(row["luid"] == self.luid and row["address"] == address for row in self.ip.addresses())
                    or any(row["luid"] == self.luid and row["prefix"] in (str(AP_NETWORK), f"{address}/32")
                           for row in self.ip.routes())):
                raise WifiError("static_cleanup_failed", "Temporary IPv4 address or on-link route remains")
            self.static_address = None
            self.static_policy = None

    def check_tcp(self, check, source, current, deadline):
        started = time.monotonic()
        remaining = deadline - started
        if remaining <= 0:
            return "tcp80_not_checked"
        try:
            with socket.create_connection((AP_ADDRESS, 80), timeout=min(1, remaining), source_address=(source, 0)):
                pass
            check["tcp80Reachable"] = True
            self.guard_http(source, current)
            return "ready" if time.monotonic() < deadline else "wifi_readiness_deadline"
        except OSError:
            check["tcp80Reachable"] = False
            return "tcp80_unreachable"
        finally:
            check["tcp80ElapsedSeconds"] = round(time.monotonic() - started, 3)

    def guard_http(self, source, current):
        if self.connection() != current:
            raise WifiError("wifi_identity", "WLAN association changed before HTTP")
        stage, now = local_readiness(self.snapshot(), self.index, static_address=self.static_address)
        if now != source:
            raise WifiError(stage if now is None else "route_wrong_source", "Pinned IPv4 source/route changed before HTTP")
        peer = self.ip.neighbor(AP_ADDRESS, self.luid, self.index)
        if peer is None:
            # A just-used entry can be absent or incomplete in the cache; ask the link once.
            peer = self.ip.resolve(AP_ADDRESS, self.luid, self.index)
        if peer != current["bssid"]:
            # Name the observed peer: a stale entry from another 192.168.4.1 SoftAP differs from no entry.
            raise WifiError("wifi_peer_identity", f"Maintenance IP ARP MAC {peer or 'unresolved'} does not match the AP BSSID")

    def release_for_reboot(self):
        self.remove_static()
        # Spec 04 §8: drop stale source association, preserving the saved host
        # profile/radio snapshot and the transient credentials for observation.
        code = self.dll.WlanDisconnect(self.handle, C.byref(self.guid), None)
        if code not in (0, 5023):
            raise RuntimeError(f"Could not release the old SoftAP link ({code})")

    def restore(self):
        errors = []
        try:
            try:
                self.remove_static()
            except (RuntimeError, OSError, subprocess.SubprocessError, KeyboardInterrupt):
                errors.append("temporary IPv4 cleanup not verified")
            if self.temporary:
                code = self.dll.WlanDisconnect(self.handle, C.byref(self.guid), None)
                if code not in (0, 5023):
                    errors.append(f"disconnect {code}")
                code = self.dll.WlanDeleteProfile(
                    self.handle, C.byref(self.guid), self.temporary, None
                )
                if code not in (0, 1168):
                    errors.append(f"delete temporary profile {code}")
                if self.saved:
                    try:
                        self.connect_profile(self.saved["profile"])
                        deadline = time.monotonic() + 30
                        while time.monotonic() < deadline:
                            current = self.connection()
                            if current and current["profile"] == self.saved["profile"]:
                                break
                            time.sleep(1)
                        else:
                            errors.append("previous Wi-Fi profile did not reconnect")
                    except RuntimeError:
                        errors.append("previous Wi-Fi profile connection refused")
            for index, software, _ in self.saved_radios:
                try:
                    self.set_radio(index, software)
                except RuntimeError as error:
                    errors.append(str(error))
            deadline = time.monotonic() + 10
            while True:
                restored = self.inspect_interface(4, Radios)
                if [
                    (r.index, r.software) for r in restored.radios[: restored.count]
                ] == [(i, s) for i, s, _ in self.saved_radios]:
                    break
                if time.monotonic() >= deadline:
                    errors.append("Wi-Fi radio state differs")
                    break
                time.sleep(0.5)
            if not self.saved and self.connection() is not None:
                errors.append("Wi-Fi was not restored to disconnected state")
        finally:
            self.profile_xml = None
            self.dll.WlanCloseHandle(self.handle, None)
        if errors:
            raise RuntimeError("Network restoration failed: " + "; ".join(errors))
        return {
            "restored": True,
            "previouslyConnected": bool(self.saved),
            "temporaryProfileRemoved": True,
            "radioStatesRestored": True,
            "temporaryIpv4Absent": self.static_address is None,
        }


class DeploymentLock:
    """Per-Windows-session mutex also prevents two devices racing one Wi-Fi adapter."""

    def __init__(self):
        self.kernel = C.WinDLL("kernel32.dll", use_last_error=True)
        self.kernel.CreateMutexW.argtypes = [C.c_void_p, D, C.c_wchar_p]
        self.kernel.CreateMutexW.restype = C.c_void_p
        self.kernel.CloseHandle.argtypes = [C.c_void_p]
        self.kernel.ReleaseMutex.argtypes = [C.c_void_p]
        C.set_last_error(0)
        self.handle = self.kernel.CreateMutexW(
            None, True, "Local\\PickleScore-Agent-Deployment"
        )
        if not self.handle:
            raise RuntimeError("Cannot acquire Windows deployment mutex")
        if C.get_last_error() == 183:
            self.kernel.CloseHandle(self.handle)
            self.handle = None
            raise RuntimeError("Another agent deployment owns the Windows adapter")

    def close(self):
        if self.handle:
            self.kernel.ReleaseMutex(self.handle)
            self.kernel.CloseHandle(self.handle)
            self.handle = None

"""Windows WLAN adapter for Spec04 §8. Secrets stay in native API memory.

No route/metric changes, saved-profile overwrites, password arguments or XML files.
"""

import ctypes as C
import http.client
import json
import ssl
import subprocess
import time
import uuid
from xml.sax.saxutils import escape

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


class ConnectParameters(C.Structure):
    _fields_ = [
        ("mode", D),
        ("profile", C.c_wchar_p),
        ("ssid", C.c_void_p),
        ("bssidList", C.c_void_p),
        ("bssType", D),
        ("flags", D),
    ]


def powershell(script):
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
        timeout=25,
        check=False,
    )
    if result.returncode:
        raise RuntimeError("Windows network inspection failed (PowerShell)")
    try:
        return json.loads(result.stdout)
    except (ValueError, TypeError) as error:
        raise RuntimeError("Invalid Windows network inspection response") from error


def route(remote):
    # Fixed literal addresses only, never interpolate untrusted command text.
    if remote not in ("1.1.1.1", "192.168.4.1"):
        raise ValueError("Unsupported route target")
    return powershell(
        "$r=@(Find-NetRoute -RemoteIPAddress '" + remote + "'); "
        "@{index=$r[0].InterfaceIndex; address=$r[0].IPAddress; "
        "nextHop=$r[1].NextHop; prefix=$r[1].DestinationPrefix} | ConvertTo-Json -Compress"
    )


def ethernet_probe():
    r = route("1.1.1.1")
    adapters = powershell(
        "Get-NetAdapter -Physical | Select-Object ifIndex,MediaType,Status | ConvertTo-Json -Compress"
    )
    if isinstance(adapters, dict):
        adapters = [adapters]
    if not any(
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
        self.saved = None
        self.saved_radios = []
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
        self.saved = self.connection()
        radios = self.inspect_interface(4, Radios)
        self.saved_radios = [
            (r.index, r.software, r.hardware) for r in radios.radios[: radios.count]
        ]
        guid = str(uuid.UUID(bytes_le=bytes(self.guid.data)))
        adapters = powershell(
            "Get-NetAdapter -Physical | Select-Object ifIndex,InterfaceGuid | ConvertTo-Json -Compress"
        )
        if isinstance(adapters, dict):
            adapters = [adapters]
        self.index = next(
            a["ifIndex"]
            for a in adapters
            if a["InterfaceGuid"].strip("{}").lower() == guid
        )

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

    def connect_profile(self, profile):
        transient = profile == self.temporary and self.profile_xml is not None
        params = ConnectParameters(
            1 if transient else 0,
            self.profile_xml if transient else profile,
            None,
            None,
            1,
            0,
        )
        self.check(
            self.dll.WlanConnect(
                self.handle, C.byref(self.guid), C.byref(params), None
            ),
            "connect",
        )

    def connect(self, ssid, password, expected_bssid=None, timeout=45, *, retry=True):
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
        self.dll.WlanScan(self.handle, C.byref(self.guid), None, None, None)
        time.sleep(2)  # radio startup and first scan are asynchronous
        self.connect_profile(self.temporary)
        deadline = time.monotonic() + timeout
        retry_at = time.monotonic() + 25
        while time.monotonic() < deadline:
            current = self.connection()
            if current and current["ssid"] == ssid:
                if expected_bssid and current["bssid"] != expected_bssid.lower():
                    raise RuntimeError(
                        "Wrong SoftAP BSSID; no credentials sent to HTTP"
                    )
                r = route("192.168.4.1")
                if (
                    r["index"] == self.index
                    and r["prefix"] == "192.168.4.0/24"
                    and r["nextHop"] == "0.0.0.0"
                    and r["address"].startswith("192.168.4.")
                    and r["address"] != "192.168.4.1"
                ):
                    self.address = r["address"]
                    self.current = current
                    return {
                        "interfaceIndex": self.index,
                        "sourceAddress": self.address,
                        **current,
                    }
            if retry and not current and time.monotonic() >= retry_at:
                # Do not interrupt a WPA handshake; it can legitimately take >8 seconds.
                try:
                    disconnected = self.inspect_interface(7, Connection).state == 4
                except RuntimeError:
                    disconnected = True
                if disconnected:
                    self.dll.WlanScan(self.handle, C.byref(self.guid), None, None, None)
                    self.connect_profile(self.temporary)
                retry_at = time.monotonic() + 25
            time.sleep(1)
        raise RuntimeError(
            "SoftAP connection/DHCP/routing timeout (credentials, window or adapter)"
        )

    def release_for_reboot(self):
        # Spec 04 §8: drop stale source association, preserving the saved host
        # profile/radio snapshot and the transient credentials for observation.
        code = self.dll.WlanDisconnect(self.handle, C.byref(self.guid), None)
        if code not in (0, 5023):
            raise RuntimeError(f"Could not release the old SoftAP link ({code})")

    def restore(self):
        errors = []
        try:
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

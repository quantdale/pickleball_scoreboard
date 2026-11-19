"""Windows IP Helper bindings for the local OTA transport (Spec 04 §8)."""

import ctypes as C
import socket

D = C.c_uint32
B = C.c_ubyte
P = C.POINTER
AF_INET = socket.AF_INET


# netioapi.h uses SOCKADDR_INET (28 bytes, aligned to ULONG), including for IPv4.
class Address(C.Structure):
    _fields_ = [("family", C.c_uint16), ("port", C.c_uint16), ("ip", B * 4), ("rest", D * 5)]

    @classmethod
    def ipv4(cls, value):
        return cls(AF_INET, 0, (B * 4).from_buffer_copy(socket.inet_aton(value)))

    def text(self):
        if self.family != AF_INET:
            raise RuntimeError("IP Helper did not return an IPv4 address")
        return socket.inet_ntoa(bytes(self.ip))


class Unicast(C.Structure):
    _fields_ = [("address", Address), ("luid", C.c_uint64), ("index", D), ("origin", D),
                ("suffixOrigin", D), ("valid", D), ("preferred", D), ("prefix", B),
                ("skip", B), ("dad", D), ("scope", D), ("created", C.c_int64)]


class Prefix(C.Structure):
    _fields_ = [("address", Address), ("length", B)]


class Route(C.Structure):
    _fields_ = [("luid", C.c_uint64), ("index", D), ("destination", Prefix),
                ("nextHop", Address), ("sitePrefixLength", B), ("valid", D), ("preferred", D),
                ("metric", D), ("protocol", D), ("loopback", B), ("autoconfigure", B),
                ("publish", B), ("immortal", B), ("age", D), ("origin", D)]


class Neighbor(C.Structure):
    _fields_ = [("address", Address), ("index", D), ("luid", C.c_uint64), ("mac", B * 32),
                ("length", D), ("state", D), ("flags", B), ("reachability", D)]


class IpHelper:
    def __init__(self):
        self.dll = C.WinDLL("iphlpapi.dll")
        signatures = {
            "GetUnicastIpAddressTable": ([C.c_uint16, P(C.c_void_p)], D),
            "GetIpForwardTable2": ([C.c_uint16, P(C.c_void_p)], D),
            "FreeMibTable": ([C.c_void_p], None),
            "GetBestInterfaceEx": ([P(Address), P(D)], D),
            "GetBestRoute2": ([P(C.c_uint64), D, P(Address), P(Address), D, P(Route), P(Address)], D),
            "GetIpNetEntry2": ([P(Neighbor)], D),
            "ConvertInterfaceGuidToLuid": ([C.c_void_p, P(C.c_uint64)], D),
            "ConvertInterfaceLuidToIndex": ([P(C.c_uint64), P(D)], D),
            "InitializeUnicastIpAddressEntry": ([P(Unicast)], None),
            "CreateUnicastIpAddressEntry": ([P(Unicast)], D),
            "DeleteUnicastIpAddressEntry": ([P(Unicast)], D),
        }
        for name, (args, result) in signatures.items():
            fn = getattr(self.dll, name)
            fn.argtypes, fn.restype = args, result

    @staticmethod
    def check(code, action):
        if code:
            raise OSError(code, f"Windows IP Helper {action} failed ({code})")

    def adapter(self, guid):
        luid, index = C.c_uint64(), D()
        self.check(self.dll.ConvertInterfaceGuidToLuid(C.byref(guid), C.byref(luid)), "GUID to LUID")
        self.check(self.dll.ConvertInterfaceLuidToIndex(C.byref(luid), C.byref(index)), "LUID to index")
        return luid.value, index.value

    def table(self, fetch, kind):
        class Table(C.Structure):
            _fields_ = [("count", D), ("first", kind)]

        ptr = C.c_void_p()
        self.check(fetch(AF_INET, C.byref(ptr)), "read IPv4 table")
        try:
            if ptr.value is None:
                raise RuntimeError("Empty IP Helper table pointer")
            count = C.cast(ptr, P(D))[0]
            return tuple((kind * count).from_buffer_copy(
                C.string_at(ptr.value + Table.first.offset, count * C.sizeof(kind))))
        finally:
            self.dll.FreeMibTable(ptr)

    def addresses(self):
        return [{"index": r.index, "luid": r.luid, "address": r.address.text(), "prefixLength": r.prefix,
                 "skipAsSource": bool(r.skip), "origin": {0: "Other", 1: "Manual", 2: "WellKnown", 3: "Dhcp"}.get(r.origin),
                 "state": {0: "Invalid", 1: "Tentative", 2: "Duplicate", 3: "Deprecated", 4: "Preferred"}.get(r.dad)}
                for r in self.table(self.dll.GetUnicastIpAddressTable, Unicast)]

    @staticmethod
    def route_row(row):
        return {"index": row.index, "luid": row.luid,
                "prefix": f"{row.destination.address.text()}/{row.destination.length}", "nextHop": row.nextHop.text()}

    def routes(self):
        return [self.route_row(r) for r in self.table(self.dll.GetIpForwardTable2, Route)]

    def selection(self, remote):
        destination, source, row = Address.ipv4(remote), Address(), Route()
        index = D()
        # Select globally first, not on the caller's preferred adapter. Resolve the source
        # on the selected interface; the policy must separately pin both to this WLAN.
        code = self.dll.GetBestInterfaceEx(C.byref(destination), C.byref(index))
        if not code:
            code = self.dll.GetBestRoute2(None, index, None, C.byref(destination), 0, C.byref(row), C.byref(source))
        if code in (2, 1168, 1231):  # no route / interface currently unavailable
            return []
        self.check(code, "select route/source")
        return [{"kind": "address", "index": index.value, "address": source.text()},
                {"kind": "route", **self.route_row(row)}]

    def neighbor(self, address, luid, index):
        row = Neighbor(Address.ipv4(address), index, luid)
        code = self.dll.GetIpNetEntry2(C.byref(row))
        if code in (2, 1168):
            return None
        self.check(code, "read ARP neighbor")
        mac = bytes(row.mac[:6])
        return mac.hex(":") if row.length == 6 and any(mac) else None

    def add(self, address, luid, index, prefix):
        row = Unicast()
        self.dll.InitializeUnicastIpAddressEntry(C.byref(row))
        row.address, row.luid, row.index = Address.ipv4(address), luid, index
        row.origin = row.suffixOrigin = 1  # Manual
        row.prefix, row.skip, row.dad = prefix, False, 1  # never optimistic DAD
        row.valid = row.preferred = 300  # OS backstop if forcibly terminated
        return self.dll.CreateUnicastIpAddressEntry(C.byref(row))

    def delete(self, address, luid, index):
        row = Unicast(address=Address.ipv4(address), luid=luid, index=index)
        code = self.dll.DeleteUnicastIpAddressEntry(C.byref(row))
        if code not in (2, 1168):  # expired or adapter already removed it
            self.check(code, "remove temporary IPv4")

"""Offline UART evidence for the pinned ESP32; never substitutes for live OTA health."""

import re

# Spec 04 §§7/9: correlate ROM output with the application install marker.
_RESET = re.compile(
    rb"^rst:0x([0-9a-f]+) \(([^\r\n)]+)\)", re.IGNORECASE | re.MULTILINE
)
_FAULT = re.compile(
    rb"Guru Meditation|Backtrace:|stack canary|stack overflow|Brownout|assert failed|"
    rb"task watchdog got|watchdog timeout|startup self-check failed|"
    rb"OTA (?:HEAP|INIT) rejected|OTA cleanup incomplete",
    re.IGNORECASE,
)


def expected_install_reset(uart: bytes, build: str) -> bool:
    """Check the reset only, not the outcome of boot or the running image hash."""
    resets = list(_RESET.finditer(uart))
    if len(resets) != 1 or _FAULT.search(uart):
        return False
    reset = resets[0]
    marker = f"OTA installing build={build}; reboot resets in-memory score".encode()
    # Pinned ESP32 ROM rtc.h: SW_CPU_RESET is decimal 12; SW_RESET is decimal 3.
    return (
        int(reset[1], 16) == 12
        and reset[2] == b"SW_CPU_RESET"
        and 0 <= uart.find(marker) < reset.start()
    )

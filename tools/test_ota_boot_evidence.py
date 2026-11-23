"""Spec 04 §§7/9: offline regression for UART trial evidence, never a live health check."""

import unittest

from tools.ota_boot_evidence import expected_install_reset

INSTALL = b"OTA installing build=bench-target; reboot resets in-memory score\r\n"
RESET = b"rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\r\n"
BOOT = b"OTA build=bench-target ABI=PKSCORE-OTA-ABI1 slot=ota_0 pending=1\r\n"
CONFIRM = b"OTA boot confirmed build=bench-target heap=67136 min_heap=66772\r\n"


class BootEvidenceTest(unittest.TestCase):
    def test_esp32_sw_cpu_reset_is_hex_c_not_hex_3(self):
        self.assertTrue(expected_install_reset(INSTALL + RESET, "bench-target"))
        self.assertFalse(
            expected_install_reset(
                INSTALL + RESET.replace(b"0xc", b"0x3"), "bench-target"
            )
        )
        self.assertFalse(
            expected_install_reset(INSTALL + b"rst:0x3 (SW_RESET)\n", "bench-target")
        )

    def test_expected_reset_requires_matching_install_marker(self):
        self.assertFalse(expected_install_reset(RESET + INSTALL, "bench-target"))
        self.assertFalse(expected_install_reset(INSTALL + RESET, "other-build"))
        self.assertFalse(expected_install_reset(INSTALL, "bench-target"))

    def test_rejects_extra_or_faulted_reset(self):
        self.assertFalse(
            expected_install_reset(INSTALL + RESET + RESET, "bench-target")
        )
        self.assertFalse(
            expected_install_reset(
                INSTALL + b"rst:0xc (TG1WDT_SYS_RESET)\n", "bench-target"
            )
        )
        self.assertFalse(
            expected_install_reset(
                INSTALL + RESET + b"Stack canary watchpoint triggered\n", "bench-target"
            )
        )

    def test_boot_confirmation_can_exist_without_network_evidence(self):
        self.assertTrue(
            expected_install_reset(INSTALL + RESET + BOOT + CONFIRM, "bench-target")
        )
        # This classifier only attests the reset signal, not the new build or boot health.
        self.assertTrue(expected_install_reset(INSTALL + RESET + BOOT, "bench-target"))


if __name__ == "__main__":
    unittest.main()

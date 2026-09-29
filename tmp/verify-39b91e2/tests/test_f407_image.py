"""Artifact contract; this does not execute firmware or validate hardware."""
import os
from pathlib import Path
import struct
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build" / "F4"
PREFIX = os.environ.get("CROSS_COMPILE", "arm-none-eabi-")


class FirmwareImage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = (BUILD / "SmartCar.bin").read_bytes()
        cls.symbols = {}
        listing = subprocess.check_output(
            [PREFIX + "nm", "-n", str(BUILD / "SmartCar.elf")], text=True)
        for line in listing.splitlines():
            fields = line.split()
            if len(fields) == 3:
                cls.symbols[fields[2]] = (int(fields[0], 16), fields[1])

    def test_flash_and_stack_bounds(self):
        self.assertLessEqual(len(self.binary), 512 * 1024)
        sp, reset = struct.unpack_from("<II", self.binary)
        self.assertEqual(sp, 0x20020000)  # 128 KiB SRAM, not SRAM + CCM
        self.assertEqual(reset, self.symbols["Reset_Handler"][0] | 1)
        self.assertEqual(self.symbols["g_pfnVectors"][0], 0x08000000)
        self.assertLessEqual(self.symbols["_end"][0] + 0x800, sp)

    def test_vector_ownership(self):
        # Cortex exceptions and STM32F407 IRQ numbers from the device vector table.
        for vector, name in [(15, "SysTick_Handler"), (16 + 54, "TIM6_DAC_IRQHandler"),
                             (16 + 40, "EXTI15_10_IRQHandler"),
                             (16 + 38, "USART2_IRQHandler")]:
            with self.subTest(handler=name):
                address, kind = self.symbols[name]
                self.assertEqual(kind.upper(), "T")  # strong, not startup weak alias
                self.assertEqual(struct.unpack_from("<I", self.binary, vector * 4)[0],
                                 address | 1)
        self.assertEqual(self.symbols["TIM4_IRQHandler"][0],
                         self.symbols["Default_Handler"][0])
        self.assertEqual(self.symbols["EXTI0_IRQHandler"][0],
                         self.symbols["Default_Handler"][0])

    def test_real_runtime_and_no_legacy_application(self):
        for name in ("SystemInit", "HAL_Init", "HAL_IncTick", "Bsp_Init", "main"):
            self.assertIn(name, self.symbols)
        for name in ("App_Init", "Motor_SetPWM", "HAL_PWREx_EnableOverDrive"):
            self.assertNotIn(name, self.symbols)
        self.assertGreater((BUILD / "SmartCar.hex").stat().st_size, 0)
        self.assertGreater((BUILD / "SmartCar.map").stat().st_size, 0)
        # Every Intel HEX record must have a correct checksum.
        lines = (BUILD / "SmartCar.hex").read_text().splitlines()
        for line in lines:
            self.assertTrue(line.startswith(":"))
            record = bytes.fromhex(line[1:])
            self.assertEqual(len(record), record[0] + 5)
            self.assertEqual(sum(record) & 255, 0)
        self.assertEqual(lines[-1], ":00000001FF")


if __name__ == "__main__":
    unittest.main()

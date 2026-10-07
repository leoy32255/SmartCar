import os
from pathlib import Path
import subprocess
import unittest
ROOT = Path(__file__).resolve().parents[1]
class Sensors(unittest.TestCase):
    def test_public_sensor_contract(self):
        out = ROOT / "build/host/sensors.exe"
        out.parent.mkdir(parents=True, exist_ok=True)
        cmd = [os.environ.get("HOST_CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-ICore/Inc", "Core/F407/sensors.c", "tests/sensors_harness.c", "-o", str(out)]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(out)], check=True, timeout=20)

    def test_chip_select_release_on_hal_error(self):
        out = ROOT / "build/host/sensor_port.exe"
        out.parent.mkdir(parents=True, exist_ok=True)
        cmd = [os.environ.get("HOST_CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-DSTM32F407xx", "-DUSE_HAL_DRIVER", "-D__SOFTFP__", "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast"]
        for folder in ("Core/Inc", "Drivers/STM32F4xx_HAL_Driver/Inc", "Drivers/CMSIS/Device/ST/STM32F4xx/Include", "Drivers/CMSIS/Include"):
            cmd.append("-I"+folder)
        cmd += ["Core/F407/sensor_port.c", "tests/sensor_port_harness.c", "-o", str(out)]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(out)], check=True, timeout=20)
    def test_uart_ring_overflow_and_tx_wrap(self):
        out = ROOT / "build/host/uart.exe"
        out.parent.mkdir(parents=True, exist_ok=True)
        cmd = [os.environ.get("HOST_CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-DSTM32F407xx", "-DUSE_HAL_DRIVER", "-D__SOFTFP__", "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast"]
        for folder in ("Core/Inc", "Drivers/STM32F4xx_HAL_Driver/Inc", "Drivers/CMSIS/Device/ST/STM32F4xx/Include", "Drivers/CMSIS/Include"):
            cmd.append("-I"+folder)
        cmd += ["tests/uart_harness.c", "-o", str(out)]
        subprocess.run(cmd, cwd=ROOT, check=True)
        subprocess.run([str(out)], check=True, timeout=20)

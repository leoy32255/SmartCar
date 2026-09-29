"""Run the board initialization with real ST types and simulated HAL/MMIO."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BoardInitialization(unittest.TestCase):
    def test_revision3_configuration_and_safe_outputs(self):
        output = ROOT / "build" / "host"
        output.mkdir(parents=True, exist_ok=True)
        executable = output / ("bsp_test.exe" if os.name == "nt" else "bsp_test")
        command = [os.environ.get("HOST_CC", "gcc"), "-std=gnu11", "-O0", "-g",
                   "-DSTM32F407xx", "-DUSE_HAL_DRIVER", "-D__SOFTFP__",
                   "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast"]
        for folder in ("Core/Inc", "Drivers/STM32F4xx_HAL_Driver/Inc",
                       "Drivers/CMSIS/Device/ST/STM32F4xx/Include", "Drivers/CMSIS/Include"):
            command.append("-I" + str(ROOT / folder))
        command += [str(ROOT / "Core/F407/bsp.c"), str(ROOT / "tests/bsp_harness.c"),
                    "-o", str(executable)]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()

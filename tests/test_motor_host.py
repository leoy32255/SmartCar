"""Execute the F407 motor API and real BSP with simulated peripheral access."""
import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MotorDriver(unittest.TestCase):
    def test_motor_output_contract(self):
        self.run_motor(0, 0)

    def test_left_polarity_correction(self):
        self.run_motor(1, 0)

    def test_right_polarity_correction(self):
        self.run_motor(0, 1)

    def test_both_polarity_corrections(self):
        self.run_motor(1, 1)

    def run_motor(self, left_invert, right_invert):
        output = ROOT / "build" / "host"
        output.mkdir(parents=True, exist_ok=True)
        executable = output / (f"motor_test_{left_invert}{right_invert}" +
                               (".exe" if os.name == "nt" else ""))
        command = [os.environ.get("HOST_CC", "gcc"), "-std=gnu11", "-O0", "-g",
                   "-Wall", "-Wextra", "-Werror", "-DTEST_MOTOR",
                   "-DSTM32F407xx", "-DUSE_HAL_DRIVER", "-D__SOFTFP__",
                   "-Wno-pointer-to-int-cast", "-Wno-int-to-pointer-cast"]
        command += [f"-DBSP_MOTOR_LEFT_INVERT={left_invert}",
                    f"-DBSP_MOTOR_RIGHT_INVERT={right_invert}"]
        for folder in ("Core/Inc", "Drivers/STM32F4xx_HAL_Driver/Inc",
                       "Drivers/CMSIS/Device/ST/STM32F4xx/Include", "Drivers/CMSIS/Include"):
            command.append("-I" + str(ROOT / folder))
        sources = ["Core/F407/bsp.c", "Core/F407/motor.c",
                   "tests/bsp_harness.c", "tests/motor_harness.c"]
        command += [str(ROOT / source) for source in sources] + ["-o", str(executable)]
        subprocess.run(command, check=True)
        result = subprocess.run([str(executable)], capture_output=True,
                                text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("Motor direction and duty-boundary checks passed", result.stdout)


if __name__ == "__main__":
    unittest.main()

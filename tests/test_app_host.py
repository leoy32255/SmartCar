import os
from pathlib import Path
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[1]
class AppTests(unittest.TestCase):
    def test_motion_app(self):self.run_app(0)
    def test_diagnostic_app(self):self.run_app(1)
    def run_app(self,diagnostic):
        out=ROOT/"build/host"/f"app_{diagnostic}.exe"
        out.parent.mkdir(parents=True,exist_ok=True)
        cmd=[os.environ.get("HOST_CC","gcc"),"-std=gnu11","-Wall","-Wextra","-Werror","-DTEST_APP",f"-DAPP_DIAGNOSTIC={diagnostic}","-DSTM32F407xx","-DUSE_HAL_DRIVER","-D__SOFTFP__","-Wno-pointer-to-int-cast","-Wno-int-to-pointer-cast"]
        for folder in ("Core/Inc","Drivers/STM32F4xx_HAL_Driver/Inc","Drivers/CMSIS/Device/ST/STM32F4xx/Include","Drivers/CMSIS/Include"):cmd.append("-I"+folder)
        cmd += ["Core/F407/"+f+".c" for f in ("bsp","motor","sensors","sensor_port","control","runtime","app","interrupts")]
        cmd += ["tests/bsp_harness.c","tests/app_harness.c","-o",str(out)]
        subprocess.run(cmd,cwd=ROOT,check=True)
        subprocess.run([str(out)],check=True,timeout=20)

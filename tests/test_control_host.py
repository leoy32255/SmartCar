import os
from pathlib import Path
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ControlTests(unittest.TestCase):
    def test_control_safety_contract(self):
        out=ROOT/"build/host/control.exe"
        out.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run([os.environ.get("HOST_CC","gcc"),"-std=c11","-Wall","-Wextra","-Werror","-ICore/Inc","Core/F407/control.c","tests/control_harness.c","-o",str(out)],cwd=ROOT,check=True)
        subprocess.run([str(out)],check=True,timeout=20)

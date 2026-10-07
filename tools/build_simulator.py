"""Build the actual C protocol/controller as a host-only simulated device."""
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
output=ROOT/"build/host"/("smartcar_sim.dll" if os.name=="nt" else "smartcar_sim.so")
output.parent.mkdir(parents=True,exist_ok=True)
cmd=[os.environ.get("HOST_CC","gcc"),"-std=c11","-O2","-Wall","-Wextra","-Werror","-shared"]
if os.name!="nt":cmd.append("-fPIC")
cmd += ["-ICore/Inc","Core/F407/control.c","Core/F407/runtime.c","panel/simulator.c","-o",str(output)]
subprocess.run(cmd,cwd=ROOT,check=True)
print(output)

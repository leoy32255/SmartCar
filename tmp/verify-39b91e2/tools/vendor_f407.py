"""Import unmodified official files from pinned checkouts; write hash manifest.

Usage: python tools/vendor_f407.py <HAL checkout> <device checkout> <CMSIS_5 checkout>
Run only when intentionally restoring/updating dependencies, not at build time.
"""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = [
    ("https://github.com/STMicroelectronics/stm32f4xx-hal-driver", "v1.8.3",
     "c2e1406d7ea4b73aa42b98ddeb75a8670b1a5a16"),
    ("https://github.com/STMicroelectronics/cmsis-device-f4", "v2.6.10",
     "5f41fb29d22773896c780052bf61e47fc924d524"),
    ("https://github.com/ARM-software/CMSIS_5", "5.9.0",
     "2b7495b8535bdcb306dac29b9ded4cfb679d7e5c"),
]


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    checkouts = [Path(p).resolve() for p in sys.argv[1:]]
    for folder, (_, _, commit) in zip(checkouts, SOURCES):
        actual = subprocess.check_output(["git", "-C", str(folder), "rev-parse", "HEAD"], text=True).strip()
        if actual != commit:
            raise SystemExit(f"Unexpected upstream commit in {folder}: {actual}")
        if subprocess.check_output(["git", "-C", str(folder), "status", "--porcelain"], text=True).strip():
            raise SystemExit(f"Modified upstream checkout: {folder}")

    manifest = {"sources": [dict(url=u, tag=t, commit=c) for u, t, c in SOURCES], "files": {}}

    def copy(source_index, relative, target):
        source = checkouts[source_index] / relative
        destination = ROOT / target
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        manifest["files"][target] = dict(source=source_index, path=relative,
            sha256=hashlib.sha256(destination.read_bytes()).hexdigest())

    modules = "hal hal_cortex hal_rcc hal_rcc_ex hal_pwr hal_pwr_ex hal_flash hal_flash_ex hal_gpio hal_tim hal_tim_ex hal_uart hal_spi hal_dma".split()
    headers = modules + ["hal_def", "hal_dma_ex", "hal_flash_ramfunc", "hal_gpio_ex"]
    for module in modules:
        name = f"Src/stm32f4xx_{module}.c"
        copy(0, name, "Drivers/STM32F4xx_HAL_Driver/" + name)
    for module in headers:
        name = f"Inc/stm32f4xx_{module}.h"
        copy(0, name, "Drivers/STM32F4xx_HAL_Driver/" + name)
    copy(0, "Inc/Legacy/stm32_hal_legacy.h", "Drivers/STM32F4xx_HAL_Driver/Inc/Legacy/stm32_hal_legacy.h")
    copy(0, "LICENSE.md", "Drivers/STM32F4xx_HAL_Driver/LICENSE.md")
    for name in ["Include/stm32f4xx.h", "Include/stm32f407xx.h", "Include/system_stm32f4xx.h",
                 "Source/Templates/system_stm32f4xx.c", "LICENSE.md"]:
        copy(1, name, "Drivers/CMSIS/Device/ST/STM32F4xx/" + name)
    copy(1, "Source/Templates/gcc/startup_stm32f407xx.s", "Core/Startup/startup_stm32f407xx.s")
    for path in sorted((checkouts[2] / "CMSIS/Core/Include").glob("*.h")):
        copy(2, "CMSIS/Core/Include/" + path.name, "Drivers/CMSIS/Include/" + path.name)
    copy(2, "LICENSE.txt", "Drivers/CMSIS/LICENSE.txt")
    (ROOT / "Drivers/manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Vendored {len(manifest['files'])} unchanged upstream files")


if __name__ == "__main__":
    main()

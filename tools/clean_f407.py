"""Remove only this repository's F407 output, refusing redirected paths."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
target = root / "build" / "F4"
if target.resolve() != target or target.is_symlink():
    raise SystemExit("Refusing to clean a redirected build directory")
if target.exists():
    shutil.rmtree(target)

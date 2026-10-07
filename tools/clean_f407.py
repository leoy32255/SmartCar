"""Remove only both repository F407 output directories, refusing redirection."""
from pathlib import Path
import shutil
root=Path(__file__).resolve().parents[1]
for name in ("F4","F4-diagnostic"):
    target=root/"build"/name
    if target.resolve()!=target or target.is_symlink():
        raise SystemExit("Refusing to clean a redirected build directory")
    if target.exists():shutil.rmtree(target)

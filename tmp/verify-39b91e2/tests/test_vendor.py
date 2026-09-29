import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class VendorIntegrity(unittest.TestCase):
    def test_pinned_upstream_bytes(self):
        manifest = json.loads((ROOT / "Drivers/manifest.json").read_text(encoding="utf-8"))
        for path, record in manifest["files"].items():
            with self.subTest(path=path):
                self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), record["sha256"])


if __name__ == "__main__":
    unittest.main()

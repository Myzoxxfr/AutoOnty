"""Version bumps must leave Windows dependency versions alone."""

import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
import xml.etree.ElementTree as ET


class VersionBumpTest(unittest.TestCase):
    def test_bump_preserves_dependencies_and_source_boms(self):
        root = Path(__file__).resolve().parents[1]
        spec = importlib.util.spec_from_file_location(
            "bump_version", root / "scripts/bump_version.py"
        )
        bump = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(bump)
        files = ("src/core/update.cpp", "src/app.rc", "src/app.manifest", "version", "README.md")
        with tempfile.TemporaryDirectory() as directory:
            target = Path(directory)
            for name in files:
                destination = target / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(root / name, destination)
            bump.ROOT = directory
            old, current = bump.current_version()
            next_version = (*current[:2], current[2] + 1)
            label = bump.short(next_version)
            full_version = f"{label}.0"
            numeric = next_version[0] * 1000000 + next_version[1] * 1000 + next_version[2]
            bump.apply(old, next_version)
            identity = ET.parse(target / "src/app.manifest").getroot()
            ns = {"asm": "urn:schemas-microsoft-com:asm.v1"}
            self.assertEqual(identity.find("asm:assemblyIdentity", ns).get("version"), full_version)
            dependency = identity.find("asm:dependency/asm:dependentAssembly/asm:assemblyIdentity", ns)
            self.assertEqual(dependency.get("version"), "6.0.0.0")
            self.assertEqual((target / "version").read_bytes(), f"{numeric}\n".encode())
            self.assertIn(f'APP_VERSION = L"v{label}"', (target / "src/core/update.cpp").read_text(encoding="utf-8-sig"))
            self.assertIn("FILEVERSION " + ",".join(map(str, (*next_version, 0))), (target / "src/app.rc").read_text(encoding="utf-8-sig"))
            for name in ("src/core/update.cpp", "src/app.rc"):
                self.assertEqual((target / name).read_bytes()[:3], (root / name).read_bytes()[:3])


if __name__ == "__main__":
    unittest.main()

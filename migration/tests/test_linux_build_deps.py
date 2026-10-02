#!/usr/bin/env python3
"""Unit checks for the local Debian dependency relocation helper."""

import importlib.util
from pathlib import Path
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "prepare_linux_build_deps", REPO / "tools/prepare-linux-build-deps.py")
DEPS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DEPS)


class RelocationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "root"
        self.bin = self.root / "usr/bin"
        self.lib = self.root / "usr/lib/x86_64-linux-gnu"
        self.pcdir = self.lib / "pkgconfig"
        self.bin.mkdir(parents=True)
        self.pcdir.mkdir(parents=True)

    def test_multiarch_pc_and_shebang_relocation_is_idempotent(self):
        pc = self.pcdir / "test.pc"
        pc.write_text("prefix=/usr\nLibs: -L/usr/lib/x86_64-linux-gnu -ltest\n")
        scanner = self.bin / "g-ir-scanner"
        scanner.write_text("#!/usr/bin/python3\nTOOL_PATH = '/usr/bin/tool'\n")
        DEPS.relocate(self.root)
        expected = f"prefix={self.root}/usr\nLibs: -L{self.root}/usr/lib/x86_64-linux-gnu -ltest\n"
        self.assertEqual(pc.read_text(), expected)
        self.assertEqual(scanner.read_text().splitlines()[0], "#!/usr/bin/python3")
        before = scanner.read_text()
        DEPS.relocate(self.root)
        self.assertEqual(pc.read_text(), expected)
        self.assertEqual(scanner.read_text(), before)

    def test_lcms_static_archives_are_kept_as_link_only_dependencies(self):
        scanner = self.bin / "x86_64-linux-gnu-g-ir-scanner"
        scanner.write_text("#!/usr/bin/python3\nargv = [TOOL_PATH] + extra_argv + sys.argv[1:]\n")
        for name in ("lcms2_fast_float", "lcms2_threaded"):
            (self.lib / ("lib" + name + ".a")).touch()
        DEPS.relocate(self.root)
        statement = scanner.read_text().splitlines()[1]
        import types
        context = {"TOOL_PATH": "scanner", "extra_argv": [],
                   "sys": types.SimpleNamespace(argv=["wrapper", "-llcms2",
                       "-llcms2_fast_float", "-llcms2_threaded", "-lglib-2.0"])}
        exec(statement, context)
        self.assertEqual(context["argv"], ["scanner", "-llcms2",
            "--extra-library=lcms2_fast_float",
            "--extra-library=lcms2_threaded", "-lglib-2.0"])
        before = scanner.read_text()
        DEPS.relocate(self.root)
        self.assertEqual(scanner.read_text(), before)

    def test_real_shared_plugins_do_not_receive_static_workaround(self):
        scanner = self.bin / "x86_64-linux-gnu-g-ir-scanner"
        original = "#!/usr/bin/python3\nargv = [TOOL_PATH] + extra_argv + sys.argv[1:]\n"
        scanner.write_text(original)
        for name in ("lcms2_fast_float", "lcms2_threaded"):
            (self.lib / ("lib" + name + ".a")).touch()
            (self.lib / ("lib" + name + ".so")).touch()
        DEPS.relocate(self.root)
        self.assertEqual(scanner.read_text(), original)

    def test_environment_processes_python_package_pth_files(self):
        DEPS.write_env(self.base)
        self.assertIn("site.addsitedir", (self.base / "python/sitecustomize.py").read_text())
        self.assertIn(str(self.base / "python"), (self.base / "env.sh").read_text())
        self.assertTrue((self.base / "bin/pkg-config").stat().st_mode & 0o111)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Source-only controls for the point-shadow runner; never compile or run C/C++.

Every build/version/pkg-config/test subprocess is mocked. Temporary fake object
and executable files exercise runner bookkeeping only and are not native test
evidence. The only real subprocesses read the hash-pinned local git objects.
"""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from unittest.mock import patch


RUNNER_PATH = Path(__file__).with_name("run_filter_point_shadow_tests.py")
runner = types.ModuleType("point_shadow_runner")
runner.__file__ = str(RUNNER_PATH)
exec(compile(RUNNER_PATH.read_bytes(), str(RUNNER_PATH), "exec"), runner.__dict__)
ORIGINAL_ROOT = runner.ROOT


class RunnerSourceControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pinned = {
            f"{runner.LEGACY_COMMIT}:{name}": subprocess.check_output(
                ["git", "show", f"{runner.LEGACY_COMMIT}:{name}"], cwd=ORIGINAL_ROOT,
                env=dict(os.environ, GIT_NO_LAZY_FETCH="1"))
            for name in runner.LEGACY_SOURCES
        }
        cls.files = runner.source_paths() + runner.fixture_input_paths()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="point-shadow-python-controls-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for source in self.files:
            target = self.root / source.relative_to(ORIGINAL_ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(runner, "ROOT", self.root))
        self.stack.enter_context(patch.object(runner, "__file__", str(self.root / RUNNER_PATH.relative_to(ORIGINAL_ROOT))))
        self.git = self.stack.enter_context(patch.object(runner.subprocess, "check_output", side_effect=self.git_show))
        self.subprocesses = self.stack.enter_context(patch.object(runner.subprocess, "run", side_effect=self.fake_run))
        self.environment = {
            "CC": "mock-cc --c-driver", "CXX": "mock-cxx --cxx-driver",
            "CPPFLAGS": "-DCOMMON_CONTROL=1", "CFLAGS": "-DC_ONLY_CONTROL=1",
            "CXXFLAGS": "-DCXX_ONLY_CONTROL=1", "LDFLAGS": "-Wl,--mock-link-control",
            "ASAN_OPTIONS": "symbolize=0", "UBSAN_OPTIONS": "report_error_type=1",
        }
        self.stack.enter_context(patch.dict(os.environ, self.environment))
        self.commands = []
        self.after_compile = None
        self.after_test = None
        self.fail_c_compile = False

    def git_show(self, command, **kwargs):
        self.assertEqual(command[:2], ["git", "show"])
        self.assertEqual(kwargs["cwd"], self.root)
        self.assertEqual(kwargs["env"]["GIT_NO_LAZY_FETCH"], "1")
        return self.pinned[command[2]]

    def fake_run(self, command, **kwargs):
        """No command is executed, including the fake executable below."""
        self.commands.append((command, kwargs))
        if "-o" in command:
            if "-x" in command and self.fail_c_compile:
                return subprocess.CompletedProcess(command, 1, "", "mock C failure")
            Path(command[command.index("-o") + 1]).write_bytes(b"mock artifact, not native executable\n")
            if self.after_compile:
                self.after_compile()
                self.after_compile = None
        elif command[0].endswith("painter-filter-point-shadow") and self.after_test:
            self.after_test()
        return subprocess.CompletedProcess(command, 0, "", "")

    def invoke(self, *options):
        report_path = self.root / "control-only-report.json"
        build_path = self.root / "mock-build"
        with patch.object(sys, "argv", [str(RUNNER_PATH), str(build_path), "--report", str(report_path), *options]), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = runner.main()
        return result, json.loads(report_path.read_text()), build_path

    def test_literal_oracle_and_all_archived_subsets(self):
        paths, provenance = runner.verify_fixtures()
        self.assertEqual(paths, runner.fixture_input_paths())
        self.assertEqual(len(paths), 50)
        self.assertEqual([provenance[name] for name in
                          ("threshold_alpha_cases", "max_rgb_cases", "value_invert_cases")], [20, 16, 4])
        self.assertTrue(provenance["value_invert_c_oracle"]["literal_callback_matches_pinned_source"])
        self.subprocesses.assert_not_called()

    def test_callback_rewrite_is_rejected(self):
        oracle = self.root / runner.VALUE_INVERT_ORACLE
        oracle.write_bytes(oracle.read_bytes().replace(b"value2 = value / 2;", b"value2 = 0;"))
        with self.assertRaisesRegex(ValueError, "not the literal pinned callback"):
            runner.verify_fixtures()
        self.subprocesses.assert_not_called()

    def test_scaffold_rewrite_is_rejected(self):
        oracle = self.root / runner.VALUE_INVERT_ORACLE
        oracle.write_bytes(oracle.read_bytes().replace(b"typedef int", b"typedef long"))
        with self.assertRaisesRegex(ValueError, "types/macros/wrapper seal differs"):
            runner.verify_fixtures()
        self.subprocesses.assert_not_called()

    def test_each_value_invert_output_hash_is_verified(self):
        base = (self.root / runner.MANIFEST).parent
        outputs = sorted(base.glob("*-plug-in-vinvert-0.raw"))
        self.assertEqual(len(outputs), 4)
        for output in outputs:
            original = output.read_bytes()
            with self.subTest(output=output.name):
                output.write_bytes(bytes([original[0] ^ 1]) + original[1:])
                with self.assertRaisesRegex(ValueError, "Archived original output hash differs"):
                    runner.verify_fixtures()
                output.write_bytes(original)
        self.subprocesses.assert_not_called()

    def test_value_invert_capture_source_hash_is_verified(self):
        capture_path = (self.root / runner.MANIFEST).parent / "capture-report.json"
        capture = json.loads(capture_path.read_text())
        capture["source_sha256"]["value-invert.c"] = "0" * 64
        capture_path.write_text(json.dumps(capture))
        with self.assertRaisesRegex(ValueError, "Archived point source provenance differs: value-invert.c"):
            runner.verify_fixtures()
        self.subprocesses.assert_not_called()

    def test_verify_only_creates_no_build_and_invokes_no_native_commands(self):
        result, report, build = self.invoke("--verify-only", "--sanitize")
        self.assertEqual(result, 0)
        self.assertIsNone(report["all_passed"])
        self.assertTrue(report["source_only_verified"])
        self.assertEqual(report["commands"], [])
        self.assertEqual(report["sanitizers"], [])
        self.assertFalse(build.exists())
        self.subprocesses.assert_not_called()

    def test_separate_c_compile_and_cpp_link_honor_environment(self):
        result, report, _ = self.invoke()
        self.assertEqual(result, 0)
        records = report["commands"]
        c = [record["command"] for record in records if record["kind"] == "compile-c"]
        cpp = [record["command"] for record in records if record["kind"] == "compile-c++"]
        self.assertEqual(len(c), 1)
        self.assertEqual(len(cpp), len(runner.CPP_SOURCES))
        self.assertEqual(c[0][:2], ["mock-cc", "--c-driver"])
        self.assertEqual(c[0][c[0].index("-x") + 1], "c")
        self.assertIn("-std=c99", c[0])
        self.assertIn("-DC_ONLY_CONTROL=1", c[0])
        self.assertNotIn("-DCXX_ONLY_CONTROL=1", c[0])
        for command in c + cpp:
            self.assertIn("-DCOMMON_CONTROL=1", command)
        for command in cpp:
            self.assertEqual(command[:2], ["mock-cxx", "--cxx-driver"])
            self.assertIn("-DCXX_ONLY_CONTROL=1", command)
            self.assertNotIn("-DC_ONLY_CONTROL=1", command)
        link = next(record["command"] for record in records if record["kind"] == "link")
        self.assertIn(c[0][-1], link)
        self.assertIn("-Wl,--mock-link-control", link)
        self.assertIn("value-invert-legacy-oracle.c.o", report["object_sha256"])
        self.assertEqual(report["changed_after_compile"], [])
        self.assertEqual(report["changed_after_run"], [])
        self.assertTrue(report["objects_unchanged"])
        self.assertEqual(report["object_sha256"], report["object_sha256_after_run"])

    def test_sanitizers_apply_to_c_cpp_and_link_and_preserve_options(self):
        result, report, _ = self.invoke("--sanitize")
        self.assertEqual(result, 0)
        for record in report["commands"]:
            if record["kind"] in ("compile-c", "compile-c++", "link"):
                self.assertIn("-fsanitize=address,undefined", record["command"])
                self.assertIn("-fno-omit-frame-pointer", record["command"])
        env = report["build_environment"]
        self.assertEqual(env["ASAN_OPTIONS"], "symbolize=0:detect_leaks=0:halt_on_error=1:abort_on_error=1")
        self.assertEqual(env["UBSAN_OPTIONS"], "report_error_type=1:halt_on_error=1:print_stacktrace=1")
        self.assertFalse(report["leak_detection"])

    def test_changed_oracle_after_compile_blocks_execution(self):
        oracle = self.root / runner.VALUE_INVERT_ORACLE
        self.after_compile = lambda: oracle.write_bytes(oracle.read_bytes() + b"/* changed */\n")
        result, report, _ = self.invoke()
        self.assertEqual(result, 1)
        self.assertFalse(report["all_passed"])
        self.assertIn(runner.VALUE_INVERT_ORACLE, report["changed_after_compile"])
        self.assertNotIn("test", [record["kind"] for record in report["commands"]])

    def test_changed_value_invert_fixture_after_run_rejects_result(self):
        fixture = next((self.root / runner.MANIFEST).parent.glob("*-plug-in-vinvert-0.raw"))
        self.after_test = lambda: fixture.write_bytes(fixture.read_bytes() + b"changed")
        result, report, _ = self.invoke()
        self.assertEqual(result, 1)
        self.assertFalse(report["all_passed"])
        self.assertIn(str(fixture.relative_to(self.root)), report["changed_after_run"])

    def test_c_compile_failure_prevents_cpp_link_and_test(self):
        self.fail_c_compile = True
        result, report, _ = self.invoke()
        self.assertEqual(result, 1)
        self.assertFalse(report["all_passed"])
        kinds = [record["kind"] for record in report["commands"]]
        self.assertIn("compile-c", kinds)
        self.assertTrue(set(kinds).isdisjoint(("compile-c++", "link", "test")))

    def test_changed_c_oracle_object_after_run_rejects_result(self):
        def change_object():
            obj = next((self.root / "mock-build").rglob("value-invert-legacy-oracle.c.o"))
            obj.write_bytes(obj.read_bytes() + b"changed object")
        self.after_test = change_object
        result, report, _ = self.invoke()
        self.assertEqual(result, 1)
        self.assertFalse(report["all_passed"])
        self.assertFalse(report["objects_unchanged"])
        self.assertTrue(report["executable_unchanged"])

    def test_removed_cpp_object_after_run_rejects_result(self):
        self.after_test = lambda: next((self.root / "mock-build").rglob("filter-native-kernels.cpp.o")).unlink()
        result, report, _ = self.invoke()
        self.assertEqual(result, 1)
        self.assertFalse(report["all_passed"])
        self.assertFalse(report["objects_unchanged"])
        self.assertIsNone(report["object_sha256_after_run"]["filter-native-kernels.cpp.o"])

    def test_changed_executable_after_run_rejects_result(self):
        def change_executable():
            executable = next((self.root / "mock-build").rglob("painter-filter-point-shadow"))
            executable.write_bytes(executable.read_bytes() + b"changed executable")
        self.after_test = change_executable
        result, report, _ = self.invoke()
        self.assertEqual(result, 1)
        self.assertFalse(report["all_passed"])
        self.assertFalse(report["executable_unchanged"])
        self.assertTrue(report["objects_unchanged"])

    def test_inputs_changed_during_verification_are_rejected(self):
        original_verify = runner.verify_fixtures
        oracle = self.root / runner.VALUE_INVERT_ORACLE

        def changed_verify():
            result = original_verify()
            oracle.write_bytes(oracle.read_bytes() + b"/* changed during verification */\n")
            return result

        with patch.object(runner, "verify_fixtures", side_effect=changed_verify):
            with self.assertRaisesRegex(ValueError, "Inputs changed during seal verification"):
                self.invoke("--verify-only")
        self.subprocesses.assert_not_called()

    def test_existing_report_is_never_overwritten(self):
        report = self.root / "control-only-report.json"
        report.write_text("existing evidence\n")
        with self.assertRaises(FileExistsError):
            self.invoke()
        self.assertEqual(report.read_text(), "existing evidence\n")
        self.subprocesses.assert_not_called()


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Source-only Python controls for the strict Edge shadow evidence runner.

Every compiler, pkg-config, linker and native-test subprocess is mocked. Fake
object/executable bytes exercise reporting and seals only, never native Edge
correctness. The only real subprocesses read local hash-pinned git objects.
Run directly with Python; no compiler/build lane is needed or consumed.
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

RUNNER_PATH = Path(__file__).with_name("run_filter_edge_shadow_tests.py")
runner = types.ModuleType("edge_shadow_runner")
runner.__file__ = str(RUNNER_PATH)
exec(compile(RUNNER_PATH.read_bytes(), str(RUNNER_PATH), "exec"), runner.__dict__)
ORIGINAL_ROOT = runner.ROOT


class RunnerSourceControls(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.pinned = {f"{runner.LEGACY_COMMIT}:{name}": subprocess.check_output(
            ["git", "show", f"{runner.LEGACY_COMMIT}:{name}"], cwd=ORIGINAL_ROOT,
            env=dict(os.environ, GIT_NO_LAZY_FETCH="1")) for name in runner.LEGACY_SOURCES}
        cls.pinned.update({f"{runner.PUBLIC_BASE_COMMIT}:{name}": subprocess.check_output(
            ["git", "show", f"{runner.PUBLIC_BASE_COMMIT}:{name}"], cwd=ORIGINAL_ROOT,
            env=dict(os.environ, GIT_NO_LAZY_FETCH="1")) for name in runner.PRESERVED_SOURCES})
        cls.files = runner.source_paths() + runner.fixture_input_paths()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="edge-shadow-python-controls-")
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
        self.native = self.stack.enter_context(patch.object(runner.subprocess, "run", side_effect=self.fake_run))
        self.stack.enter_context(patch.dict(os.environ, {
            "CC": "mock-cc --c-driver", "CXX": "mock-cxx --cxx-driver",
            "CPPFLAGS": "-DCOMMON_CONTROL=1", "CFLAGS": "-DC_ONLY_CONTROL=1",
            "CXXFLAGS": "-DCXX_ONLY_CONTROL=1", "LDFLAGS": "-Wl,--mock-link-control",
            "ASAN_OPTIONS": "symbolize=0", "UBSAN_OPTIONS": "report_error_type=1"}))
        self.commands = []
        self.hook = None
        self.fail_kind = None
        self.failure = "nonzero"
        self.test_stdout = runner.SUCCESS_MARKER + "\n"
        self.omit_artifact = False

    def git_show(self, command, **kwargs):
        self.assertEqual(command[:2], ["git", "show"])
        self.assertEqual(kwargs["cwd"], self.root)
        self.assertEqual(kwargs["env"]["GIT_NO_LAZY_FETCH"], "1")
        return self.pinned[command[2]]

    def command_kind(self, command):
        if "--version" in command:
            return "c-compiler-version" if command[0] == "mock-cc" else "c++-compiler-version"
        if command[0] == "pkg-config":
            return "pkg-cflags" if "--cflags" in command else "pkg-libs"
        if "-c" in command:
            return "compile-c" if command[0] == "mock-cc" else "compile-c++"
        if "-o" in command:
            return "link"
        return "test"

    def fake_run(self, command, **kwargs):
        """Do not execute command, even when it names an apparent executable."""
        kind = self.command_kind(command)
        self.commands.append((kind, command, kwargs))
        if kind == self.fail_kind:
            if self.failure == "timeout":
                raise subprocess.TimeoutExpired(command, 180, output=b"partial stdout", stderr=b"partial stderr")
            if self.failure == "launch-error":
                raise FileNotFoundError("mock missing executable")
            if self.failure == "stderr":
                return subprocess.CompletedProcess(command, 0, "", "mock diagnostic\n")
            return subprocess.CompletedProcess(command, 9, "", "mock failure\n")
        if "-o" in command and not self.omit_artifact:
            Path(command[command.index("-o") + 1]).write_bytes(b"mock artifact, not native executable\n")
        if self.hook:
            self.hook(kind, command)
        stdout = self.test_stdout if kind == "test" else ""
        return subprocess.CompletedProcess(command, 0, stdout, "")

    def invoke(self, *options, report_name="control-only-report.json"):
        report_path = self.root / report_name
        build = self.root / "mock-build"
        with patch.object(sys, "argv", [str(RUNNER_PATH), str(build), "--report", str(report_path), *options]), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            result = runner.main()
        return result, json.loads(report_path.read_text()), build

    def mutate_once(self, kind, action):
        def hook(current, command):
            if current == kind:
                self.hook = None
                action()
        self.hook = hook

    def artifact(self, name):
        return next((self.root / "mock-build").rglob(name))

    def assert_failed(self, report):
        self.assertIs(report["all_passed"], False)
        self.assertFalse(report["source_only_verified"])

    def test_literal_blocks_and_complete_archived_subsets(self):
        oracle = runner.verify_oracle()
        self.assertEqual(set(oracle["verified_blocks"]), runner.EXPECTED_BLOCKS)
        self.assertTrue(oracle["literal_blocks_match_pinned_source"])
        paths, provenance = runner.verify_fixtures()
        self.assertEqual(paths, runner.fixture_input_paths())
        self.assertEqual(provenance["rgba_edge"]["case_count"], 76)
        self.assertEqual(provenance["gray_edge"]["case_count"], 45)
        self.assertIn(self.root / runner.GRAY_DIR / "input-noalpha.y", paths)
        self.assertIn(self.root / runner.GRAY_DIR / "edge-noalpha-m5-w2-a2.y", paths)
        self.assertFalse(any(path.name.startswith("gauss-") for path in paths))
        self.native.assert_not_called()

    def test_existing_contract_files_are_identical_to_pinned_public_base(self):
        result = runner.verify_preserved_sources()
        self.assertTrue(result["whole_files_byte_identical"])
        self.assertEqual(result["source_sha256"], runner.PRESERVED_SOURCES)
        for name in runner.PRESERVED_SOURCES:
            path = self.root / name
            original = path.read_bytes()
            with self.subTest(path=name):
                path.write_bytes(original + b"/* altered existing contract */")
                with self.assertRaisesRegex(ValueError, "Existing complete-output contract source changed"):
                    runner.verify_preserved_sources()
                path.write_bytes(original)
        self.native.assert_not_called()

    def test_missing_existing_contract_verification_cannot_pass(self):
        with patch.object(runner, "verify_preserved_sources", return_value={}):
            code, report, build = self.invoke("--verify-only")
        self.assertEqual(code, 1)
        self.assert_failed(report)
        self.assertFalse(report["contract_preservation_verified"])
        self.assertFalse(build.exists())
        self.native.assert_not_called()

    def test_each_literal_oracle_block_rewrite_is_rejected(self):
        path = self.root / runner.ORACLE
        original = path.read_bytes()
        for name in runner.EXPECTED_BLOCKS:
            with self.subTest(block=name):
                block = runner.marked(original, name)
                path.write_bytes(original.replace(block, block + b"\n/* altered literal block */", 1))
                with self.assertRaisesRegex(ValueError, "not the literal pinned source"):
                    runner.verify_oracle()
                path.write_bytes(original)
        self.native.assert_not_called()

    def test_missing_duplicate_or_reversed_exact_markers_are_rejected(self):
        path = self.root / runner.ORACLE
        original = path.read_bytes()
        begin = b"/* BEGIN EXACT LEGACY EDGE */\n"
        end = b"\n/* END EXACT LEGACY EDGE */"
        for corrupt in (original.replace(begin, b"", 1), original + begin,
                        original.replace(begin, b"SWAP_MARKER", 1).replace(end, begin, 1).replace(b"SWAP_MARKER", end, 1)):
            with self.subTest(corrupt=corrupt[:25]):
                path.write_bytes(corrupt)
                with self.assertRaises(ValueError):
                    runner.verify_oracle()
        path.write_bytes(original)
        self.native.assert_not_called()

    def test_oracle_scaffold_and_header_rewrites_are_rejected(self):
        for name in (runner.ORACLE, runner.ORACLE_HEADER):
            path = self.root / name
            original = path.read_bytes()
            with self.subTest(path=name):
                path.write_bytes(original + b"\n/* changed adapter */\n")
                with self.assertRaisesRegex(ValueError, "adapter/header seal differs"):
                    runner.verify_oracle()
                path.write_bytes(original)
        self.native.assert_not_called()

    def test_pinned_source_tampering_is_rejected(self):
        for name in runner.LEGACY_SOURCES:
            key = f"{runner.LEGACY_COMMIT}:{name}"
            original = self.pinned[key]
            with self.subTest(source=name), patch.dict(self.pinned, {key: original + b"changed"}):
                with self.assertRaisesRegex(ValueError, "Pinned legacy source hash differs"):
                    runner.verify_oracle()
        self.native.assert_not_called()

    def test_each_archived_edge_output_hash_is_verified(self):
        count = 0
        for directory, gray in ((runner.EDGE_DIR, False), (runner.GRAY_DIR, True)):
            base = self.root / directory
            for row in runner.rows(base, gray):
                path = base / row[-1]
                original = path.read_bytes()
                with self.subTest(output=path.name):
                    path.write_bytes(bytes([original[0] ^ 1]) + original[1:])
                    with self.assertRaisesRegex(ValueError, "Archived original input/output bytes differ"):
                        runner.verify_fixtures()
                    path.write_bytes(original)
                count += 1
        self.assertEqual(count, 121)
        self.native.assert_not_called()

    def test_archived_inputs_and_png_outputs_are_verified(self):
        for directory, gray in ((runner.EDGE_DIR, False), (runner.GRAY_DIR, True)):
            base = self.root / directory
            row = runner.rows(base, gray)[0]
            for name in (row[-2], Path(row[-1]).stem + ".png", row[-2].replace("input-", "loaded-", 1)):
                path = base / name
                original = path.read_bytes()
                with self.subTest(path=path):
                    path.write_bytes(original + b"changed")
                    with self.assertRaises(ValueError):
                        runner.verify_fixtures()
                    path.write_bytes(original)
        self.native.assert_not_called()

    def test_archived_manifests_and_reports_are_pinned(self):
        for name in runner.ARCHIVE_SEALS:
            path = self.root / name
            original = path.read_bytes()
            with self.subTest(path=name):
                path.write_bytes(original + b" ")
                with self.assertRaisesRegex(ValueError, "provenance/manifest seal differs"):
                    runner.verify_fixtures()
                path.write_bytes(original)
        self.native.assert_not_called()

    def test_fixture_paths_cannot_escape_archive(self):
        for name in ("../escaped", "/tmp/escaped", "nested/escaped"):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, "escapes archive"):
                runner.local_fixture(self.root / runner.EDGE_DIR, name)

    def test_verify_only_has_no_build_native_commands_or_execution_claim(self):
        result, report, build = self.invoke("--verify-only", "--sanitize")
        self.assertEqual(result, 0)
        self.assertIsNone(report["all_passed"])
        self.assertTrue(report["source_only_verified"])
        self.assertTrue(report["exact_oracle_verified"])
        self.assertEqual(report["verification_mode"], "source-only")
        self.assertEqual(report["commands"], [])
        self.assertEqual(report["sanitizers"], [])
        self.assertIsNone(report["leak_detection"])
        self.assertNotIn("output_directory", report)
        self.assertFalse(build.exists())
        self.native.assert_not_called()

    def test_missing_exact_verification_cannot_claim_pass(self):
        actual = runner.verify_oracle()
        mutations = [dict(actual, literal_blocks_match_pinned_source=False),
                     dict(actual, verified_blocks={}), dict(actual, adapter_sha256={})]
        for i, result in enumerate(mutations):
            with self.subTest(mutation=i), patch.object(runner, "verify_oracle", return_value=result):
                code, report, build = self.invoke("--verify-only", report_name=f"missing-exact-{i}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertIn("Missing exact oracle verification", report["error"])
                self.assertFalse(build.exists())
        self.native.assert_not_called()

    def test_separate_c11_and_cpp14_build_honors_environment(self):
        code, report, _ = self.invoke()
        self.assertEqual(code, 0)
        self.assertTrue(report["all_passed"])
        self.assertFalse(report["source_only_verified"])
        records = report["commands"]
        c = [r["command"] for r in records if r["kind"] == "compile-c"]
        cpp = [r["command"] for r in records if r["kind"] == "compile-c++"]
        self.assertEqual(len(c), 1)
        self.assertEqual(len(cpp), len(runner.CPP_SOURCES))
        self.assertEqual(c[0][:2], ["mock-cc", "--c-driver"])
        self.assertEqual(c[0][c[0].index("-x") + 1], "c")
        self.assertIn("-std=c11", c[0])
        self.assertIn("-DC_ONLY_CONTROL=1", c[0])
        self.assertNotIn("-DCXX_ONLY_CONTROL=1", c[0])
        for command in c + cpp:
            self.assertIn("-DCOMMON_CONTROL=1", command)
            self.assertIn("-UNDEBUG", command)
        for command in cpp:
            self.assertEqual(command[:2], ["mock-cxx", "--cxx-driver"])
            self.assertIn("-std=c++14", command)
            self.assertIn("-fno-rtti", command)
            self.assertIn("-fexceptions", command)
            self.assertIn("-DCXX_ONLY_CONTROL=1", command)
            self.assertNotIn("-DC_ONLY_CONTROL=1", command)
        self.assertTrue(any("test-filter-edge-shadow-header.cpp" in " ".join(c) for c in cpp))
        link = next(r["command"] for r in records if r["kind"] == "link")
        self.assertIn(c[0][-1], link)
        self.assertIn("-Wl,--mock-link-control", link)
        self.assertIn("-lm", link)
        self.assertTrue(all(r["classification"] == "clean-zero" for r in records))
        self.assertEqual(records[-1]["command"][1:], [str(self.root / runner.EDGE_DIR), str(self.root / runner.GRAY_DIR)])
        self.assertTrue(report["exact_success_marker_verified"])
        self.assertEqual(report["object_sha256"], report["object_sha256_after_run"])
        self.assertEqual(report["executable_sha256"], report["executable_sha256_after_run"])
        self.assertEqual(report["changed_after_verify"], [])
        self.assertEqual(report["changed_after_compile"], [])
        self.assertEqual(report["changed_before_run"], [])
        self.assertEqual(report["changed_after_run"], [])
        self.assertEqual(report["input_sha256_after_verify"], report["input_sha256_after_run"])
        self.assertTrue(report["contract_preservation_verified"])

    def test_sanitizers_cover_c_cpp_link_and_exclude_leaks(self):
        code, report, _ = self.invoke("--sanitize")
        self.assertEqual(code, 0)
        self.assertEqual(report["sanitizers"], ["address", "undefined", "float-cast-overflow"])
        for record in report["commands"]:
            if record["kind"] in ("compile-c", "compile-c++", "link"):
                self.assertIn("-fsanitize=address,undefined,float-cast-overflow", record["command"])
                self.assertIn("-fno-sanitize=leak", record["command"])
                self.assertIn("-fno-omit-frame-pointer", record["command"])
            if record["kind"] == "compile-c++":
                self.assertIn("-frtti", record["command"])
        env = report["build_environment"]
        self.assertEqual(env["ASAN_OPTIONS"], "symbolize=0:detect_leaks=0:halt_on_error=1:abort_on_error=1")
        self.assertEqual(env["UBSAN_OPTIONS"], "report_error_type=1:halt_on_error=1:print_stacktrace=1")
        self.assertFalse(report["leak_detection"])

    def test_every_command_stage_failure_stops_and_preserves_evidence(self):
        stages = ["c-compiler-version", "c++-compiler-version", "pkg-cflags", "pkg-libs",
                  "compile-c", "compile-c++", "link", "test"]
        for i, stage in enumerate(stages):
            with self.subTest(stage=stage):
                self.fail_kind = stage
                code, report, _ = self.invoke(report_name=f"failed-stage-{i}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertEqual(report["commands"][-1]["kind"], stage)
                self.assertEqual(report["commands"][-1]["exit_code"], 9)
                self.assertEqual(report["commands"][-1]["classification"], "nonzero-exit")

    def test_zero_exit_with_stderr_never_passes_any_stage(self):
        for i, stage in enumerate(("c-compiler-version", "pkg-cflags", "compile-c", "compile-c++", "link", "test")):
            with self.subTest(stage=stage):
                self.fail_kind, self.failure = stage, "stderr"
                code, report, _ = self.invoke(report_name=f"stderr-stage-{i}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertEqual(report["commands"][-1]["classification"], "zero-exit-with-stderr")
                self.assertEqual(report["commands"][-1]["exit_code"], 0)

    def test_timeouts_and_launch_errors_preserve_failed_classification(self):
        for failure in ("timeout", "launch-error"):
            with self.subTest(failure=failure):
                self.fail_kind, self.failure = "compile-c", failure
                code, report, _ = self.invoke(report_name=f"{failure}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                record = report["commands"][-1]
                self.assertEqual(record["classification"], failure)
                self.assertIsNone(record["exit_code"])
                if failure == "timeout":
                    self.assertEqual(record["stdout"], "partial stdout")
                    self.assertEqual(record["stderr"], "partial stderr")

    def test_missing_or_duplicate_success_marker_is_not_native_success(self):
        for i, output in enumerate(("", "all checks passed\n", runner.SUCCESS_MARKER + " suffix\n",
                                    (runner.SUCCESS_MARKER + "\n") * 2)):
            with self.subTest(output=output):
                self.test_stdout = output
                code, report, _ = self.invoke(report_name=f"bad-marker-{i}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertFalse(report["exact_success_marker_verified"])
                self.assertEqual(report["commands"][-1]["classification"], "clean-zero")

    def test_missing_compiler_output_is_failure(self):
        self.omit_artifact = True
        code, report, _ = self.invoke()
        self.assertEqual(code, 1)
        self.assert_failed(report)
        self.assertIn("error", report)
        self.assertNotIn("test", [r["kind"] for r in report["commands"]])

    def test_changed_or_deleted_sources_after_compile_block_execution(self):
        source = self.root / runner.ORACLE_HEADER
        original = source.read_bytes()
        for i, remove in enumerate((False, True)):
            with self.subTest(remove=remove):
                self.mutate_once("compile-c", source.unlink if remove else lambda: source.write_bytes(original + b"changed"))
                code, report, _ = self.invoke(report_name=f"source-{i}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertIn(runner.ORACLE_HEADER, report["changed_after_compile"])
                self.assertNotIn("test", [r["kind"] for r in report["commands"]])
                source.write_bytes(original)

    def test_changed_fixture_after_link_blocks_execution(self):
        source = self.root / runner.GRAY_DIR / "input-noalpha.y"
        self.mutate_once("link", lambda: source.write_bytes(source.read_bytes() + b"changed"))
        code, report, _ = self.invoke()
        self.assertEqual(code, 1)
        self.assertIn(str(source.relative_to(self.root)), report["changed_before_run"])
        self.assertEqual(report["stage"], "pre-run-seal-failed")
        self.assertNotIn("test", [r["kind"] for r in report["commands"]])

    def test_changed_or_deleted_object_during_compile_blocks_link(self):
        for i, remove in enumerate((False, True)):
            with self.subTest(remove=remove):
                def change():
                    obj = max((self.root / "mock-build").rglob("legacy-edge-shadow-oracle.c.o"), key=lambda p: p.stat().st_mtime_ns)
                    obj.unlink() if remove else obj.write_bytes(b"changed object before link")
                self.mutate_once("compile-c++", change)
                code, report, _ = self.invoke(report_name=f"pre-link-object-{i}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertEqual(report["stage"], "compile-seal-failed")
                self.assertNotIn("link", [r["kind"] for r in report["commands"]])
                self.assertNotIn("test", [r["kind"] for r in report["commands"]])

    def test_changed_or_deleted_object_during_link_blocks_execution(self):
        for i, remove in enumerate((False, True)):
            with self.subTest(remove=remove):
                def change():
                    # The newest private directory is the active build.
                    obj = max((self.root / "mock-build").rglob("legacy-edge-shadow-oracle.c.o"), key=lambda p: p.stat().st_mtime_ns)
                    obj.unlink() if remove else obj.write_bytes(b"changed object")
                self.mutate_once("link", change)
                code, report, _ = self.invoke(report_name=f"pre-run-object-{i}.json")
                self.assertEqual(code, 1)
                self.assertEqual(report["stage"], "pre-run-seal-failed")
                self.assertNotIn("test", [r["kind"] for r in report["commands"]])

    def test_source_and_fixture_changes_or_deletions_after_run_reject_result(self):
        paths = [self.root / runner.ORACLE, self.root / runner.EDGE_DIR / "input-5x4.rgba",
                 self.root / runner.GRAY_DIR / "edge-noalpha-m0-w2-a2.y"]
        for i, path in enumerate(paths):
            original = path.read_bytes()
            for remove in (False, True):
                with self.subTest(path=path, remove=remove):
                    self.mutate_once("test", path.unlink if remove else lambda: path.write_bytes(original + b"changed"))
                    code, report, _ = self.invoke(report_name=f"post-run-{i}-{remove}.json")
                    self.assertEqual(code, 1)
                    self.assert_failed(report)
                    self.assertIn(str(path.relative_to(self.root)), report["changed_after_run"])
                    path.write_bytes(original)

    def test_changed_and_deleted_objects_after_run_reject_result(self):
        for name in ("legacy-edge-shadow-oracle.c.o", "filter-edge-shadow.cpp.o"):
            for remove in (False, True):
                with self.subTest(name=name, remove=remove):
                    def change():
                        obj = max((self.root / "mock-build").rglob(name), key=lambda p: p.stat().st_mtime_ns)
                        obj.unlink() if remove else obj.write_bytes(b"changed object")
                    self.mutate_once("test", change)
                    code, report, _ = self.invoke(report_name=f"object-{name}-{remove}.json")
                    self.assertEqual(code, 1)
                    self.assert_failed(report)
                    self.assertFalse(report["objects_unchanged"])
                    self.assertTrue(report["executable_unchanged"])
                    if remove:
                        self.assertIsNone(report["object_sha256_after_run"][name])

    def test_changed_and_deleted_executable_after_run_reject_result(self):
        for remove in (False, True):
            with self.subTest(remove=remove):
                def change():
                    exe = max((self.root / "mock-build").rglob("painter-filter-edge-shadow"), key=lambda p: p.stat().st_mtime_ns)
                    exe.unlink() if remove else exe.write_bytes(b"changed executable")
                self.mutate_once("test", change)
                code, report, _ = self.invoke(report_name=f"executable-{remove}.json")
                self.assertEqual(code, 1)
                self.assert_failed(report)
                self.assertFalse(report["executable_unchanged"])
                self.assertTrue(report["objects_unchanged"])
                if remove:
                    self.assertIsNone(report["executable_sha256_after_run"])

    def test_inputs_changed_during_verification_are_rejected(self):
        original = runner.verify_fixtures
        path = self.root / runner.ORACLE_HEADER
        def verify_then_change():
            result = original()
            path.write_bytes(path.read_bytes() + b"changed")
            return result
        with patch.object(runner, "verify_fixtures", side_effect=verify_then_change):
            code, report, _ = self.invoke("--verify-only")
        self.assertEqual(code, 1)
        self.assert_failed(report)
        self.assertIn(runner.ORACLE_HEADER, report["changed_after_verify"])
        self.native.assert_not_called()

    def test_existing_report_is_never_overwritten_even_by_verification(self):
        report = self.root / "control-only-report.json"
        report.write_text("existing evidence\n")
        for options in ((), ("--verify-only",)):
            with self.subTest(options=options), self.assertRaises(FileExistsError):
                self.invoke(*options)
        self.assertEqual(report.read_text(), "existing evidence\n")
        self.native.assert_not_called()
        self.git.assert_not_called()

    def test_repeated_builds_use_distinct_private_output_directories(self):
        code1, first, _ = self.invoke(report_name="first.json")
        code2, second, _ = self.invoke(report_name="second.json")
        self.assertEqual((code1, code2), (0, 0))
        self.assertNotEqual(first["output_directory"], second["output_directory"])
        for report in (first, second):
            self.assertEqual(Path(report["output_directory"]).stat().st_mode & 0o777, 0o700)
            self.assertEqual((Path(report["output_directory"]) / "tmp").stat().st_mode & 0o777, 0o700)
        self.assertEqual((self.root / "first.json").stat().st_mode & 0o777, 0o600)


if __name__ == "__main__":
    unittest.main()

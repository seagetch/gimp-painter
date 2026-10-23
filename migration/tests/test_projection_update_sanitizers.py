#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Pure-Python controls for the focused runner; never build or run native code."""

import fcntl
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.dont_write_bytecode = True
SOURCE = Path(__file__).with_name("run_projection_update_sanitizers.py")
SPEC = importlib.util.spec_from_file_location("projection_update_runner", SOURCE)
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)
PATH = "/update-geometry/translation-bounds"
POSITIVE = "TAP version 14\n# seed\n1..1\nok 1 " + PATH + "\n"


class RunnerControls(unittest.TestCase):
    def test_exact_positive_tap(self):
        self.assertTrue(runner.exact_tap(POSITIVE, PATH))
        self.assertTrue(runner.exact_tap(POSITIVE.replace("1..1\n", "1..1  \n"), PATH))

    def test_skipped_todo_plans_and_rows(self):
        for directive in ("SKIP", "TODO", "skip", "todo"):
            for text in (POSITIVE.replace("1..1", "1..1 # " + directive),
                         POSITIVE.replace(PATH, PATH + " # " + directive)):
                with self.subTest(text=text):
                    self.assertFalse(runner.exact_tap(text, PATH))

    def test_empty_duplicate_prefix_and_bailout(self):
        for text in ("", "1..0\n", "1..1\n", POSITIVE + "1..1\n",
                     POSITIVE + "ok 2 " + PATH + "\n",
                     POSITIVE + "Bail out! unexpected failure\n",
                     POSITIVE.replace(PATH, PATH + "-different"),
                     POSITIVE.replace("ok 1", "not ok 1")):
            with self.subTest(text=text):
                self.assertFalse(runner.exact_tap(text, PATH))

    def test_runtime_diagnostics_reject_zero_exit(self):
        clean = {"status": "passed", "exit_code": 0, "stdout": POSITIVE, "stderr": ""}
        self.assertTrue(runner.test_outcome(clean, PATH)["passed"])
        for message in ("ERROR: AddressSanitizer: use-after-free",
                        "runtime error: signed integer overflow", "GLib-CRITICAL failure",
                        "Gimp-WARNING: bad state", "Segmentation fault", "SIGABRT"):
            for destination in ("stdout", "stderr"):
                result = dict(clean)
                result[destination] += message + "\n"
                with self.subTest(message=message, destination=destination):
                    self.assertFalse(runner.test_outcome(result, PATH)["passed"])
        for status, code in (("failed", 0), ("passed", 7), ("timeout", -9)):
            self.assertFalse(runner.test_outcome(dict(clean, status=status, exit_code=code), PATH)["passed"])

    def test_inherited_exclusive_lock(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "lock"
            other = Path(directory) / "other"
            other.touch()
            with path.open("w+") as lock:
                with self.assertRaises(RuntimeError):
                    runner.verify_lock(lock.fileno(), path)
                fcntl.flock(lock, fcntl.LOCK_SH)
                with self.assertRaises(RuntimeError):
                    runner.verify_lock(lock.fileno(), path)
                fcntl.flock(lock, fcntl.LOCK_EX)
                self.assertTrue(runner.verify_lock(lock.fileno(), path)["exclusive_caller_lock_verified"])
                with self.assertRaises(RuntimeError):
                    runner.verify_lock(lock.fileno(), other)

    def test_private_compile_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            ordinary = base / "ordinary.o"
            old_dep = base / "ordinary.d"
            ordinary.write_bytes(b"ordinary object sentinel")
            old_dep.write_bytes(b"ordinary dependency sentinel")
            entry = {"directory": str(base), "file": "unit.c",
                     "arguments": ["cc", "-DKEEP=1", "-MMD", "-MP", "-MF", str(old_dep),
                                   "-MQold", "-MT", str(ordinary), "-c", "unit.c", "-o", str(ordinary)]}
            obj, dep = base / "private.o", base / "private.d"
            argv = runner.compile_command(entry, obj, dep)
            self.assertEqual(runner.object_for(entry), ordinary)
            self.assertEqual(argv[argv.index("-o") + 1], str(obj))
            self.assertEqual(argv[argv.index("-MF") + 1], str(dep))
            self.assertEqual(argv[argv.index("-MT") + 1], str(obj))
            self.assertEqual(argv.count("-MD"), 1)
            self.assertNotIn(str(old_dep), argv)
            self.assertNotIn(str(ordinary), argv)
            self.assertNotIn("-MQold", argv)
            self.assertIn("-DKEEP=1", argv)
            self.assertEqual(argv[-len(runner.FLAGS):], runner.FLAGS)
            self.assertEqual(ordinary.read_bytes(), b"ordinary object sentinel")
            self.assertEqual(old_dep.read_bytes(), b"ordinary dependency sentinel")
            self.assertFalse(obj.exists())
            self.assertFalse(dep.exists())

    def test_caller_options_preserved(self):
        original = 'detect_leaks=1:symbolizer_path="/path with spaces/symbolizer":handle_segv=0'
        required = "detect_leaks=0:halt_on_error=1"
        self.assertEqual(runner.merge_options(original, required), original + ":" + required)
        self.assertEqual(runner.merge_options("", required), required)

    def make_run(self, directory, timeout=10):
        base = Path(directory)
        return runner.Run(SimpleNamespace(build=base, report=base / "report.json",
                                           total_timeout=30, command_timeout=timeout))

    def test_failure_receipt_survives_failed_command(self):
        with tempfile.TemporaryDirectory() as directory:
            run = self.make_run(directory)
            with self.assertRaises(RuntimeError):
                run.command([sys.executable, "-c", "import sys; print('deliberate failure'); sys.exit(7)"],
                            "pure-python failure control", Path(directory))
            run.report.update(status="failed", error="expected control failure")
            run.finish()
            report = json.loads(run.args.report.read_text())
            self.assertEqual(report["status"], "failed")
            self.assertEqual(report["commands"][0]["exit_code"], 7)
            self.assertIn("deliberate failure", report["commands"][0]["stdout"])
            self.assertTrue(all(test["status"] == "not_completed" and not test["passed"]
                                for test in report["tests"]))

    def test_timeout_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            run = self.make_run(directory, timeout=0.05)
            with self.assertRaises(RuntimeError):
                run.command([sys.executable, "-c", "import time; time.sleep(10)"],
                            "pure-python timeout control", Path(directory))
            self.assertEqual(json.loads(run.args.report.read_text())["commands"][0]["status"], "timeout")

    def test_changed_source_fails_seal(self):
        with tempfile.TemporaryDirectory() as directory:
            run = self.make_run(directory)
            source = Path(directory) / "source.c"
            source.write_text("before")
            run.inputs = runner.seals({source})
            source.write_text("after")
            run.report["status"] = "passed"
            run.finish()
            self.assertEqual(run.report["status"], "failed")
            self.assertEqual(run.report["source_seals"]["changed"], [str(source)])


if __name__ == "__main__":
    unittest.main(verbosity=2)

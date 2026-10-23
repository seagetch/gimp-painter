#!/usr/bin/env python3
"""Check original 04.010 against existing native Meson compiler commands.

No build regeneration or GIMP execution occurs. Each production app C++ command
is replayed against a tiny standalone throw/catch/RAII program in a temporary
directory. Native GNU-style drivers only: other platforms need their own run.
The report also identifies upstream .cc units and test-only target exclusions.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
CPP_SUFFIXES = {".cpp", ".cc", ".cp", ".cxx", ".c++", ".CPP", ".C"}
PROBE = """#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS) && !defined(_CPPUNWIND)
#error Painter_requires_CXX_exceptions
#endif
struct Guard {
  int *count;
  ~Guard() noexcept { ++*count; }
};
static void inner(int *count) { Guard guard{count}; throw 47; }
int main() {
  int count = 0;
  try { inner(&count); }
  catch (int value) { return value == 47 && count == 1 ? 0 : 2; }
  catch (...) { return 3; }
  return 4;
}
"""


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def arguments(entry):
    return list(entry["arguments"]) if "arguments" in entry else shlex.split(entry["command"])


def source_path(entry):
    return (Path(entry["directory"]) / entry["file"]).resolve()


def replay_arguments(entry):
    """Remove only source/output/dependency actions, preserving flag order."""
    result = []
    skip = False
    for arg in arguments(entry):
        if skip:
            skip = False
        elif arg in ("-o", "-MF", "-MQ", "-MT"):
            skip = True
        elif arg in ("-c", "-MD", "-MMD", "-MP"):
            continue
        elif arg.startswith(("-MF", "-MQ", "-MT")):
            continue
        elif arg == entry["file"]:
            continue
        else:
            result.append(arg)
    if skip or not result:
        raise ValueError("Incomplete compiler command")
    return result


def exception_flags(args):
    return [a for a in args if a in ("-fexceptions", "-fno-exceptions") or a.startswith("/EH")]


def explicitly_enabled(args):
    flags = exception_flags(args)
    return bool(flags and flags[-1] in ("-fexceptions", "/EHsc", "/EHs", "/EHa"))


def target_for(entry, targets):
    output = (Path(entry["directory"]) / entry["output"]).resolve()
    matches = []
    for target in targets:
        for filename in target["filename"]:
            objdir = Path(str(Path(filename).resolve()) + ".p")
            if output.is_relative_to(objdir):
                matches.append(target)
    if len(matches) != 1:
        raise ValueError(f"Expected one Meson target for {entry['output']}; found {len(matches)}")
    return matches[0]


def production_target(target):
    defined = Path(target["defined_in"])
    if "tests" in defined.parts:
        return False
    return target["type"] == "static library" or bool(target.get("installed"))


def run(command, cwd):
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=90)
    # Compiler diagnostics only; never capture or dump the process environment.
    return {"returncode": result.returncode,
            "diagnostic": (result.stderr + result.stdout)[-1600:]}


def check(args):
    metadata = args.metadata_dir.resolve()
    database = metadata / "compile_commands.json"
    targets_path = metadata / "meson-info/intro-targets.json"
    compilers_path = metadata / "meson-info/intro-compilers.json"
    targets = json.loads(targets_path.read_text())
    compiler = json.loads(compilers_path.read_text())["host"]["cpp"]
    if compiler["id"] not in ("gcc", "clang", "apple-clang"):
        raise ValueError("This native replay checker supports GNU-style drivers only")
    entries = json.loads(database.read_text())
    report = {
        "task": "original 04.010 compiler policy",
        "recorded_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "status": "RUNNING", "compiler": compiler,
        "inputs": {str(p): digest(p) for p in (database, targets_path, compilers_path)},
        "probe_sha256": hashlib.sha256(PROBE.encode()).hexdigest(),
        "units": [], "excluded_targets": {},
        "limitations": [
            "Native Linux GNU-style compiler replay only; Windows, macOS and cross compilers are unverified.",
            "Standalone programs prove flag handling and C++ unwinding; C ABI boundary behavior is tested separately.",
            "No GIMP executable, GUI, network listener, user profile or source-writing generator is run.",
            "Painter policy scope is current app production .cpp units; upstream .cc units are inventoried separately.",
            "Prepending -fno-exceptions models earlier global flags; it is not a fresh whole-project configure.",
        ],
    }
    excluded = Counter()
    with tempfile.TemporaryDirectory(prefix="painter-exceptions-", dir=args.scratch_dir) as temporary:
        scratch = Path(temporary)
        probe = scratch / "probe.cpp"
        probe.write_text(PROBE)
        executable = scratch / "probe"
        for entry in entries:
            source = source_path(entry)
            if source.suffix not in CPP_SUFFIXES:
                continue
            if not source.is_relative_to(ROOT / "app"):
                # Generated test sources live under their build directory.
                if not source.is_relative_to(Path(entry["directory"]) / "app"):
                    continue
            target = target_for(entry, targets)
            if not production_target(target):
                excluded[target["name"]] += 1
                continue
            command = replay_arguments(entry)
            unit = {
                "source": str(source.relative_to(ROOT)), "source_sha256": digest(source),
                "target": target["name"], "output": entry["output"],
                "command_sha256": hashlib.sha256(json.dumps(arguments(entry)).encode()).hexdigest(),
                "policy_flags": exception_flags(command),
                "rtti_flags": [a for a in command if "rtti" in a or a.startswith("/GR")],
                "requires_explicit_policy": source.suffix == ".cpp",
                "explicitly_enabled": explicitly_enabled(command),
            }
            suffix = [str(probe), "-o", str(executable), "-fdiagnostics-color=never"]
            positive = run(command + suffix, entry["directory"])
            unit["native_compile"] = positive
            if positive["returncode"] == 0:
                unit["native_unwind"] = run([str(executable)], scratch)
            # An earlier global disable must be overridden by target policy.
            override = run(command[:1] + ["-fno-exceptions"] + command[1:] + suffix,
                           entry["directory"])
            unit["earlier_disable_compile"] = override
            if override["returncode"] == 0:
                unit["earlier_disable_unwind"] = run([str(executable)], scratch)
            # A final explicit disable must make this same fixture fail.
            negative = run(command + ["-fno-exceptions"] + suffix, entry["directory"])
            unit["final_disable_compile"] = negative
            report["units"].append(unit)
    report["excluded_targets"] = dict(sorted(excluded.items()))
    units = report["units"]
    painter = [u for u in units if u["requires_explicit_policy"]]
    gaps = [u["source"] for u in painter if not u["explicitly_enabled"]]
    native_failures = [u["source"] for u in units if u.get("native_unwind", {}).get("returncode") != 0]
    control_failures = [u["source"] for u in units
                        if u["final_disable_compile"]["returncode"] == 0
                        or (u["explicitly_enabled"] and u.get("earlier_disable_unwind", {}).get("returncode") != 0)]
    report["summary"] = {
        "production_cpp_commands": len(units), "painter_cpp_commands": len(painter),
        "upstream_cc_commands": len(units) - len(painter),
        "native_unwind_passes": len(units) - len(native_failures),
        "painter_explicit_policy_gaps": gaps, "native_failures": native_failures,
        "negative_control_failures": control_failures,
    }
    report["status"] = "PASS" if units and not (gaps or native_failures or control_failures) else "FAIL"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"status": report["status"], **report["summary"]}, indent=2))
    return 0 if report["status"] == "PASS" else 1


class MetadataTests(unittest.TestCase):
    def test_command_and_arguments_keep_last_flag(self):
        base = {"file": "unit.cpp", "command": "c++ -fno-exceptions -fexceptions -c unit.cpp -o unit.o -MD -MF unit.d"}
        self.assertEqual(replay_arguments(base), ["c++", "-fno-exceptions", "-fexceptions"])
        self.assertTrue(explicitly_enabled(replay_arguments(base)))
        base["arguments"] = ["c++", "-fexceptions", "-fno-exceptions", "-c", "unit.cpp"]
        self.assertFalse(explicitly_enabled(replay_arguments(base)))

    def test_production_is_target_based(self):
        self.assertTrue(production_target({"defined_in": "/repo/app/core/meson.build", "type": "static library"}))
        self.assertFalse(production_target({"defined_in": "/repo/app/painter/meson.build", "type": "executable", "installed": False}))
        self.assertFalse(production_target({"defined_in": "/repo/app/tests/meson.build", "type": "static library"}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--metadata-dir", type=Path, help="Build directory or preserved metadata directory")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--scratch-dir", type=Path, default=Path("/tmp"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(MetadataTests)
        return 0 if unittest.TextTestRunner().run(suite).wasSuccessful() else 1
    if not args.metadata_dir or not args.output:
        parser.error("--metadata-dir and --output are required")
    return check(args)


if __name__ == "__main__":
    sys.exit(main())

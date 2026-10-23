#!/usr/bin/env python3
"""Exercise the exact current exception configure block in isolated Meson projects.

Run after sourcing the documented native build environment. Wrapper compilers
delegate to its real C++ compiler; they only model a rejected exception switch
or an accepted switch that leaves exceptions disabled. No application is built.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which("c++")
    if not compiler:
        raise RuntimeError("No native C++ compiler")
    source = (ROOT / "app/painter/meson.build").read_text()
    start = source.index("painter_exception_switch =")
    end = source.index("\npainter_cpp_args =", start)
    current = source[start:end]
    legacy = "painter_exception_args = cxx.get_supported_arguments(['-fexceptions', '-fno-rtti'])\n"
    wrappers = {}
    for mode in ("reject", "disable"):
        wrapper = work / (mode + "-cxx")
        wrapper.write_text("#!/usr/bin/python3\nimport os, sys\n"
                           + ("if '-fexceptions' in sys.argv:\n    sys.exit(2)\n" if mode == "reject" else "")
                           + f"args = [{compiler!r}] + sys.argv[1:]"
                           + (" + ['-fno-exceptions']\n" if mode == "disable" else "\n")
                           + "os.execv(args[0], args)\n")
        wrapper.chmod(0o755)
        wrappers[mode] = wrapper
    cases = [
        ("native", current, None, [], True, None),
        ("earlier-global-disable", current, None, ["-Dcpp_args=-fno-exceptions"], True, None),
        ("legacy-silently-filtered", legacy, "reject", [], True, None),
        ("required-switch-rejected", current, "reject", [], False, "does not support"),
        ("accepted-switch-disabled", current, "disable", [], False, "Painter requires C++ throw/catch support"),
    ]
    results = []
    for name, policy, wrapper, options, expected_success, expected_error in cases:
        project = work / name
        project.mkdir()
        (project / "meson.build").write_text(
            "project('painter-exception-policy-probe', 'cpp', default_options: ['cpp_std=c++14'])\n"
            "cxx = meson.get_compiler('cpp')\n" + policy + "\n")
        env = dict(os.environ)
        env["CXX"] = str(wrappers[wrapper]) if wrapper else compiler
        command = ["meson", "setup", str(project / "build"), str(project)] + options
        result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=90)
        lines = (result.stdout + result.stderr).splitlines()
        selected = [line for line in lines if any(word in line for word in (
            "supports arguments", "Painter C++ exception support", "ERROR:", "not supported"))]
        valid = ((result.returncode == 0) == expected_success
                 and (expected_error is None or expected_error in result.stdout + result.stderr))
        results.append({"name": name, "returncode": result.returncode,
                        "expected_configure_success": expected_success,
                        "pass": valid, "diagnostics": selected})
        print(name, "PASS" if valid else "FAIL")
    report = {"status": "PASS" if all(r["pass"] for r in results) else "FAIL",
              "policy_block_sha256": hashlib.sha256(current.encode()).hexdigest(),
              "production_meson_sha256": hashlib.sha256(source.encode()).hexdigest(),
              "cases": results,
              "limitations": ["Native GNU compiler with deterministic wrapper controls only.",
                              "No MSVC, clang-cl, macOS or cross-compiler execution is claimed.",
                              "Isolated policy projects reproduce the exact current exception block; not a full GIMP setup on alternative compilers."]}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())

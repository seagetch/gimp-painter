#!/usr/bin/env python3
"""Build/run independent nonlinear-U8 Threshold/Max RGB/Value Invert raw shadows.

The focused test preserves Threshold Alpha/Max RGB coverage and checks all 40
archived point outputs (20 Threshold, 16 Max RGB, 4 Value Invert). Max RGB and
Value Invert final outputs are compared after an explicit merge. A separately
compiled C oracle preserves the literal pinned vinvert_render_row callback;
generated raw arithmetic/geometry/cancellation/transactional cases are separate.
It does not launch old GIMP or certify the native FilterLayer/XCF integration.

Source/fixture verification only (no compilation or native execution):
  python3 app/painter/tests/run_filter_point_shadow_tests.py --verify-only
When execution is authorized, hold the shared build lock:
  flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_point_shadow_tests.py BUILD --report REPORT
  flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_point_shadow_tests.py BUILD --report REPORT --sanitize

CC, CXX, CPPFLAGS, CFLAGS, CXXFLAGS and LDFLAGS are honored. Sanitizer flags apply
to both the C oracle and the C++ target. Existing sanitizer options are
preserved, with required options appended. ASan/UBSan excludes LeakSanitizer;
there is no leak claim. Each build uses a new private output directory and a
new report path. No GUI/profile, install, publication, or capture is performed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = "migration/fixtures/legacy-filter-points/fixtures.tsv"
LEGACY_COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"
LEGACY_SOURCES = {
    "plug-ins/common/value-invert.c": "bf7c46cac7f62c13ec73590d53c12ba3dd85fd28def5f74451aaf43b969fef20",
    "plug-ins/common/max-rgb.c": "e783de344ff3230ad611453dd4b4c7f87a40ffa0b0fa4b49a40b4449283cfcc1",
    "plug-ins/common/threshold-alpha.c": "608b6e013c6c2285ccdd651f1c2e8204990a8cdae1d65d8b8774b47e7a59a3ca",
    "libgimp/gimpregioniterator.c": "1289b75a1c49185d9ddaa5f97b5a406b297eab78560854344e8b9a4da41213be",
}
VALUE_INVERT_ORACLE = "app/painter/tests/value-invert-legacy-oracle.c"
# Covers the untouched callback AND the self-contained types, MAX/MIN macros,
# license/attribution and exported wrapper. Callback equality is checked again
# against the complete hash-pinned original source, rather than an equation.
VALUE_INVERT_ORACLE_SHA256 = "86a4ad3bc3b09aa1fce74ce740af5cf627a753711d4d78ecc2f018cbd44f7ca4"
C_SOURCES = [VALUE_INVERT_ORACLE]
CPP_SOURCES = [
    "app/painter/filter-raster.cpp",
    "app/painter/filter-native-kernels.cpp",
    "app/painter/filter-context.cpp",
    "app/painter/filter-point-shadow.cpp",
    "app/painter/tests/test-filter-point-shadow.cpp",
]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_value_invert_oracle(pinned_source):
    definition = b"static void\nvinvert_render_row ("
    if pinned_source.count(definition) != 1:
        raise ValueError("Pinned Value Invert callback definition is not unique")
    # The hash-pinned legacy source ends with this complete callback. Keeping
    # the original bytes also preserves its branch ordering and integer casts.
    callback = pinned_source[pinned_source.index(definition):]
    if not callback.endswith(b"}\n"):
        raise ValueError("Pinned Value Invert callback boundary differs")
    oracle = (ROOT / VALUE_INVERT_ORACLE).read_bytes()
    begin = b"/* BEGIN LITERAL PINNED CALLBACK */\n"
    end = b"/* END LITERAL PINNED CALLBACK */\n"
    if oracle.count(begin) != 1 or oracle.count(end) != 1:
        raise ValueError("Value Invert C oracle callback markers differ")
    actual = oracle.split(begin, 1)[1].split(end, 1)[0]
    if actual != callback:
        raise ValueError("Value Invert C oracle is not the literal pinned callback")
    if hashlib.sha256(oracle).hexdigest() != VALUE_INVERT_ORACLE_SHA256:
        raise ValueError("Value Invert C oracle types/macros/wrapper seal differs")
    return {
        "path": VALUE_INVERT_ORACLE,
        "sha256": VALUE_INVERT_ORACLE_SHA256,
        "callback_sha256": hashlib.sha256(callback).hexdigest(),
        "literal_callback_matches_pinned_source": True,
        "build_language": "C99, separately compiled with CC/CFLAGS",
        "note": "Literal source callback oracle; not a fresh original-GIMP runtime capture",
    }


def fixture_input_paths():
    """Discover every archived input before verification/build sealing begins."""
    base = (ROOT / MANIFEST).parent.resolve()
    paths = {base / name for name in ("capture-report.json", "fixtures.tsv", "capture.scm", "capture.log", "README.md")}
    for line in (ROOT / MANIFEST).read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 7:
            raise ValueError("Malformed point fixture manifest row")
        for name in fields[-2:]:
            path = (base / name).resolve()
            if path.parent != base:
                raise ValueError(f"Point fixture escapes the archive directory: {name}")
            paths.add(path)
    return sorted(paths)


def verify_fixtures():
    base = (ROOT / MANIFEST).parent
    capture_path = base / "capture-report.json"
    capture = json.loads(capture_path.read_text())
    if (capture.get("source_commit") != LEGACY_COMMIT or capture.get("status") != "passed" or
            capture.get("exit_code") != 0 or capture.get("errors") != [] or capture.get("case_count") != 40):
        raise ValueError("Archived genuine point capture is not the accepted 40-case corpus")
    for name, key in [("fixtures.tsv", "fixtures_sha256"), ("capture.scm", "script_sha256"),
                      ("capture.log", "log_sha256")]:
        if digest(base / name) != capture.get(key):
            raise ValueError(f"Archived capture seal differs: {name}")
    for name in ("threshold-alpha.c", "max-rgb.c", "value-invert.c"):
        if capture.get("source_sha256", {}).get(name) != LEGACY_SOURCES["plug-ins/common/" + name]:
            raise ValueError(f"Archived point source provenance differs: {name}")
    env = dict(os.environ, GIT_NO_LAZY_FETCH="1")
    pinned = {}
    for path, expected in LEGACY_SOURCES.items():
        source = subprocess.check_output(["git", "show", f"{LEGACY_COMMIT}:{path}"], cwd=ROOT, env=env)
        if hashlib.sha256(source).hexdigest() != expected:
            raise ValueError(f"Pinned legacy source differs: {path}")
        pinned[path] = source
    oracle = verify_value_invert_oracle(pinned["plug-ins/common/value-invert.c"])
    paths = {capture_path, base / "fixtures.tsv", base / "capture.scm", base / "capture.log", base / "README.md"}
    cases = {}
    for case in capture.get("cases", []):
        key = (case["procedure"], case["argument"], case["width"], case["height"],
               case["channels"], case["input"], case["output"])
        if key in cases:
            raise ValueError("Duplicate archived capture case")
        cases[key] = case
    seen = set()
    threshold_cases = 0
    max_rgb_cases = 0
    value_invert_cases = 0
    for line in (ROOT / MANIFEST).read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if len(fields) != 7:
            raise ValueError("Malformed point fixture manifest row")
        proc, argument, width, height, channels, src, dst = fields
        key = (proc, int(argument), int(width), int(height), int(channels), src, dst)
        if key not in cases or key in seen:
            raise ValueError("Fixture manifest and archived capture cases differ")
        seen.add(key)
        if proc not in ("plug-in-threshold-alpha", "plug-in-max-rgb", "plug-in-vinvert"):
            raise ValueError(f"Unknown point fixture procedure: {proc}")
        if int(width) <= 0 or int(height) <= 0:
            raise ValueError("Point fixture dimensions must be positive")
        if proc == "plug-in-threshold-alpha" and int(channels) not in (2, 4):
            raise ValueError("Threshold Alpha fixture does not have alpha")
        if proc == "plug-in-max-rgb" and int(channels) not in (3, 4):
            raise ValueError("Max RGB fixture is not RGB/RGBA")
        if proc == "plug-in-vinvert" and (int(channels) not in (3, 4) or int(argument) != 0):
            raise ValueError("Value Invert fixture must be argument-free RGB/RGBA")
        for name in (src, dst):
            path = (base / name).resolve()
            if path.parent != base.resolve() or path.stat().st_size != int(width) * int(height) * int(channels):
                raise ValueError(f"Invalid point fixture path or byte extent: {name}")
            paths.add(path)
        if digest(base / dst) != cases[key]["output_sha256"]:
            raise ValueError(f"Archived original output hash differs: {dst}")
        if proc == "plug-in-threshold-alpha":
            threshold_cases += 1
        elif proc == "plug-in-max-rgb":
            max_rgb_cases += 1
        else:
            value_invert_cases += 1
    if seen != set(cases) or threshold_cases != 20 or max_rgb_cases != 16 or value_invert_cases != 4:
        raise ValueError("Fixture manifest lost original Threshold Alpha/Max RGB/Value Invert cases")
    provenance = {
        "source_commit": LEGACY_COMMIT,
        "verified_source_sha256": LEGACY_SOURCES,
        "archived_capture_report": str(capture_path.relative_to(ROOT)),
        "archived_captured_utc": capture["captured_utc"],
        "archived_executable_sha256": capture["executables"],
        "threshold_alpha_cases": threshold_cases,
        "max_rgb_cases": max_rgb_cases,
        "value_invert_cases": value_invert_cases,
        "value_invert_c_oracle": oracle,
        "note": "Historical full-drawable final PDB outputs, not raw callback captures; Max RGB and Value Invert comparisons follow explicit full-coverage merge. No new old-runtime or owner-context capture",
    }
    return sorted(paths), provenance


def source_paths():
    pending = [ROOT / path for path in C_SOURCES + CPP_SOURCES]
    seen = set()
    while pending:
        path = pending.pop().resolve()
        if path in seen:
            continue
        seen.add(path)
        for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', path.read_text(), re.MULTILINE):
            for candidate in [path.parent / name, ROOT / "app/painter" / name]:
                if candidate.is_file():
                    pending.append(candidate)
                    break
            else:
                raise ValueError(f"Unresolved local include {name} in {path}")
    seen.update([Path(__file__).resolve(), ROOT / "app/painter/meson.build",
                 ROOT / "app/painter/tests/test_filter_point_shadow_runner.py"])
    return sorted(seen)


def hash_paths(paths):
    return {str(path.relative_to(ROOT)): digest(path) for path in paths}


def changed_paths(before):
    return [name for name, sha in before.items() if not (ROOT / name).is_file() or digest(ROOT / name) != sha]


def flags(name):
    return shlex.split(os.environ.get(name, ""))


def append_options(env, name, required):
    values = env.get(name, "")
    env[name] = values + (":" if values else "") + ":".join(required)


def write_report(path, report):
    path.write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, nargs="?")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if not args.verify_only and (args.build is None or args.report is None):
        parser.error("BUILD and --report are required unless --verify-only is selected")
    sources = hash_paths(source_paths())
    fixture_paths = fixture_input_paths()
    fixtures = hash_paths(fixture_paths)
    before = {**sources, **fixtures}
    verified_paths, provenance = verify_fixtures()
    if verified_paths != fixture_paths:
        raise ValueError("Fixture inputs changed during seal verification")
    changed_after_verify = changed_paths(before)
    if changed_after_verify:
        raise ValueError("Inputs changed during seal verification: " + ", ".join(changed_after_verify))
    report = {
        "scope": "Non-UI U8 Threshold Alpha, Max RGB and Value Invert raw-shadow foundation; generated scalar/geometry checks, direct raw and vector/spill fault handling, completed-buffer isolation; historical final PDB comparisons",
        "max_rgb_raw_evidence": "Independent generated pairwise equations, all alpha values, ties and signed mode classes; archived Max RGB outputs are compared only after explicit full-coverage merge",
        "value_invert_raw_evidence": "Literal hash-pinned legacy vinvert_render_row callback; execution mode compiles the oracle separately as C and compares archived final Value Invert outputs only after explicit full-coverage merge",
        "not_covered": ["Fresh original-GIMP runtime capture", "Native FilterLayer/XCF app integration",
                        "Final owner context timing", "Owner RGB-only admission", "Scheduler/owner lifetime", "Full modern-real precision/owner route acceptance",
                        "UI", "Windows/macOS execution", "Actual I/O beyond 4 GiB", "LeakSanitizer"],
        "legacy_evidence": provenance,
        "source_sha256": sources,
        "fixture_sha256": fixtures,
        "changed_after_verify": changed_after_verify,
        "sanitizers": ["address", "undefined"] if args.sanitize else [],
        "leak_detection": False if args.sanitize else None,
        "leak_detection_note": "Focused ASan/UBSan mode excludes LeakSanitizer; no leak claim is made",
        "commands": [], "all_passed": False,
    }
    if args.report:
        # Reserve a fresh report before compilation; never replace old evidence.
        with args.report.open("x") as destination:
            destination.write(json.dumps(report, indent=2) + "\n")
        args.report.chmod(0o600)
    if args.verify_only:
        report.update(scope="Source/archived-fixture seal verification only; no compilation or native execution",
                      all_passed=None, source_only_verified=True, sanitizers=[])
        print(json.dumps(report, indent=2))
        if args.report:
            write_report(args.report, report)
        return 0

    args.build.mkdir(parents=True, exist_ok=True)
    out = Path(tempfile.mkdtemp(prefix="filter-point-shadow-" + ("sanitize-" if args.sanitize else "normal-"),
                                dir=args.build.resolve()))
    temporary = out / "tmp"
    temporary.mkdir(mode=0o700)
    env = dict(os.environ, TMPDIR=str(temporary))
    sanitize = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else []
    if args.sanitize:
        append_options(env, "ASAN_OPTIONS", ["detect_leaks=0", "halt_on_error=1", "abort_on_error=1"])
        append_options(env, "UBSAN_OPTIONS", ["halt_on_error=1", "print_stacktrace=1"])
    report["output_directory"] = str(out)
    report["build_environment"] = {name: env.get(name, "") for name in
                                   ["CC", "CXX", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS", "ASAN_OPTIONS", "UBSAN_OPTIONS"]}
    started = time.monotonic()

    def invoke(command, kind):
        record = {"kind": kind, "command": command}
        report["commands"].append(record)
        try:
            run = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
            record.update(exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr)
            print(run.stdout, end="")
            if run.stderr:
                print(run.stderr, end="", file=sys.stderr)
            return run.returncode == 0
        except subprocess.TimeoutExpired as error:
            record.update(exit_code=None, timed_out=True,
                          stdout=(error.stdout or b"").decode(errors="replace") if isinstance(error.stdout, bytes) else error.stdout or "",
                          stderr=(error.stderr or b"").decode(errors="replace") if isinstance(error.stderr, bytes) else error.stderr or "")
            return False
        except OSError as error:
            record.update(exit_code=None, error=str(error))
            return False

    try:
        cc = shlex.split(os.environ.get("CC", "cc"))
        cxx = shlex.split(os.environ.get("CXX", "c++"))
        success = invoke([*cc, "--version"], "c-compiler-version")
        success = invoke([*cxx, "--version"], "c++-compiler-version") and success
        success = invoke(["pkg-config", "--cflags", "glib-2.0"], "pkg-cflags") and success
        pkg = shlex.split(report["commands"][-1].get("stdout", ""))
        success = invoke(["pkg-config", "--libs", "glib-2.0"], "pkg-libs") and success
        libs = shlex.split(report["commands"][-1].get("stdout", ""))
        common = ["-Wall", "-Wextra", "-Werror", "-g", "-O1" if args.sanitize else "-O2", "-pthread"]
        includes = ["-I" + str(ROOT / "app/painter")]
        objects = []
        for source in C_SOURCES:
            if not success:
                break
            obj = out / (Path(source).name + ".o")
            objects.append(str(obj))
            command = [*cc, *common, *flags("CPPFLAGS"), "-std=c99", *flags("CFLAGS"),
                       *sanitize, "-x", "c", "-c", str(ROOT / source), "-o", str(obj)]
            success = invoke(command, "compile-c")
        for source in CPP_SOURCES:
            if not success:
                break
            obj = out / (Path(source).name + ".o")
            objects.append(str(obj))
            command = [*cxx, *common, *flags("CPPFLAGS"), "-std=c++14", *flags("CXXFLAGS"),
                       "-fexceptions", "-frtti" if args.sanitize else "-fno-rtti", *sanitize, *includes, *pkg,
                       "-c", str(ROOT / source), "-o", str(obj)]
            success = invoke(command, "compile-c++")
        executable = out / "painter-filter-point-shadow"
        if success:
            success = invoke([*cxx, *objects, *flags("LDFLAGS"), *sanitize, "-pthread", *libs,
                              "-o", str(executable)], "link")
        report["changed_after_compile"] = changed_paths(before)
        if success and not report["changed_after_compile"]:
            report["object_sha256"] = {Path(path).name: digest(Path(path)) for path in objects}
            report["executable_sha256"] = digest(executable)
            success = invoke([str(executable), str(ROOT / MANIFEST)], "test")
            report["executable_sha256_after_run"] = digest(executable)
            report["object_sha256_after_run"] = {
                Path(path).name: digest(Path(path)) if Path(path).is_file() else None
                for path in objects}
        report["changed_after_run"] = changed_paths(before)
        report["executable_unchanged"] = (bool(report.get("executable_sha256")) and
                                          report.get("executable_sha256") == report.get("executable_sha256_after_run"))
        report["objects_unchanged"] = (bool(report.get("object_sha256")) and
                                       report.get("object_sha256") == report.get("object_sha256_after_run"))
        report["all_passed"] = (success and not report["changed_after_compile"] and
                                not report["changed_after_run"] and report["executable_unchanged"] and
                                report["objects_unchanged"])
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        report["error"] = str(error)
    finally:
        report["elapsed_seconds"] = time.monotonic() - started
        write_report(args.report, report)
    return 0 if report["all_passed"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        print(f"Point-shadow verification/build setup failed: {error}", file=sys.stderr)
        raise SystemExit(1)

#!/usr/bin/env python3
"""Build/run the non-UI sharpen foundation against unchanged legacy C bodies.

This is source-extracted arithmetic evidence, NOT a native old-GIMP capture.
The harness has no FilterLayer/selection/component/shadow-merge integration.

Source-only verification (no compilation or native execution):
  python3 app/painter/tests/run_filter_sharpen_tests.py --verify-only
Focused builds, when execution is authorized, under the shared build lock:
  flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_sharpen_tests.py BUILD --report REPORT
  flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_sharpen_tests.py BUILD --report REPORT --sanitize

C and C++ are compiled separately. CC/CXX, CPPFLAGS, CFLAGS, CXXFLAGS and
LDFLAGS are honored; sanitizer options preserve unrelated environment entries.
No install, GUI process, profile access, repository publication or app-route
change is performed. Meson registers the same C/C++ test without this builder.
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
import time

ROOT = Path(__file__).resolve().parents[3]
LEGACY_COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"
LEGACY_PATH = "plug-ins/common/sharpen.c"
LEGACY_SHA256 = "6b16f3d3ee6722157bad2772d1cfdee8e3db8e7ef7fb350ae2cbee8ee832312b"
ORACLE = "app/painter/tests/legacy-sharpen-oracle.c"
CPP_SOURCES = ["app/painter/filter-raster.cpp", "app/painter/filter-native-kernels.cpp",
               "app/painter/filter-sharpen.cpp", "app/painter/tests/test-filter-sharpen.cpp"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def marked(text, name):
    begin = f"/* BEGIN EXACT LEGACY {name} */\n"
    end = f"\n/* END EXACT LEGACY {name} */"
    if text.count(begin) != 1 or text.count(end) != 1:
        raise ValueError(f"Missing or ambiguous oracle markers: {name}")
    return text.split(begin, 1)[1].split(end, 1)[0]


def verify_oracle():
    env = dict(os.environ, GIT_NO_LAZY_FETCH="1")
    original = subprocess.check_output(
        ["git", "show", f"{LEGACY_COMMIT}:{LEGACY_PATH}"], cwd=ROOT, env=env)
    if hashlib.sha256(original).hexdigest() != LEGACY_SHA256:
        raise ValueError("Pinned legacy sharpen source hash differs")
    source = original.decode("utf-8")
    start = source.index("static void\ncompute_luts (void)")
    end = source.index("\n\n/*\n * 'sharpen_dialog()", start)
    rows = source.index("static void\ngray_filter (gint")
    expected = {"LUT AND SHARPEN": source[start:end], "ROW FILTERS": source[rows:].rstrip()}
    oracle = (ROOT / ORACLE).read_text()
    for name, body in expected.items():
        if marked(oracle, name) != body:
            raise ValueError(f"Oracle body is not byte-for-byte legacy source: {name}")
    return {"commit": LEGACY_COMMIT, "path": LEGACY_PATH, "sha256": LEGACY_SHA256,
            "verified_blocks": {name: hashlib.sha256(body.encode()).hexdigest()
                                for name, body in expected.items()}}


def source_paths(sources):
    pending = [ROOT / path for path in [ORACLE, *sources]]
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
    seen.update([Path(__file__).resolve(), ROOT / "app/painter/meson.build"])
    return sorted(seen)


def hash_sources(paths):
    return {str(path.relative_to(ROOT)): digest(path) for path in paths}


def flags(name):
    return shlex.split(os.environ.get(name, ""))


def append_options(env, name, required):
    values = env.get(name, "")
    env[name] = values + (":" if values else "") + ":".join(required)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, nargs="?")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--shadow", action="store_true",
                        help="Test full-raster zero-shadow/ROI wrapper instead of the cropped kernel")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if not args.verify_only and (args.build is None or args.report is None):
        parser.error("BUILD and --report are required unless --verify-only is selected")
    provenance = verify_oracle()
    sources = list(CPP_SOURCES)
    if args.shadow:
        sources[-1] = "app/painter/tests/test-filter-sharpen-shadow.cpp"
        sources += ["app/painter/filter-sharpen-shadow.cpp", "app/painter/filter-context.cpp"]
    paths = source_paths(sources)
    before = hash_sources(paths)
    if args.verify_only:
        result = {"scope": "Source-only oracle verification; no compilation or native execution",
                  "legacy_source": provenance, "source_sha256": before}
        print(json.dumps(result, indent=2))
        if args.report:
            args.report.write_text(json.dumps(result, indent=2) + "\n")
        return 0

    target = "filter-sharpen-shadow" if args.shadow else "filter-sharpen"
    out = args.build.resolve() / (target + ("-sanitize" if args.sanitize else "-normal"))
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    sanitize = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if args.sanitize else []
    if args.sanitize:
        append_options(env, "ASAN_OPTIONS", ["detect_leaks=0", "halt_on_error=1", "abort_on_error=1"])
        append_options(env, "UBSAN_OPTIONS", ["halt_on_error=1", "print_stacktrace=1"])
    report = {
        "scope": ("Non-UI full-raster zero-shadow/ROI wrapper, source C oracle, vector/spill" if args.shadow else
                  "Non-UI raw-shadow sharpen kernel, native-format source C oracle, vector/spill wrappers"),
        "not_covered": ["Native old-GIMP runtime", "FilterLayer app route", "Selection/component/alpha shadow merge",
                        "Scheduler/owner lifetime", "UI", "Windows/macOS execution", "Actual I/O beyond 4 GiB"],
        "legacy_source": provenance,
        "source_sha256": before,
        "sanitizers": ["address", "undefined"] if args.sanitize else [],
        "leak_detection": False if args.sanitize else None,
        "leak_detection_note": "Focused ASan/UBSan mode excludes LeakSanitizer; no leak claim is made",
        "commands": [], "all_passed": False,
    }
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
        pkg = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "glib-2.0"], text=True))
        libs = shlex.split(subprocess.check_output(["pkg-config", "--libs", "glib-2.0"], text=True))
        cc = shlex.split(os.environ.get("CC", "cc"))
        cxx = shlex.split(os.environ.get("CXX", "c++"))
        common = ["-Wall", "-Wextra", "-Werror", "-g", "-O1" if args.sanitize else "-O2", "-pthread"]
        includes = ["-I" + str(ROOT / "app/painter")]
        objects = []
        success = True
        for source in [ORACLE, *sources]:
            obj = out / (Path(source).name + ".o")
            objects.append(str(obj))
            is_c = source.endswith(".c")
            language = (["-std=c11", *flags("CFLAGS")] if is_c else
                        ["-std=c++14", *flags("CXXFLAGS"), "-fexceptions", "-frtti" if args.sanitize else "-fno-rtti"])
            command = [*(cc if is_c else cxx), *common, *flags("CPPFLAGS"), *language, *sanitize,
                       *includes, *([] if is_c else pkg), "-c", str(ROOT / source), "-o", str(obj)]
            if not invoke(command, "compile-c" if is_c else "compile-c++"):
                success = False
                break
        executable = out / ("painter-" + target)
        if success:
            success = invoke([*cxx, *objects, *flags("LDFLAGS"), *sanitize, "-pthread", *libs,
                              "-o", str(executable)], "link")
        report["changed_after_compile"] = [name for name, sha in before.items() if digest(ROOT / name) != sha]
        if success and not report["changed_after_compile"]:
            report["executable_sha256"] = digest(executable)
            success = invoke([str(executable)], "test")
        report["changed_after_run"] = [name for name, sha in before.items() if digest(ROOT / name) != sha]
        report["all_passed"] = success and not report["changed_after_compile"] and not report["changed_after_run"]
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        report["error"] = str(error)
    finally:
        report["elapsed_seconds"] = time.monotonic() - started
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["all_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

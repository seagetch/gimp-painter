#!/usr/bin/env python3
"""Check the actual argument snapshot helpers independently of the GIMP app.

Source the restored build environment first. Example:
  python3 migration/tests/run_xcf_argument_storage_tests.py --sanitizers

The normal and forced big-endian helper builds exercise identical production
text. Only the conditional selecting the conversion branch changes; this is
not a claim of testing a big-endian host or the full XCF transport. Full codec
syntax is also checked with the configured compilation database. Source hashes
are captured under the build lock before compilation and checked after testing.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
CODEC = "app/xcf/painter-xcf-arguments.cpp"
TEST = "app/xcf/tests/test-painter-xcf-arguments-storage.cpp"
SOURCES = (
    CODEC, "app/xcf/painter-xcf-arguments.hpp",
    "app/xcf/painter-xcf-storage.cpp", "app/xcf/painter-xcf-storage.hpp",
    "app/painter/bytes.hpp", "app/core/gimpfilterlayer.h",
    "app/core/gimpfilterlayer-arguments.hpp", "libgimpbase/gimpparamspecs.c",
    "libgimpbase/gimpparamspecs.h", TEST,
    "migration/tests/run_xcf_argument_storage_tests.py",
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def hashes():
    return {name: digest((ROOT / name).read_bytes()) for name in SOURCES}


def fragment(text, start, end):
    if text.count(start) != 1 or text.count(end) != 1:
        raise ValueError("Production extraction anchors changed; review the runner")
    return text[text.index(start):text.index(end)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--builddir", type=Path, default=ROOT / "build-installed-filter")
    parser.add_argument("--report", type=Path, default=ROOT / "migration/tests/xcf-multipart-storage/arguments-storage.json")
    parser.add_argument("--lock-file", type=Path, default=Path(os.environ.get("GIMP_PAINTER_BUILD_LOCK", "/workspace/shared/gimp-painter-build.lock")))
    parser.add_argument("--sanitizers", action="store_true", help="Also run ASan/UBSan builds")
    parser.add_argument("--detect-leaks", action="store_true", help="Enable LeakSanitizer when the execution runtime supports it")
    args = parser.parse_args()
    if args.detect_leaks and not args.sanitizers:
        parser.error("--detect-leaks requires --sanitizers")
    builddir = args.builddir.resolve()
    report = {
        "scope": "Production argument encode/decode helper boundaries, materialization budget, ownership and cancellation; controlled allocation failures in codec/core scalar-import helpers; full codec syntax; forced endian branch is not big-endian hardware or full native XCF integration",
        "status": "failed", "sanitizers": args.sanitizers,
        "leak_detection": args.detect_leaks, "checks": [],
    }
    if args.report.exists():
        previous = json.loads(args.report.read_text())
        failures = list(previous.get("prior_failed_attempts", []))
        if previous.get("status") == "failed":
            failures.append({key: previous[key] for key in
                             ("started_utc", "completed_utc", "error", "source_sha256_before", "sources_unchanged")
                             if key in previous})
            failures[-1]["failed_checks"] = [check for check in previous.get("checks", []) if check.get("exit_code")]
        if failures:
            report["prior_failed_attempts"] = failures

    def run(label, command, cwd=ROOT, environment=None):
        started = time.monotonic()
        result = subprocess.run(command, cwd=cwd, env=environment, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
        report["checks"].append({"name": label, "exit_code": result.returncode,
                                 "seconds": round(time.monotonic() - started, 3),
                                 "output": result.stdout[-6000:]})
        if result.returncode:
            raise RuntimeError(label + " failed")
        print(label + ": passed", flush=True)

    with args.lock_file.open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        report["started_utc"] = datetime.now(timezone.utc).isoformat()
        report["source_sha256_before"] = hashes()
        try:
            database = builddir / "compile_commands.json"
            entry = next(item for item in json.loads(database.read_text())
                         if item["file"].endswith("/painter-xcf-arguments.cpp"))
            report["compile_entry_sha256"] = digest(json.dumps(entry, sort_keys=True).encode())
            report["config_sha256"] = digest((builddir / "config.h").read_bytes())
            syntax = []
            skip = False
            for argument in entry.get("arguments", []) or shlex.split(entry["command"]):
                if skip:
                    skip = False
                elif argument in ("-MQ", "-MF", "-MT", "-o"):
                    skip = True
                elif argument not in ("-MD", "-MMD", "-c", entry["file"]):
                    syntax.append(argument)
            syntax.append("-fsyntax-only")
            source = (ROOT / CODEC).read_text()
            helper = fragment(source, "struct VariantFree", "struct ArgsFree") + fragment(
                source, "constexpr gsize block_size", "GVariant *reference (")
            helper = helper.replace("#if G_BYTE_ORDER == G_BIG_ENDIAN",
                                    "#if defined(PAINTER_TEST_FORCE_BIG_ENDIAN) || G_BYTE_ORDER == G_BIG_ENDIAN")
            array = fragment((ROOT / "libgimpbase/gimpparamspecs.h").read_text(),
                             "typedef struct _GimpArray GimpArray;", "\n/*\n * GIMP_TYPE_COLOR_ARRAY\n */")
            imported = fragment((ROOT / "app/core/gimpfilterlayer-arguments.hpp").read_text(),
                                "inline gpointer try_argument_copy", "class FilterArguments")
            base_library = builddir / "libgimpbase/libgimpbase-3.0.so"
            report["libgimpbase_sha256_before"] = digest(base_library.read_bytes())
            flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "gio-2.0"], text=True))
            compiler = shlex.split(os.environ.get("CXX", "c++"))
            report["compiler"] = subprocess.check_output(compiler + ["--version"], text=True).splitlines()[0]
            with tempfile.TemporaryDirectory(prefix="painter-xcf-argument-check-") as temporary:
                work = Path(temporary)
                (work / "painter-xcf-arguments-storage-helpers.hpp").write_text(
                    'extern "C" {\n' + array + "\n}\nnamespace GimpPainterXcf { namespace {\n"
                    '[[noreturn]] void invalid () { throw std::runtime_error ("invalid"); }\n'
                    + helper + "\n} }\nnamespace ImportedScalarTest {\n" + imported + "\n}\n")
                forced = work / "painter-xcf-arguments-forced.cpp"
                forced.write_text(source.replace("#if G_BYTE_ORDER == G_BIG_ENDIAN", "#if 1"))
                run("codec-native-syntax", syntax + [str(ROOT / CODEC)], Path(entry["directory"]))
                run("codec-forced-big-endian-syntax", syntax + [str(forced)], Path(entry["directory"]))
                for sanitized in ([False, True] if args.sanitizers else [False]):
                    for endian in (False, True):
                        label = ("forced-big-endian" if endian else "native") + ("-asan-ubsan" if sanitized else "")
                        binary = work / label
                        command = compiler + ["-std=c++14", "-g", "-O1" if sanitized else "-O2", "-Wall", "-Wextra", "-Werror", "-pthread"]
                        if sanitized:
                            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
                        if endian:
                            command += ["-DPAINTER_TEST_FORCE_BIG_ENDIAN=1"]
                        command += ["-I" + str(ROOT / "app/xcf"), "-I" + str(work),
                                    str(ROOT / TEST), str(ROOT / "app/xcf/painter-xcf-storage.cpp"),
                                    str(base_library), "-Wl,-rpath," + str(base_library.parent), "-o", str(binary)] + flags
                        run(label + "-compile", command)
                        environment = dict(os.environ)
                        if sanitized:
                            environment.update(ASAN_OPTIONS=f"detect_leaks={int(args.detect_leaks)}:halt_on_error=1:abort_on_error=1",
                                               UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
                        run(label + "-tests", [str(binary)], environment=environment)
            report["status"] = "passed"
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError, StopIteration) as error:
            report["error"] = str(error)
        finally:
            report["source_sha256_after"] = hashes()
            report["sources_unchanged"] = report["source_sha256_before"] == report["source_sha256_after"]
            if "libgimpbase_sha256_before" in report:
                report["libgimpbase_sha256_after"] = digest(base_library.read_bytes())
                report["sources_unchanged"] &= report["libgimpbase_sha256_before"] == report["libgimpbase_sha256_after"]
            if not report["sources_unchanged"]:
                report["status"] = "failed"
                report["error"] = "Tested sources changed during compilation or execution"
            report["completed_utc"] = datetime.now(timezone.utc).isoformat()
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(str(args.report))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

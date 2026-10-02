#!/usr/bin/env python3
"""Build exact vector/spill kernels, compare old-runtime bytes, and record bounds.

Run with the migration dependency environment and hold the shared build lock:
flock /tmp/gimp-painter-build.lock python3 migration/tests/run_filter_raster_kernel_tests.py BUILD --report REPORT [--sanitize]
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
parser.add_argument("--sanitize", action="store_true")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = args.build.resolve() / ("filter-raster-kernel-sanitizers" if args.sanitize else "filter-raster-kernel-tests")
out.mkdir(parents=True, exist_ok=True)
sources = ["app/painter/filter-edge.cpp", "app/painter/filter-gauss.cpp",
           "app/painter/filter-raster.cpp", "app/painter/filter-raster-kernels.cpp"]
headers = ["app/painter/filter-edge.hpp", "app/painter/filter-gauss.hpp", "app/painter/filter-raster.hpp",
           "app/painter/filter-edge-kernel-private.hpp", "app/painter/filter-gauss-kernel-private.hpp",
           "app/painter/filter-raster-kernels.hpp"]
test = "app/painter/tests/test-filter-raster-kernels.cpp"
fixtures = [root / "migration/fixtures" / corpus / "fixtures.tsv" for corpus in
            ["legacy-edge", "legacy-gauss", "legacy-gray-filter"]]
flags = ["-std=c++14", "-Wall", "-Wextra", "-Werror", "-g", "-pthread",
         "-I" + str(root / "app/painter")]
flags += (["-O1", "-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-frtti"]
          if args.sanitize else ["-O2"])
pkg_flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0"], text=True))
env = dict(os.environ)
if args.sanitize:
    env.update({"ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
                "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
commands = []
runs = []
all_sources = sources + headers + [test, "migration/tests/run_filter_raster_kernel_tests.py"]

def compile_run(name, translation_units, arguments, glib=False):
    exe = out / name
    command = ["g++", *flags, *[str(root / source) for source in translation_units],
               *(pkg_flags if glib else []), "-o", str(exe)]
    subprocess.run(command, check=True)
    commands.append(command)
    test_command = [str(exe), *[str(arg) for arg in arguments]]
    result = subprocess.run(test_command, env=env, capture_output=True, text=True)
    runs.append({"command": test_command, "exit_code": result.returncode,
                 "stdout": result.stdout, "stderr": result.stderr})
    print(result.stdout, end="")
    if result.returncode:
        print(result.stderr, file=sys.stderr)
    return exe

exe = compile_run("painter-filter-raster-kernels", sources + [test], fixtures, True)
for name, units, manifest in [
    ("edge", ["app/painter/filter-edge.cpp"], fixtures[0]),
    ("gauss", ["app/painter/filter-gauss.cpp"], fixtures[1]),
    ("gray", ["app/painter/filter-edge.cpp", "app/painter/filter-gauss.cpp"], fixtures[2]),
]:
    test_source = f"app/painter/tests/test-filter-{name}.cpp"
    all_sources.append(test_source)
    compile_run("standalone-filter-" + name, units + [test_source], [manifest])

# An isolated run has no vector oracle or whole-raster fixture allocation, so
# its process high-water mark can independently support the static heap bound.
bounded_rss = None
if not args.sanitize and hasattr(os, "wait4"):
    command = [str(exe), "--bounded-only"]
    stdout_file, stderr_file = out / "bounded.stdout", out / "bounded.stderr"
    with stdout_file.open("w") as stdout, stderr_file.open("w") as stderr:
        with subprocess.Popen(command, env=env, stdout=stdout, stderr=stderr) as child:
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            exit_code = child.returncode
    runs.append({"command": command, "exit_code": exit_code,
                 "stdout": stdout_file.read_text(), "stderr": stderr_file.read_text()})
    if not exit_code:
        bounded_rss = usage.ru_maxrss
    print(stdout_file.read_text(), end="")
fixture_files = {path for manifest in fixtures for path in manifest.parent.iterdir()
                 if path.suffix in {".tsv", ".rgba", ".y", ".ya"}}
report = {
    "scope": "Exact independent worker kernels and original standalone vector APIs; no GIMP adapter/scheduler coverage",
    "sanitizers": ["address", "undefined", "float-cast-overflow"] if args.sanitize else [],
    "leak_detection": False,
    "leak_detection_limit": "LeakSanitizer cannot operate under this executor ptrace sandbox; initial enabled attempt failed in LSan startup/exit, so ASan/UBSan run with detect_leaks=0",
    "compile_commands": commands,
    "source_sha256": {source: hashlib.sha256((root / source).read_bytes()).hexdigest() for source in all_sources},
    "fixture_sha256": {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                       for path in sorted(fixture_files)},
    "runtime_corpus_cases": {"edge_rgb": 76, "gauss_rgb": 104, "gray_grayalpha": 77},
    "allocation_bounds": {
        "edge_stack_bytes": 16408,
        "gaussian_peak_heap_bytes_excluding_objects_stdio_allocator":
            "max(4198400, 72*IIR_line_length, 16*RLE_line_length+28*RLE_curve_length+8, 131072)",
        "gaussian_scratch_rasters": "one if vertical>0, otherwise zero; input and output excluded",
        "disk_case_raster_bytes": 21003276,
        "isolated_bounded_case_peak_rss_kib": bounded_rss,
        "rss_caveat": "Whole process high-water mark, includes libraries/allocator/stack; Linux unsanitized isolated bounded case only",
    },
    "runs": runs,
    "all_passed": all(run["exit_code"] == 0 for run in runs),
}
args.report.write_text(json.dumps(report) + "\n")
sys.exit(0 if report["all_passed"] else 1)

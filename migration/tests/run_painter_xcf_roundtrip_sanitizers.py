#!/usr/bin/env python3
"""Instrument XCF application reading and writing and independent layer adapters in the existing full GIMP harness.

Run after building app/tests/painter-xcf-roundtrip, with the build environment loaded.
Does not claim to instrument all upstream GIMP/dependencies. Original build
objects are never replaced. Hold /tmp/gimp-painter-build.lock when sharing it.
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
args = parser.parse_args()
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "painter-xcf-roundtrip-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "XCF load/save/typed metadata/probe/source/seek/read/write, Clone/Filter and image-duplication adapters, scheduler/admission and Edge/Gauss executors, test harness and shared BindingStore; remaining GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined"], "leak_detection": False, "instrumented_cpp_rtti": True,
          "sources": [], "commands": [], "source_sha256": {}}
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/xcf/xcf.c", "app/xcf/xcf-load.c", "app/xcf/xcf-read.c", "app/xcf/xcf-seek.c",
          "app/xcf/xcf-save.c", "app/xcf/xcf-write.c", "app/xcf/painter-xcf-preserve.cpp", "app/xcf/painter-xcf-arguments.cpp", "app/core/gimpitem.c", "app/core/gimpimage-duplicate.c",
          "app/xcf/painter-xcf-load.cpp", "app/xcf/painter-xcf-compat.cpp",
          "app/tests/test-painter-xcf-roundtrip.c", "app/core/gimpclonelayer.cpp", "app/core/gimpfilterlayer.cpp",
          "app/painter/filter-scheduler.cpp", "app/painter/filter-edge.cpp", "app/painter/filter-gauss.cpp",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp", "app/painter/gimp-painter-error.cpp"}
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

headers = [
    "app/core/core-types.h", "app/core/core-enums.h", "app/core/gimpimage.h",
    "app/core/gimpimage-private.h", "app/core/gimpclonelayer.h",
    "app/core/gimpfilterlayer.h", "app/core/gimpfilterlayer-arguments.hpp",
    "app/core/gimp-painter-provenance.h", "app/xcf/xcf-private.h", "app/xcf/xcf.h",
    "app/xcf/painter-xcf-preserve.h", "app/xcf/painter-xcf-arguments.hpp",
    "app/painter/object-ref.hpp", "app/painter/connection.hpp",
    "app/painter/binding-store.hpp", "app/painter/filter-scheduler.hpp",
    "app/painter/work-admission.hpp", "app/painter/filter-edge.hpp", "app/painter/filter-gauss.hpp",
]
report["header_sha256"] = {name: digest(root / name) for name in headers}
report["hash_note"] = "Source hashes captured at compilation; selected interface headers captured before compilation. Any later change makes this run unsuitable for a frozen checkpoint."
replacements = {}
archive_replacements = {}
commands = json.loads((build / "compile_commands.json").read_text())
for entry in commands:
    source = (Path(entry["directory"]) / entry["file"]).resolve()
    try:
        relative = source.relative_to(root).as_posix()
    except ValueError:
        continue
    if relative not in wanted or relative in report["sources"]:
        continue
    module = "app/xcf/" if relative.startswith("app/xcf/") else "app/core/" if relative.startswith("app/core/") else "app/painter/" if relative.startswith("app/painter/") else None
    if module and not entry["output"].startswith(module + "libapp"):
        continue  # choose the real application command, not a standalone test copy
    cmd = shlex.split(entry["command"])
    cleaned = []
    skip = False
    for arg in cmd:
        if skip:
            skip = False
            continue
        if arg in ("-MF", "-MQ", "-MT"):
            skip = True
            continue
        if arg in ("-MD", "-MMD"):
            continue
        cleaned.append(arg)
    obj = output / (source.name + ".o")
    cleaned[cleaned.index("-o") + 1] = str(obj)
    cleaned += flags
    if source.suffix == ".cpp":
        cleaned += ["-frtti"]  # consistent allocator/control-block RTTI for UBSan vptr
    report["sources"].append(relative)
    report["commands"].append(cleaned)
    before = digest(source)
    subprocess.run(cleaned, cwd=build, check=True)
    if digest(source) != before:
        raise RuntimeError(f"Source changed during compilation: {relative}")
    report["source_sha256"][relative] = before
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        archive_replacements[entry["output"]] = str(obj)
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/painter-xcf-roundtrip"], cwd=build, text=True)
link = shlex.split(link_text.strip().splitlines()[-1])
exe = build / "app/tests/painter-xcf-roundtrip-asan"
link[link.index("-o") + 1] = str(exe)
link = [replacements.get(arg, arg) for arg in link]
# Omit original instrumented members from private thin archives. Leaving
# the native -fno-rtti scheduler allocation unit available can select its
# shared_ptr control-block COMDAT while the caller performs a vptr check.
for archive in ["app/core/libappcore.a", "app/xcf/libappxcf.a", "app/painter/libapppainter.a"]:
    members = subprocess.check_output(["ar", "t", archive], cwd=build, text=True).splitlines()
    rewritten = [str((build / member).resolve()) for member in members if member not in archive_replacements]
    sanitized_archive = output / Path(archive).name
    if sanitized_archive.exists():
        sanitized_archive.unlink()
    command = ["ar", "crsT", str(sanitized_archive)] + rewritten
    report["commands"].append(command)
    subprocess.run(command, cwd=build, check=True)
    link = [str(sanitized_archive) if arg == archive else arg for arg in link]
# A C-only harness has no early C++ RTTI anchor. Put instrumented definitions
# before native libraries so shared Error/control-block COMDATs cannot select
# an earlier -fno-rtti vtable. Their archive members were omitted above.
link[1:1] = flags + list(archive_replacements.values())
report["link_strategy"] = "Focused RTTI-enabled objects precede private archives with native members omitted; no sanitizer checks suppressed"
report["commands"].append(link)
subprocess.run(link, cwd=build, check=True)
env = dict(os.environ)
env.update({"GIMP_TESTING_ABS_TOP_SRCDIR": str(root),
            "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build),
            "GIMP_TESTING_PLUGINDIRS": str(build / "plug-ins/common"),
            "UI_TEST": "yes", "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe)], cwd=build, env=env, capture_output=True, text=True)
changed = [name for name, old in {**report["source_sha256"], **report["header_sha256"]}.items()
           if digest(root / name) != old]
exit_code = result.returncode if not changed else 1
report.update({"exit_code": exit_code, "test_exit_code": result.returncode,
               "changed_after_compile": changed, "stdout": result.stdout, "stderr": result.stderr})
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
if changed:
    print("Source/header changed during focused checkpoint: " + ", ".join(changed), file=sys.stderr)
sys.exit(exit_code)

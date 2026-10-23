#!/usr/bin/env python3
"""Build bounded BucketFillBrush original C provider ownership instrumentation; run on native display.

Hold /workspace/shared/gimp-painter-build.lock. Private archives leave production untouched.
"""
import hashlib
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
from painter_sanitizer_scope import bridge_rtti_sources, CXX_SUFFIXES

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
target = "bucket-fill-brush-entry"
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "bucket-fill-entry-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Original BucketFillBrush C operation names, sole FillBrush tool provider and common BindingStore users; native tool/options ownership; remaining listed C++ RTTI-only, other GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined", "float-cast-overflow"], "leak_detection": False, "instrumented_cpp_rtti": True,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/tests/test-bucket-fill-brush-entry.c", "app/tests/test-bucket-fill-brush-entry.cpp",
          "app/tools/gimpfillbrushtool.cpp", "app/paint/gimpfillbrush.cpp",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp", "app/painter/gimp-painter-error.cpp"}
# Shared BindingStore template/vtable registrations must agree on RTTI.
wanted |= {path.relative_to(root).as_posix() for path in (root / "app").rglob("*.cpp")
           if "/tests/" not in str(path) and '"painter/binding-store.hpp"' in path.read_text()}
# Instrument native tool/options ownership crossed by this entry and actual UI.
wanted |= {"app/core/gimpobject.c", "app/core/gimptoolinfo.c", "app/core/gimptooloptions.c", "app/core/gimpcontext.c",
           "app/tools/gimptool.c", "app/tools/gimpdrawtool.c", "app/tools/gimppainttool.c", "app/tools/gimpbrushtool.c"}
instrumented = set(wanted)
rtti_only = bridge_rtti_sources(root, build) - instrumented
wanted |= rtti_only
report["instrumented_sources"] = sorted(instrumented)
report["rtti_compatibility_only_sources"] = sorted(rtti_only)
headers = {"app/tools/gimpbucketfillbrushtool.h", "app/tools/gimpbucketfillbrushtool.hpp", "app/tools/gimpfillbrushtool.h",
           "app/paint/gimpfillbrush.h", "app/painter/binding-store.hpp", "app/painter/object-ref.hpp", "app/painter/boundary.hpp"}
hashes={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in wanted|headers}
replacements = {}
extra = []
archive_replacements = {}
commands = json.loads((build / "compile_commands.json").read_text())
for entry in commands:
    source = (Path(entry["directory"]) / entry["file"]).resolve()
    try:
        relative = source.relative_to(root).as_posix()
    except ValueError:
        continue
    if relative not in wanted:
        continue
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
    if relative in instrumented:
        cleaned += flags
    if source.suffix in CXX_SUFFIXES:
        cleaned += ["-frtti"] # consistent shared_ptr COMDAT RTTI for UBSan vptr

    report["sources"].append(relative)
    report["commands"].append(cleaned)
    # The ordinary object records Meson's complete dependency rebuild, while
    # the source and explicit shared-header hashes also catch local edits.
    ordinary = (build / entry["output"]).resolve()
    key = hashlib.sha256(json.dumps({"command": cleaned, "source": hashes[relative],
             "headers": {h: hashes[h] for h in headers},
             "ordinary": hashlib.sha256(ordinary.read_bytes()).hexdigest()}, sort_keys=True).encode()).hexdigest()
    key_path = obj.with_suffix(".key")
    if not (obj.exists() and key_path.exists() and key_path.read_text() == key):
        subprocess.run(cleaned, cwd=build, check=True)
        key_path.write_text(key)
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        archive_replacements[entry["output"]] = str(obj)
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
executables = {}
for target in ["bucket-fill-brush-entry"]:
    link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/" + target], cwd=build, text=True)
    link = shlex.split(link_text.strip().splitlines()[-1])
    exe = build / ("app/tests/" + target + "-asan")
    link[link.index("-o") + 1] = str(exe)
    link = [replacements.get(arg, arg) for arg in link]
    # Replace instrumented members inside private thin archives. Leaving the old
    # members available can pull in their RTTI COMDAT symbols and duplicate feature
    # definitions when -frtti is used for the vptr sanitizer.
    # Meson may flatten link_whole dependencies into additional thin archives
    # (notably libapptestutils). Replace every alias of an instrumented member,
    # otherwise RTTI references can pull an uninstrumented duplicate back in.
    absolute_replacements = {str((build / key).resolve()): value for key,value in archive_replacements.items()}
    for archive in sorted(set(arg for arg in link if arg.endswith(".a") and (build / arg).resolve().is_relative_to(build))):
        members = subprocess.check_output(["ar", "t", archive], cwd=build, text=True).splitlines()
        absolute_members = [str((build / member).resolve()) for member in members]
        if not any(member in absolute_replacements for member in absolute_members):
            continue
        rewritten = [absolute_replacements.get(member, member) for member in absolute_members]
        sanitized_archive = output / archive.replace("/", "_")
        if sanitized_archive.exists():
            sanitized_archive.unlink()
        command = ["ar", "crsT", str(sanitized_archive)] + rewritten
        report["commands"].append(command)
        subprocess.run(command, cwd=build, check=True)
        link = [str(sanitized_archive) if arg == archive else arg for arg in link]
    link[1:1] = flags
    report["commands"].append(link)
    subprocess.run(link, cwd=build, check=True)
    executables[target] = {"path": str(exe), "sha256": hashlib.sha256(exe.read_bytes()).hexdigest()}
changed=[name for name,digest in hashes.items() if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest]
report.update({"sources_sha256":hashes,"changed_during_build":changed,
               "executables":executables,
               "run_status":"Not run by builder; requires native GTK display"})
args.report.write_text(json.dumps(report,indent=2)+"\n")
if changed:raise RuntimeError("Instrumented source changed during the build")
print(json.dumps(executables))

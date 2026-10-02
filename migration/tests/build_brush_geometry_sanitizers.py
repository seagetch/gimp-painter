#!/usr/bin/env python3
"""Build focused ordinary-paper native instrumentation; run on the native display.

Hold /workspace/shared/gimp-painter-build.lock. Private archives leave production untouched.
"""
import hashlib
from painter_sanitizer_scope import bridge_rtti_sources, CXX_SUFFIXES
import argparse
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
targets = ["painter-brush-geometry", "painter-brush-geometry-trace", "painter-profile-native"]
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "brush-geometry-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Explicit-origin ordinary brush geometry, pinned generated/bitmap kernels, native BrushCore/PaintCore, profile provenance/default normalization, and original-mask/stroke/modern-invariant tests; remaining GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined", "float-cast-overflow"], "leak_detection": False, "instrumented_cpp_rtti": True,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/paint/gimppainterbrushgeometry.cpp", "app/paint/gimppainterpaper.cpp", "app/paint/gimppainterpaper-paste.cpp",
          "app/paint/gimpbrushcore.c", "app/paint/gimpbrushcore-loops.cc", "app/paint/gimppaintcore.c", "app/paint/gimppaintbrush.c", "app/paint/gimppaintoptions.c",
          "app/paint/painter-mypaint-surface/legacy-generated-mask.cpp", "app/paint/painter-mypaint-surface/legacy-mask-transform.cpp",
          "app/core/gimpdynamicsoutput.c", "app/core/gimptooloptions.c", "app/core/gimppainterprofile.cpp", "app/core/gimptempbuf.c", "app/core/gimpbrush.c", "app/core/gimpbrushgenerated.c", "app/core/gimpdata.c", "app/core/gimpcontext.c", "app/core/gimpimage-undo.c",
          "app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c",
          "app/tests/test-painter-brush-geometry.cpp", "app/tests/painter-brush-geometry-trace.cpp", "app/tests/test-painter-profile-native.cpp",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp", "app/painter/gimp-painter-error.cpp"}
headers = {"app/paint/gimppainterbrushgeometry.h", "app/paint/gimpbrushcore.h", "app/paint/gimppaintoptions.h", "app/paint/gimppaintcore.h", "app/core/gimptooloptions.h", "app/core/gimpbrush.h", "app/core/gimpbrushgenerated.h", "app/core/gimpbrush-private.h",
           "app/paint/painter-mypaint-surface/legacy-generated-mask.hpp", "app/paint/painter-mypaint-surface/legacy-mask-transform.hpp", "app/paint/painter-mypaint-surface/gegl-surface.hpp", "app/paint/gimpbrushcore-kernels.h",
           "app/core/gimp.h", "app/config/gimpcoreconfig.h", "app/operations/operations-enums.h", "app/painter/binding-store.hpp", "app/painter/boundary.hpp", "app/painter/object-ref.hpp"}
instrumented = set(wanted)
rtti_only = bridge_rtti_sources(root, build) - instrumented
wanted |= rtti_only
report["instrumented_sources"] = sorted(instrumented)
report["rtti_compatibility_only_sources"] = sorted(rtti_only)
report["scope"] += "; listed RTTI-only production bridge owners are recompiled for compatible vptr metadata, without sanitizer instrumentation"
hashes={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in wanted|headers}
snapshot = output / "source-snapshot"
for name in sorted(wanted | headers):
    destination = snapshot / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes((root/name).read_bytes())
report["source_snapshot"] = str(snapshot)

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
    if relative not in rtti_only:
        cleaned += flags
    if source.suffix in CXX_SUFFIXES:
        cleaned += ["-frtti"] # consistent shared_ptr COMDAT RTTI for UBSan vptr

    report["sources"].append(relative)
    report["commands"].append(cleaned)
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
report["executables"] = {}
for target in targets:
    link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/" + target], cwd=build, text=True)
    link = shlex.split(link_text.strip().splitlines()[-1])
    exe = output / target
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
    report["executables"][target] = {"path": str(exe), "sha256": hashlib.sha256(exe.read_bytes()).hexdigest()}
changed=[name for name,digest in hashes.items() if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest]
report.update({"sources_sha256":hashes,"changed_during_build":changed,
               "run_status":"Not run by builder; requires native GTK display"})
args.report.write_text(json.dumps(report,indent=2)+"\n")
if changed:raise RuntimeError("Instrumented source changed during the build")
print(json.dumps(report["executables"],indent=2))

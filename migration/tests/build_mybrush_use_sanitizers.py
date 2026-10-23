#!/usr/bin/env python3
"""Build bounded nested-use instrumentation; run on native display.

Hold the selected build environment lock. Private archives leave production untouched.
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
target = "gimp-painter-mybrush"
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "mybrush-use-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Nested-use class hooks and C caller/native GimpData transaction, C++ owner, resource parser/common-store lifecycle and RTTI-consistent native registration units; remaining listed C++ RTTI-compatibility-only; other GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined", "float-cast-overflow"], "leak_detection": False, "instrumented_cpp_rtti": True,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/tests/test-gimp-painter-mybrush.c", "app/tests/test-painter-mybrush-duplicate-handle.cpp",
          "app/core/gimppaintermybrush.cpp", "app/core/gimpdata.c", "app/paint/painter-mypaint/resource.cpp",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp", "app/painter/gimp-painter-error.cpp"}
# Shared BindingStore template/vtable registrations must agree on RTTI.
wanted |= {path.relative_to(root).as_posix() for path in (root / "app").rglob("*.cpp")
           if "/tests/" not in str(path) and '"painter/binding-store.hpp"' in path.read_text()}
instrumented = set(wanted)
rtti_only = bridge_rtti_sources(root, build) - instrumented
wanted |= rtti_only
report["instrumented_sources"] = sorted(instrumented)
report["rtti_compatibility_only_sources"] = sorted(rtti_only)
headers = {"app/core/gimppaintermybrush.h", "app/tests/test-painter-mybrush-use.inc", "app/tests/test-painter-mybrush-live-icon.inc", "app/tests/test-painter-mybrush-save.inc", "app/core/gimpmypaintbrush-save.h",
           "app/tests/test-painter-mybrush-filename.inc", "app/core/gimpmypaintbrush-load.h",
           "app/core/gimppaintermybrush-handle.hpp", "app/core/gimpmypaintbrush.h",
           "app/painter/binding-store.hpp", "app/painter/object-ref.hpp", "app/painter/boundary.hpp"}
hashes={name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in wanted|headers}
replacements = {}
extra = []
archive_replacements = {}
commands = json.loads((build / "compile_commands.json").read_text())
# A source may have unrelated unit-test compile variants. Instrument only
# exact ordinary objects present in this executable's actual link graph;
# do not require or accidentally substitute an unbuilt unrelated variant.
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/gimp-painter-mybrush"], cwd=build, text=True)
ordinary_link = shlex.split(link_text.strip().splitlines()[-1])
linked_objects = {str((build / arg).resolve()) for arg in ordinary_link if arg.endswith(".o")}
for archive in sorted(set(arg for arg in ordinary_link if arg.endswith(".a") and (build / arg).resolve().is_relative_to(build))):
    members = subprocess.check_output(["ar", "t", archive], cwd=build, text=True).splitlines()
    linked_objects.update(str((build / member).resolve()) for member in members)
report["ordinary_link_objects"] = sorted(linked_objects)
report["selected_compile_outputs"] = []
for entry in commands:
    source = (Path(entry["directory"]) / entry["file"]).resolve()
    try:
        relative = source.relative_to(root).as_posix()
    except ValueError:
        continue
    if relative not in wanted:
        continue
    ordinary = (build / entry["output"]).resolve()
    if str(ordinary) not in linked_objects:
        continue
    if not ordinary.is_file():
        raise RuntimeError("Required linked ordinary object is absent: " + str(ordinary))
    report["selected_compile_outputs"].append(entry["output"])
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
missing = wanted - set(report["sources"])
if missing & instrumented:
    raise RuntimeError("Link graph lacks instrumented source(s): " + repr(sorted(missing & instrumented)))
# RTTI candidates are conservatively discovered across the whole application.
# Standalone worker/main-only sources absent from this link graph do not supply
# COMDAT definitions to this executable. Record, rather than claim, those units.
excluded = {}
for relative in sorted(missing):
    variants = [entry["output"] for entry in commands
                if (Path(entry["directory"]) / entry["file"]).resolve() == root / relative]
    if not variants or any(str((build / obj).resolve()) in linked_objects for obj in variants):
        raise RuntimeError("Unresolved linked RTTI source: " + relative)
    excluded[relative] = variants
report["excluded_unlinked_rtti_sources"] = excluded
report["rtti_compatibility_only_sources"] = sorted(rtti_only - missing)
if set(report["sources"]) != wanted - missing:
    raise RuntimeError("Unexpected compiled source population")
executables = {}
for target in ["gimp-painter-mybrush"]:
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

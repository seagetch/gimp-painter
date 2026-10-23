#!/usr/bin/env python3
"""Build focused MyPaint native GTK instrumentation; run on the native display.

Hold /workspace/shared/gimp-painter-build.lock. Private archives leave production untouched.
"""
import hashlib
import sys
sys.path.insert(0, "/workspace/shared/gimp-native-baseline/source/migration/tests")
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
targets = ["painter-popup-handles", "editor-dropdown", "mypaint-selector-contracts"]
build = args.build.resolve()
root = Path("/workspace/shared/gimp-native-baseline/source")
output = build / "popup-signal-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Native popup renderer, editor dropdown, MyPaint selector and BindingStore lifecycle units; remaining GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined", "float-cast-overflow"], "leak_detection": False, "instrumented_cpp_rtti": True,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-O1"]
wanted = {'app/widgets/gimpmypaintbrushoptions-gui.cpp', 'app/core/gimpcontext.c', 'app/core/gimpobject.c', 'app/tests/test-editor-dropdown.cpp', 'app/core/gimpviewable.c', 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp', 'app/paint/painter-mypaint-surface/gimp-painter-session.cpp', 'app/widgets/gimpcontainertreeview.c', 'app/widgets/gimpcontainerview.c', 'app/core/gimpcontainer.c', 'app/widgets/gimpcontainertreestore.c', 'app/widgets/gimppaintermybrusheditor.cpp', 'app/painter/gimp-painter-binding.cpp', 'app/widgets/gimpcellrendererpopup.cpp', 'app/widgets/gimpeditor-cxx.cpp', 'app/tests/test-mypaint-selector-contracts.cpp', 'app/painter/binding-store.cpp', 'app/tests/test-painter-popup-handles.cpp', 'app/tests/test-mypaint-selector-contracts.c', 'app/widgets/gimpeditor.c', 'app/painter/gimp-painter-error.cpp'}
headers = {'app/painter/binding-store.hpp', 'app/painter/connection.hpp', 'app/widgets/gimpcellrendererpopup-handle.hpp', 'app/widgets/gimpeditor-handle.hpp', 'app/painter/object-ref.hpp', 'app/tests/test-mypaint-selector-history.inc'}
instrumented = set(wanted)
rtti_only = bridge_rtti_sources(root, build) - instrumented
wanted |= rtti_only
report["instrumented_sources"] = sorted(instrumented)
report["rtti_compatibility_only_sources"] = sorted(rtti_only)
report["scope"] += "; listed RTTI-only production bridge owners are recompiled for compatible vptr metadata, without sanitizer instrumentation"
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
    if relative not in rtti_only:
        cleaned += flags
    if source.suffix in CXX_SUFFIXES:
        cleaned += ["-frtti"] # consistent shared_ptr COMDAT RTTI for UBSan vptr

    report["sources"].append(relative)
    report["commands"].append(cleaned)
    subprocess.run(cleaned, cwd=build, check=True)
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        archive_replacements[entry["output"]] = str(obj)
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
report["executables"] = []
for target in targets:
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
    report["executables"].append({"target": target, "path":str(exe), "sha256":hashlib.sha256(exe.read_bytes()).hexdigest()})
changed=[name for name,digest in hashes.items() if hashlib.sha256((root/name).read_bytes()).hexdigest()!=digest]
report.update({"sources_sha256":hashes,"changed_during_build":changed,
               "executable":str(exe),"executable_sha256":hashlib.sha256(exe.read_bytes()).hexdigest(),
               "run_status":"Not run by builder; requires native GTK display"})
args.report.write_text(json.dumps(report,indent=2)+"\n")
if changed:raise RuntimeError("Instrumented source changed during the build")
print(exe)

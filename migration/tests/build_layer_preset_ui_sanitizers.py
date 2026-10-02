#!/usr/bin/env python3
"""Instrument layer preset model/applier and core transaction in the existing full GIMP harness.

Run after building app/tests/layer-presets-ui, with the build environment loaded.
Does not claim to instrument all upstream GIMP/dependencies. Original build
objects are never replaced. Hold /tmp/gimp-painter-build.lock when sharing it.
"""
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
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "layer-preset-ui-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Layer preset native JSON model/applier, GTK dock/actions, shared BindingStore, image Undo/group and native UI lifecycle tests; remaining GIMP and dependencies uninstrumented",
          "sanitizers": ["address", "undefined"], "leak_detection": False, "vptr_instrumentation": True,
          "startup_owner_rtti": "MyPaint Options/Session are rebuilt with RTTI to match instrumented BindingStore",
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/paint/painter-mypaint-surface/gimp-painter-options.cpp",
          "app/paint/painter-mypaint-surface/gimp-painter-session.cpp",
          "app/core/gimplayerpreset.c", "app/core/gimplayerpreset-apply.cpp",
          "app/core/gimpimage-undo.c", "app/core/gimpundostack.c", "app/core/gimpitem.c",
          "app/core/gimpgrouplayer.c", "app/tests/test-layer-presets-ui.c", "app/widgets/gimplayerpresetview.c",
          "app/actions/layer-presets-actions.c",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp",
          "app/painter/gimp-painter-error.cpp"}

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
    cleaned += flags
    if source.suffix in (".cpp", ".cc"):
        cleaned += ["-frtti"]

    report["sources"].append(relative)
    report["commands"].append(cleaned)
    subprocess.run(cleaned, cwd=build, check=True)
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        archive_replacements[entry["output"]] = str(obj)
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/layer-presets-ui"], cwd=build, text=True)
link = shlex.split(link_text.strip().splitlines()[-1])
exe = build / "app/tests/layer-presets-ui-asan"
link[link.index("-o") + 1] = str(exe)
link = [replacements.get(arg, arg) for arg in link]
# Replace instrumented members inside private thin archives. Leaving the old
# members available can pull in their RTTI COMDAT symbols and duplicate feature
# definitions when -frtti is used for the vptr sanitizer.
for archive in ["app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a", "app/core/libappcore.a", "app/painter/libapppainter.a", "app/widgets/libappwidgets.a", "app/actions/libappactions.a"]:
    members = subprocess.check_output(["ar", "t", archive], cwd=build, text=True).splitlines()
    rewritten = []
    for member in members:
        member_path = str((build / member).resolve())
        rewritten.append(archive_replacements.get(member, member_path))
    sanitized_archive = output / Path(archive).name
    if sanitized_archive.exists():
        sanitized_archive.unlink()
    command = ["ar", "crsT", str(sanitized_archive)] + rewritten
    report["commands"].append(command)
    subprocess.run(command, cwd=build, check=True)
    link = [str(sanitized_archive) if arg == archive else arg for arg in link]
link[1:1] = flags
report["commands"].append(link)
subprocess.run(link, cwd=build, check=True)
import hashlib
report.update({"executable": str(exe), "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
               "source_sha256": {source: hashlib.sha256((root/source).read_bytes()).hexdigest() for source in wanted},
               "run_status": "Not run by builder; native GTK display required"})
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(exe)

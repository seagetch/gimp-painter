#!/usr/bin/env python3
"""Run focused options/session ASan+UBSan with private full-GIMP archives.

Hold /tmp/gimp-painter-build.lock. Production objects are never replaced.
LeakSanitizer is disabled; nonlisted application/dependency code is uninstrumented.
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
parser.add_argument("--target", choices=["options", "session", "hover", "preview"], required=True)
args = parser.parse_args()
target = "gimp-painter-" + args.target
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / ("mypaint-" + args.target + "-sanitizers")
output.mkdir(exist_ok=True)
report = {"scope": "Native generated options and single-slot session adapter, their reentrant lifecycle tests, painter context/resource ownership, real paint transaction and GEGL Surface; remaining GIMP/dependencies uninstrumented",
          "sanitizers": ["address", "undefined", "float-cast-overflow"], "leak_detection": False, "instrumented_cpp_rtti": True,
          "sources": [], "commands": []}
flags = ["-fsanitize=address,undefined,float-cast-overflow", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/paint/painter-mypaint-surface/gegl-surface.cpp",
          "app/paint/painter-mypaint-surface/gimp-resources.cpp",
          "app/paint/painter-mypaint-surface/paint-core.cpp",
          "app/paint/painter-mypaint-surface/legacy-mask-transform.cpp",
          "app/paint/painter-mypaint-surface/legacy-generated-mask.cpp",
          "app/paint/gimppaintcore.c", "app/core/gimpdrawable.c", "app/core/gimpbrush.c",
          "app/core/gimppattern.c", "app/core/gimpdata.c",
          "app/core/gimpimage.c", "app/core/gimpitem.c", "app/core/gimpimage-undo.c", "app/core/gimpviewable.c",
          "app/core/gimpobject.c", "app/core/gimpresource.c", "app/tests/test-" + target + ".cpp", "app/core/gimpcontext.c", "app/core/gimppaintermybrush.cpp",
          "app/paint/painter-mypaint-surface/gimp-painter-options.cpp",
          "app/paint/painter-mypaint-surface/gimp-painter-session.cpp",
          "app/paint/painter-mypaint/resource.cpp", "app/paint/painter-mypaint/engine.cpp",
          "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp", "app/painter/gimp-painter-error.cpp"}
if args.target == "preview":
    wanted |= {"app/core/gimpbrushpipe.c", "app/core/gimpbrushgenerated.c"}
    report["scope"] += "; preview additionally instruments native pipe/generated duplication and isolated selector tests"
hashes = {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in wanted}
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
    if source.suffix == ".cpp":
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
env = dict(os.environ)
env.update({"GIMP_TESTING_ABS_TOP_SRCDIR": str(root),
            "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build),
            "GIMP_TESTING_PLUGINDIRS": str(build / "plug-ins/common"),
            "UI_TEST": "yes", "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
result = subprocess.run([str(exe)], cwd=build, env=env, capture_output=True, text=True)
changed = [name for name, digest in hashes.items()
           if hashlib.sha256((root / name).read_bytes()).hexdigest() != digest]
report.update({"exit_code": result.returncode, "stdout": result.stdout, "stderr": result.stderr,
               "sources_sha256": hashes, "changed_during_run": changed,
               "executable_sha256": hashlib.sha256(exe.read_bytes()).hexdigest()})
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(result.stdout)
if result.returncode:
    print(result.stderr, file=sys.stderr)
sys.exit(result.returncode or bool(changed))

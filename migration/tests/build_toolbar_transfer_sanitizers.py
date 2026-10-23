#!/usr/bin/env python3
"""Build nine C toolbar/action transaction units with ASan/UBSan.

Hold the shared build lock; run only after ordinary toolbar target build.
No C++ RTTI changes, original objects untouched. All other application units
and dependencies remain uninstrumented; LSan disabled in the runtime driver.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "painter-toolbar-transfer-sanitizers"
output.mkdir(exist_ok=True)
report = {"scope": "Nine C units: image window, action, action group, action implementation, toggle/radio action, menu, UI manager and real toolbar fixture. Other app/dependency units uninstrumented; LSan off",
          "sanitizers": ["address", "undefined"], "leak_detection": False,
          "sources": [], "commands": [], "run_status": "not run by builder"}
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
wanted = {"app/display/gimpimagewindow.c", "app/widgets/gimpaction.c",
          "app/widgets/gimpactiongroup.c", "app/widgets/gimpactionimpl.c",
          "app/widgets/gimptoggleaction.c", "app/widgets/gimpradioaction.c",
          "app/widgets/gimpmenu.c", "app/widgets/gimpuimanager.c",
          "app/tests/test-painter-toolbar-window.c"}
headers = {"app/widgets/gimpaction.h", "app/display/gimppaintercanvasui.h",
           "app/tests/test-painter-toolbar-transfer.inc"}
source_hashes = {p: hashlib.sha256((root/p).read_bytes()).hexdigest() for p in wanted | headers}
replacements = {}
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

    report["sources"].append(relative)
    report["commands"].append(cleaned)
    subprocess.run(cleaned, cwd=build, check=True)
    if relative.startswith("app/tests/"):
        replacements[entry["output"]] = str(obj)
    else:
        archive_replacements[entry["output"]] = str(obj)
if set(report["sources"]) != wanted:
    raise RuntimeError("Compile database lacks required source(s)")
link_text = subprocess.check_output(["ninja", "-t", "commands", "app/tests/painter-toolbar-window"], cwd=build, text=True)
link = shlex.split(link_text.strip().splitlines()[-1])
exe = build / "app/tests/painter-toolbar-window-asan"
link[link.index("-o") + 1] = str(exe)
link = [replacements.get(arg, arg) for arg in link]
# Replace instrumented members inside private thin archives; do not leave
# original definitions available for the linker to select instead.
for archive in ["app/display/libappdisplay.a", "app/widgets/libappwidgets.a"]:
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
report["executable"] = str(exe)
report["source_sha256"] = source_hashes
assert all(hashlib.sha256((root/p).read_bytes()).hexdigest() == h for p,h in source_hashes.items())
report["executable_sha256"] = hashlib.sha256(exe.read_bytes()).hexdigest()
report["source_changes_during_build"] = []
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(exe)

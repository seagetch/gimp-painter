#!/usr/bin/env python3
"""Build private Painter GTK controller/action ASan+UBSan/vptr coverage.

Build the normal app/tests/painter-layer-ui first and hold
/workspace/shared/gimp-painter-build.lock throughout this builder. It does not
start GTK or replace production objects. Run on the native display with
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 plus the regular test environment.
Only the explicitly listed units are instrumented; dependencies are not.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shlex
import subprocess
import tarfile
from painter_sanitizer_scope import bridge_rtti_sources, CXX_SUFFIXES

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
build = args.build.resolve()
root = Path(__file__).resolve().parents[2]
output = build / "painter-ui-sanitizers"
output.mkdir(exist_ok=True)
flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
instrumented = {
    "app/dialogs/painter-layer-dialog.cpp", "app/actions/layers-actions.c",
    "app/actions/layers-commands.c", "app/tests/test-painter-layer-ui.c",
    "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp",
    "app/painter/gimp-painter-error.cpp",
}
rtti_only = bridge_rtti_sources(root, build) - instrumented
wanted = instrumented | rtti_only
headers = {
    "app/dialogs/painter-layer-dialog.h", "app/painter/binding-store.hpp",
    "app/painter/object-ref.hpp", "app/painter/connection.hpp",
    "app/painter/boundary.hpp", "app/painter/gimp-painter-binding.h",
    "app/painter/gimp-painter-error.h", "app/core/gimp.h",
    "app/core/gimpclonelayer.h", "app/core/gimpfilterlayer.h",
    "app/widgets/gimpviewabledialog.h", "app/dialogs/meson.build",
    "app/tests/meson.build", "po/POTFILES.in",
    "app/tests/test-isolated-filter-editors.inc",
    "migration/tests/build_painter_ui_sanitizers.py",
    "migration/tests/painter_sanitizer_scope.py",
}
headers |= {str(Path(source).with_suffix(suffix)) for source in rtti_only
            for suffix in [".h", ".hpp"]
            if (root / Path(source).with_suffix(suffix)).exists()}
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
source_bytes = {name: (root / name).read_bytes() for name in sorted(wanted | headers)}
hashes = {name: hashlib.sha256(data).hexdigest() for name, data in source_bytes.items()}
snapshot_id = hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()
archive = args.report.resolve().parent / ("layer-dialog-store-sources-" + snapshot_id[:16] + ".tar.gz")
if not archive.exists():
    with tarfile.open(archive, "w:gz") as stream:
        for name, data in source_bytes.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            info.mode = 0o644
            stream.addfile(info, io.BytesIO(data))
with tarfile.open(archive, "r:gz") as stream:
    archived = {member.name: hashlib.sha256(stream.extractfile(member).read()).hexdigest()
                for member in stream.getmembers() if member.isfile()}
    if archived != hashes:
        raise RuntimeError("Existing source archive does not match its content identity")
report = {
    "scope": "Painter GTK dialog/controller, Layers actions/commands, common BindingStore lifecycle and UI tests instrumented; other listed production C++ is RTTI-only; GTK and other core/dependencies uninstrumented; leak detection disabled",
    "sanitizers": ["address", "undefined"], "leak_detection": False,
    "instrumented_sources": sorted(instrumented),
    "rtti_compatibility_only_sources": sorted(rtti_only),
    "sources_sha256": hashes, "source_archive": str(archive),
    "source_archive_sha256": sha(archive), "commands": [],
    "build_status": "building", "run_status": "not run by builder",
}
args.report.write_text(json.dumps(report, indent=2) + "\n")
entries = json.loads((build / "compile_commands.json").read_text())
objects, originals = {}, {}
try:
    for source in sorted(wanted):
        candidates = [entry for entry in entries
                      if (Path(entry["directory"]) / entry["file"]).resolve() == root / source]
        if not candidates:
            raise RuntimeError("Compile database lacks " + source)
        entry = next((e for e in candidates if e["output"].startswith(str(Path(source).parent) + "/")), candidates[0])
        cleaned, skip = [], False
        for arg in shlex.split(entry["command"]):
            if skip:
                skip = False
                continue
            if arg in ("-MF", "-MQ", "-MT"):
                skip = True
                continue
            if arg not in ("-MD", "-MMD"):
                cleaned.append(arg)
        obj = output / (source.replace("/", "_") + ".o")
        cleaned[cleaned.index("-o") + 1] = str(obj)
        if source in instrumented:
            cleaned += flags
        if Path(source).suffix in CXX_SUFFIXES:
            cleaned += ["-frtti"]
        report["commands"].append(cleaned)
        key = hashlib.sha256(json.dumps({
            "command": cleaned, "source": hashes[source],
            "headers": {name: hashes[name] for name in headers},
            "ordinary_object": sha(build / entry["output"]),
        }, sort_keys=True).encode()).hexdigest()
        keyfile = obj.with_suffix(".key")
        if not (obj.exists() and keyfile.exists() and keyfile.read_text() == key):
            subprocess.run(cleaned, cwd=build, check=True)
            keyfile.write_text(key)
        objects[source] = str(obj)
        originals[entry["output"]] = source

    absolute = {str((build / path).resolve()): objects[source]
                for path, source in originals.items()}
    link = shlex.split(subprocess.check_output(
        ["ninja", "-t", "commands", "app/tests/painter-layer-ui"], cwd=build,
        text=True).strip().splitlines()[-1])
    exe = build / "app/tests/painter-layer-ui-asan"
    link[link.index("-o") + 1] = str(exe)
    # Replace every alias, including Meson's flattened link_whole archives, so
    # a stale non-RTTI entry cannot be pulled back in by an instrumented caller.
    for original_archive in sorted({arg for arg in link if arg.endswith(".a")
                                    and (build / arg).resolve().is_relative_to(build)}):
        members = [str((build / member).resolve()) for member in subprocess.check_output(
            ["ar", "t", original_archive], cwd=build, text=True).splitlines()]
        if not any(member in absolute for member in members):
            continue
        destination = output / original_archive.replace("/", "_")
        if destination.exists():
            destination.unlink()
        command = ["ar", "crsT", str(destination)] + [absolute.get(member, member) for member in members]
        report["commands"].append(command)
        subprocess.run(command, cwd=build, check=True)
        link = [str(destination) if arg == original_archive else arg for arg in link]
    link = [objects[originals[arg]] if arg in originals and originals[arg].startswith("app/tests/")
            else arg for arg in link]
    link[1:1] = flags
    report["commands"].append(link)
    subprocess.run(link, cwd=build, check=True)
    report["executable"] = str(exe)
    report["executable_sha256"] = sha(exe)
    report["changed_during_build"] = [name for name, digest in hashes.items() if sha(root / name) != digest]
    if report["changed_during_build"]:
        raise RuntimeError("Sources changed during build: " + repr(report["changed_during_build"]))
    report["build_status"] = "passed"
except Exception as error:
    report["build_status"] = "failed"
    report["build_error"] = str(error)
    raise
finally:
    args.report.write_text(json.dumps(report, indent=2) + "\n")
print(exe)

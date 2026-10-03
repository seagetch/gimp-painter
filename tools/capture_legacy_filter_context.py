#!/usr/bin/env python3
"""Build a private observation-only old core harness and capture native bytes.

Source the legacy env.sh first. All oracle transformations remain the pinned old
application/plugin code. Hooks count starts/completions, capture native buffers,
and apply a requested context edit at an actual old PDB progress request. The
normal no-interface progress forwarding guard remains unchanged.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import difflib
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"
SOURCES = (
    "app/core/gimpfilterlayer.cpp", "app/core/gimpdrawable-shadow.c",
    "app/core/gimpdrawable-combine.c", "app/core/gimpitem.c", "app/core/gimplayer.c",
    "app/core/gimpimage.c", "app/paint-funcs/paint-funcs.c",
    "app/paint-funcs/paint-funcs-utils.h", "app/pdb/progress-cmds.c",
    "plug-ins/common/blinds.c", "app/pdb/pdb-cxx-utils.hpp",
)


def sha(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError("Instrumentation anchor is not unique: " + old[:80])
    return text.replace(old, new)


def instrument(source: Path, work: Path, out: Path) -> list[Path]:
    originals = {name: (source / name).read_text() for name in
                 ("app/core/gimpfilterlayer.cpp", "app/pdb/progress-cmds.c", "app/core/gimpdrawable-shadow.c")}
    edits = dict(originals)
    key = "app/core/gimpfilterlayer.cpp"
    edits[key] = replace_once(edits[key], "        bool result = runner->run(GIMP_ITEM(g_object));",
        '        extern void capture_context_start_cpp (GimpDrawable *);\n'
        '        capture_context_start_cpp (GIMP_DRAWABLE(g_object));\n'
        "        bool result = runner->run(GIMP_ITEM(g_object));")
    edits[key] = replace_once(edits[key], "  notify_filter_end();\n}\n\ngboolean GLib::FilterLayer::is_active()",
        '  notify_filter_end();\n  extern void capture_context_end_cpp (GimpDrawable *);\n'
        '  capture_context_end_cpp (GIMP_DRAWABLE(g_object));\n}\n\ngboolean GLib::FilterLayer::is_active()')
    edits[key] += '''
extern "C" void capture_context_start (GimpDrawable *);
extern "C" void capture_context_end (GimpDrawable *);
namespace GLib {
void capture_context_start_cpp (GimpDrawable *d) { capture_context_start (d); }
void capture_context_end_cpp (GimpDrawable *d) { capture_context_end (d); }
}
extern "C" gint gimp_filter_layer_capture_pending (GimpFilterLayer *layer)
{
  auto *impl = dynamic_cast<GLib::FilterLayer *>(FilterLayerInterface::cast (layer));
  return (impl->is_waiting_to_be_processed () ? 1 : 0) |
         (impl->waiting_for_runner ? 2 : 0);
}
'''
    key = "app/pdb/progress-cmds.c"
    edits[key] = replace_once(edits[key], "  percentage = g_value_get_double (&args->values[0]);",
        '  percentage = g_value_get_double (&args->values[0]);\n'
        '  { extern void capture_context_progress (Gimp *, gdouble);\n'
        '    capture_context_progress (gimp, percentage); }')
    key = "app/core/gimpdrawable-shadow.c"
    edits[key] = replace_once(edits[key], "  /*  A useful optimization here is to limit the update to the",
        '  { extern void capture_context_merge (GimpDrawable *, gboolean);\n'
        '    capture_context_merge (drawable, TRUE); }\n\n'
        "  /*  A useful optimization here is to limit the update to the")
    edits[key] = replace_once(edits[key], "      tile_manager_unref (tiles);\n    }\n}",
        '      tile_manager_unref (tiles);\n    }\n'
        '  { extern void capture_context_merge (GimpDrawable *, gboolean);\n'
        '    capture_context_merge (drawable, FALSE); }\n}')
    patches = []
    paths = []
    for name, edited in edits.items():
        path = work / Path(name).name
        path.write_text(edited)
        paths.append(path)
        patches.extend(difflib.unified_diff(originals[name].splitlines(True), edited.splitlines(True),
                                           fromfile="a/" + name, tofile="b/" + name))
    (out / "observation-hooks.patch").write_text("".join(patches))
    return paths


def build(source: Path, work: Path, out: Path) -> tuple[Path, list[list[str]]]:
    cwd = source / "app/tests"
    dry = subprocess.check_output(["make", "-n", "-W", "test-xcf.c", "test-xcf"], cwd=cwd, text=True)
    compile_line = next(line for line in dry.splitlines() if " -c -o test-xcf.o " in line)
    flags = shlex.split(compile_line[compile_line.index("/usr/bin/gcc "):].split(" -MT ")[0])[1:]
    includes = [flag for flag in flags if flag.startswith(("-I", "-D")) or flag == "-pthread"]
    includes += ["-I" + str(source / "app/core"), "-I" + str(source / "app/pdb")]
    paths = instrument(source, work, out)
    commands = []
    objects = []
    harness = ROOT / "migration/tests/legacy-filter-context-capture.c"
    for path in [harness] + paths:
        output = work / (path.stem + ".o")
        cxx = path.suffix == ".cpp"
        command = (["g++", "-std=c++17", "-include", "type_traits"] if cxx else ["gcc"]) + includes
        command += ["-g", "-O1", "-fcommon", "-c", str(path), "-o", str(output)]
        commands.append(command)
        objects.append(str(output))
    link_line = next(line for line in dry.splitlines() if "--mode=link " in line)
    command = shlex.split(link_line[link_line.index("/bin/bash "):])
    index = command.index("-o")
    executable = work / "legacy-filter-context"
    command[index:index + 3] = ["-o", str(executable)] + objects
    # The old test target predates Painter's preset archive. Match the actual
    # old application link: widgets/actions reference these implementations.
    command.insert(command.index("../../app/core/libappcore.a"), "../../app/presets/libapppresets.a")
    command += ["-lstdc++"]
    commands.append(command)
    with (out / "build.log").open("w") as log:
        for command in commands:
            log.write(shlex.join(command) + "\n")
            log.flush()
            subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
    return executable, commands


def run_capture(executable: Path, prefix: Path, out: Path, timeout: float) -> tuple[int, list[str]]:
    with tempfile.TemporaryDirectory(prefix="gimp-old-context-profile-") as tmp:
        home = Path(tmp)
        for name in ("profile", "plugins", "empty", "temp", "cache", "config", "data"):
            (home / name).mkdir(mode=0o700)
        (home / "plugins/blinds").symlink_to(prefix / "lib/gimp/2.0/plug-ins/blinds")
        (home / "profile/gimprc").write_text("".join(
            f'({name} {json.dumps(str(home / path))})\n' for name, path in
            (("plug-in-path", "plugins"), ("module-path", "empty"), ("interpreter-path", "empty"),
             ("environ-path", "empty"), ("temp-path", "temp"), ("swap-path", "temp"))))
        env = dict(os.environ)
        for key in tuple(env):
            if key.startswith("GIMP_TESTING_") or key in ("GIMP_PLUGIN_DEBUG", "GIMP_PLUGIN_DEBUG_WRAP"):
                del env[key]
        env.update(HOME=str(home), GIMP2_DIRECTORY=str(home / "profile"),
                   XDG_CONFIG_HOME=str(home / "config"), XDG_CACHE_HOME=str(home / "cache"),
                   XDG_DATA_HOME=str(home / "data"))
        command = [str(executable), str(out)]
        with (out / "capture.log").open("w") as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        return result.returncode, command


def validate(out: Path, code: int) -> dict:
    errors = []
    if code:
        errors.append("Native harness returned " + str(code))
    if "LEGACY_FILTER_CONTEXT_CAPTURE_COMPLETE" not in (out / "capture.log").read_text(errors="replace"):
        errors.append("Missing successful completion marker")
    merges = [line.split("\t") for line in (out / "merges.tsv").read_text().splitlines()]
    for row in merges:
        ident, width, height, channels, _, empty = row
        pixels = int(width) * int(height)
        for label in ("input", "shadow", "mask", "output"):
            path = out / (ident + "-" + label + ".raw")
            expected = pixels if label == "mask" else pixels * int(channels)
            if not path.is_file() or path.stat().st_size != expected:
                errors.append("Missing/invalid " + path.name)
        if ident.startswith("native-"):
            selection_kind = int(ident.split("-s")[1].split("-")[0])
            if bool(int(empty)) != (selection_kind == 0):
                errors.append("Selection's native cached bounds disagree with fixture " + ident)
            mask = (out / (ident + "-mask.raw")).read_bytes()
            if selection_kind == 1 and (0 not in mask or 255 not in mask):
                errors.append("Hard selection lacks both covered and uncovered pixels " + ident)
            if selection_kind == 2 and not any(0 < value < 255 for value in mask):
                errors.append("Soft selection lacks partial coverage " + ident)
            if selection_kind == 3 and any(mask):
                errors.append("Outside selection intersects the target " + ident)
    events = [line.split("\t") for line in (out / "events.tsv").read_text().splitlines()]
    differences = {}
    for mode in ("idle", "running"):
        for kind in range(1, 7):
            ident = f"{mode}-k{kind}"
            rows = [row for row in events if row[0] == ident]
            if not any(row[1:4] == ["finished", "2", "2"] for row in rows):
                errors.append("Missing two-job completion " + ident)
            if mode == "running" and not any(row[1] == "progress-mutate" and 0 < float(row[4]) < 1 for row in rows):
                errors.append("Missing real mid-PDB progress mutation " + ident)
            settled = out / (ident + "-settled.raw")
            next_file = out / (ident + ("-after-context.raw" if mode == "idle" else "-rerun.raw"))
            if settled.is_file() and next_file.is_file():
                differences[ident] = sum(a != b for a, b in zip(settled.read_bytes(), next_file.read_bytes()))
                if mode == "idle" and differences[ident]:
                    errors.append("Idle context mutated settled cache " + ident)
                if mode == "running" and kind in (1, 2, 6) and not differences[ident]:
                    errors.append("Fixture failed to distinguish start ROI/background from final merge " + ident)
    return dict(status="passed" if not errors else "failed", errors=errors,
                native_merge_count=sum(row[0].startswith("native-") for row in merges),
                live_merge_count=sum(not row[0].startswith("native-") for row in merges),
                context_difference_bytes=differences)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "migration/fixtures/legacy-filter-context")
    parser.add_argument("--legacy-source", type=Path, default=ROOT.parent / "gimp-painter-legacy")
    parser.add_argument("--legacy-prefix", type=Path, default=Path("/workspace/shared/gimp-legacy-build/prefix"))
    parser.add_argument("--work", type=Path, default=Path("/workspace/shared/gimp-legacy-context-build"))
    parser.add_argument("--lock", type=Path, default=Path("/workspace/shared/gimp-painter-build.lock"))
    parser.add_argument("--timeout", type=float, default=180)
    args = parser.parse_args()
    source, out, work = args.legacy_source.resolve(), args.output.resolve(), args.work.resolve()
    if (out / "capture-report.json").exists():
        raise SystemExit("Sealed report already exists; use a new --output")
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if commit != COMMIT:
        raise SystemExit("Wrong old source commit")
    hashes = {}
    for name in SOURCES:
        hashes[name] = sha(source / name)
        pinned = subprocess.check_output(["git", "-C", str(source), "show", COMMIT + ":" + name])
        if hashlib.sha256(pinned).hexdigest() != hashes[name]:
            raise SystemExit("Changed oracle transformation source " + name)
    out.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    (out / ".gitattributes").write_text("*.raw binary\n")
    if not args.capture:
        instrument(source, work, out)
        print("Prepared observation hooks; --capture builds and runs under the shared lock")
        return 0
    started = datetime.now(timezone.utc).isoformat()
    with args.lock.open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        executable, commands = build(source, work, out)
        native_binary = work / ".libs/legacy-filter-context"
        if not native_binary.is_file():
            native_binary = executable
        oracle_binaries = [args.legacy_prefix / "bin/gimp-2.8", args.legacy_prefix / "lib/gimp/2.0/plug-ins/blinds"]
        executable_hashes = {str(path): sha(path) for path in [native_binary] + oracle_binaries}
        code, command = run_capture(executable, args.legacy_prefix, out, args.timeout)
    report = validate(out, code)
    report.update(schema_version=1, source_commit=commit, source_sha256=hashes,
                  started_utc=started, completed_utc=datetime.now(timezone.utc).isoformat(),
                  executable_sha256=executable_hashes, build_commands=commands, command=command,
                  exit_code=code, instrumentation_sha256=sha(out / "observation-hooks.patch"),
                  harness_sha256=sha(ROOT / "migration/tests/legacy-filter-context-capture.c"),
                  capture_script_sha256=sha(Path(__file__)),
                  files_sha256={path.name: sha(path) for path in sorted(out.iterdir()) if path.is_file()})
    (out / "capture-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"], report["native_merge_count"], report["live_merge_count"], report["errors"])
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

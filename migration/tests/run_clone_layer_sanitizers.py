#!/usr/bin/env python3
"""Instrument the focused Clone sources without replacing normal build objects.

The optional --output-dir puts both objects and executable in a fresh separate
folder. Default output locations remain compatible with earlier invocations.
Hold the shared build lock throughout the enclosing build/test evidence run.
"""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import sys
import runpy

RTTI_SCOPE = runpy.run_path(str(Path(__file__).with_name("painter_sanitizer_scope.py")))
lexical_path = RTTI_SCOPE["lexical_path"]

SOURCES = frozenset({
    "app/core/gimpitem.c", "app/core/gimpclonelayer.cpp", "app/core/gimpgrouplayer.c",
    "app/tests/test-gimp-clone-layer.c", "app/tests/test-gimp-clone-layout.cpp",
    "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp",
    "app/painter/gimp-painter-error.cpp"})
FLAGS = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]


def compile_command(entry, obj, instrumented=True):
    cmd = list(entry.get("arguments") or shlex.split(entry["command"]))
    cleaned, skip = [], False
    for arg in cmd:
        if skip:
            skip = False
            continue
        if arg in ("-MF", "-MQ", "-MT"):
            skip = True
            continue
        if arg in ("-MD", "-MMD", "-MP", "-MG") or any(
                arg.startswith(flag) and arg != flag for flag in ("-MF", "-MQ", "-MT")):
            continue
        cleaned.append(arg)
    cleaned[cleaned.index("-o") + 1] = str(obj)
    flags = FLAGS if instrumented else []
    if Path(entry["file"]).suffix in RTTI_SCOPE["CXX_SUFFIXES"]:
        flags = flags + ["-frtti"]
    return cleaned + flags



def object_name(source):
    return source.replace("/", "_") + ".o"


def archive_arguments(link_text, build):
    argv = shlex.split(link_text.strip().splitlines()[-1])
    return sorted({arg for arg in argv if arg.endswith(".a") and
                   lexical_path(build / arg).is_relative_to(build)})


def build_plan(root, build, output, entries, link_text, archive_members):
    """Select exact linked production members, never standalone test aliases."""
    root, build, output = map(lexical_path, (root, build, output))
    argv = shlex.split(link_text.strip().splitlines()[-1])
    if set(archive_members) != set(archive_arguments(link_text, build)):
        raise RuntimeError("Missing or extra linked archive member inventory")
    linked = {lexical_path(build / arg) for arg in argv if arg.endswith(".o")}
    linked.update(lexical_path(member) for members in archive_members.values() for member in members)
    rtti = RTTI_SCOPE["bridge_rtti_sources"](root, build, entries=entries, resolve_paths=False)
    selected = {}
    for index, entry in enumerate(entries):
        source = lexical_path(Path(entry["directory"]) / entry["file"])
        if not source.is_relative_to(root):
            continue
        relative = source.relative_to(root).as_posix()
        if relative not in SOURCES | rtti:
            continue
        original = list(entry.get("arguments") or shlex.split(entry["command"]))
        original_object = lexical_path(Path(entry["directory"]) /
                           (entry.get("output") or original[original.index("-o") + 1]))
        if original_object not in linked:
            continue
        if relative in selected:
            raise RuntimeError("Ambiguous linked production compilation: " + relative)
        obj = output / object_name(relative)
        selected[relative] = {"source": relative, "compile_database_index": index,
            "instrumented": relative in SOURCES, "original_object": str(original_object),
            "object": str(obj), "command": compile_command(entry, obj, relative in SOURCES)}
    if not SOURCES.issubset(selected):
        raise RuntimeError("Compile/link inputs lack required instrumented sources")
    jobs = [selected[source] for source in sorted(selected)]
    if len({job["original_object"] for job in jobs}) != len(jobs):
        raise RuntimeError("Duplicate selected original object identities")
    absolute = {job["original_object"]: job["object"] for job in jobs}
    anchored_originals = {job["original_object"] for job in jobs if job["instrumented"]}
    anchors = [job["object"] for job in jobs if job["instrumented"]]
    replacements, archives = {}, []
    for arg in archive_arguments(link_text, build):
        members = archive_members[arg]
        if not members or len(members) != len(set(members)):
            raise RuntimeError("Empty or duplicated original archive members")
        changed = any(member in absolute for member in members)
        replacement = str(output / ("archive-" + arg.replace("/", "_"))) if changed else None
        mapped = [absolute.get(member, member) for member in members if member not in anchored_originals]
        archives.append({"argument": arg, "original_path": str(lexical_path(build / arg)),
            "members": members, "replacement": replacement, "replacement_members": mapped,
            "command": ["ar", "crsT", replacement, *mapped] if changed else None})
        if changed:
            replacements[arg] = replacement
    for arg in argv:
        absolute_arg = str(lexical_path(build / arg))
        if absolute_arg in absolute:
            replacements[arg] = None if absolute_arg in anchored_originals else absolute[absolute_arg]
    linked_rtti = set(selected) - SOURCES
    return {"compilations": jobs, "archives": archives, "replacements": replacements,
            "instrumented_anchors": anchors,
            "instrumented_sources": sorted(SOURCES),
            "rtti_compatibility_only_sources": sorted(linked_rtti),
            "excluded_nonlinked_rtti_sources": sorted(rtti - set(selected))}


def output_paths(build, explicit):
    output = explicit.resolve() if explicit else build / "clone-sanitizers"
    exe = output / "gimp-clone-layer-asan" if explicit else build / "app/tests/gimp-clone-layer-asan"
    return output, exe



def link_command(link_text, build, exe, replacements, extra, relocate_origin):
    link = shlex.split(link_text.strip().splitlines()[-1])
    original_exe = lexical_path(build / link[link.index("-o") + 1])
    if relocate_origin:
        # Preserve the normal executable's loader resolution after relocation.
        # Expand ORIGIN only; other dynamic loader tokens keep their semantics.
        origin = str(original_exe.parent)
        link = [arg.replace("${ORIGIN}", origin).replace("$ORIGIN", origin) for arg in link]
    link[link.index("-o") + 1] = str(exe)
    link = [replacements.get(arg, arg) for arg in link if replacements.get(arg, arg) is not None]
    link[1:1] = FLAGS + extra
    return link



def gimp_library_paths(output):
    if "not found" in output:
        raise RuntimeError("Dynamic loader reports an unresolved library")
    libraries = {}
    for line in output.splitlines():
        match = re.match(r"\s*(libgimp[^\s]+)\s+=>\s+(/.*?)\s+\(0x[0-9a-fA-F]+\)\s*$", line)
        if match:
            if match[1] in libraries:
                raise RuntimeError("Duplicate dynamic library resolution")
            # Keep the loader's spelling for live resolution (symlink/.. matters).
            # Offline validation normalizes only against its sealed inventory.
            libraries[match[1]] = match[2]
    if not libraries:
        raise RuntimeError("No current-build libgimp loader resolutions found")
    return libraries


def library_resolution(normal_output, sanitizer_output, build):
    # Native execution still resolves actual aliases and hashes their real ELF.
    def live_paths(text):
        try:
            return {name: str(Path(path).resolve(strict=True))
                    for name, path in gimp_library_paths(text).items()}
        except OSError as error:
            raise RuntimeError("Dynamic loader path is absent from the live build") from error
    normal, sanitizer = live_paths(normal_output), live_paths(sanitizer_output)
    if normal != sanitizer:
        raise RuntimeError("Normal and sanitizer resolve different libgimp libraries")
    result = {}
    for name, location in normal.items():
        path = Path(location)
        if not path.is_relative_to(build) or not path.is_file():
            raise RuntimeError("libgimp dependency does not resolve to the sealed current build")
        result[name] = {"path": location, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--test-path", action="append", default=[],
                        help="Run only an exact GLib test path (repeatable)")
    args = parser.parse_args(argv)
    build = args.build.resolve()
    root = Path(__file__).resolve().parents[2]
    output, exe = output_paths(build, args.output_dir)
    report = {"scope": "CloneLayer, GimpItem lifetime, group duplication, C/C++ test adapters, and shared BindingStore; remaining GIMP/dependencies uninstrumented",
              "sanitizers": ["address", "undefined"], "leak_detection": False,
              "sources": [], "rtti_compatibility_only_sources": [], "commands": [], "executions": [], "exit_code": None,
              "stdout": "", "stderr": "", "executable": str(exe)}

    def save():
        args.report.write_text(json.dumps(report, indent=2) + "\n")

    def run(command, environment=None):
        record = {"command": command, "cwd": str(build), "exit_code": None,
                  "stdout": "", "stderr": ""}
        report["commands"].append(command)
        report["executions"].append(record)
        save()
        result = subprocess.run(command, cwd=build, env=environment, capture_output=True, text=True)
        record.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr)
        save()
        if result.returncode:
            raise RuntimeError("Command failed with exit " + str(result.returncode))
        return result

    try:
        if args.output_dir and (output.is_relative_to(build) or output.is_relative_to(root)):
            raise RuntimeError("Explicit sanitizer output must be outside normal build and source")
        output.mkdir(exist_ok=not bool(args.output_dir), mode=0o700)
        entries = json.loads((build / "compile_commands.json").read_text())
        link_text = run(["ninja", "-t", "commands", "app/tests/gimp-clone-layer"]).stdout
        archive_members, original_archives = {}, {}
        for archive in archive_arguments(link_text, build):
            original = lexical_path(build / archive)
            original_archives[archive] = hashlib.sha256(original.read_bytes()).hexdigest()
            listing = run(["ar", "t", archive]).stdout
            archive_members[archive] = [str(lexical_path(build / member)) for member in listing.splitlines()]
        plan = build_plan(root, build, output, entries, link_text, archive_members)
        report["build_plan"] = plan
        report["sources"] = plan["instrumented_sources"]
        report["rtti_compatibility_only_sources"] = plan["rtti_compatibility_only_sources"]
        report["original_archive_sha256_before"] = original_archives
        report["scope"] = (str(len(plan["instrumented_sources"])) + " ASan/UBSan-instrumented sources; " +
            str(len(plan["rtti_compatibility_only_sources"])) +
            " linked production RTTI-only compatibility sources; all remaining GIMP/dependencies uninstrumented; LSan disabled")
        save()
        for job in plan["compilations"]:
            run(job["command"])
        for archive in plan["archives"]:
            if archive["command"] is not None:
                run(archive["command"])
        link = link_command(link_text, build, exe, plan["replacements"], plan["instrumented_anchors"], bool(args.output_dir))
        report["origin_relocation"] = "expand-against-original-executable-directory" if args.output_dir else "unchanged"
        run(link)
        normal_loader = run(["ldd", str(build / "app/tests/gimp-clone-layer")])
        sanitizer_loader = run(["ldd", str(exe)])
        report["library_resolution"] = library_resolution(normal_loader.stdout, sanitizer_loader.stdout, build)
        save()
        env = dict(os.environ)
        env.update({"GIMP_TESTING_ABS_TOP_SRCDIR": str(root),
                    "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build),
                    "GIMP_TESTING_PLUGINDIRS": str(build / "plug-ins/common"),
                    "UI_TEST": "yes", "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1",
                    "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
        def artifacts():
            paths = sorted(path for path in output.iterdir() if path.is_file())
            if exe not in paths:
                paths.append(exe)
            return {path.name: {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                                "bytes": path.stat().st_size,
                                "mode": oct(path.stat().st_mode & 0o777)} for path in paths}
        report["artifacts_before_run"] = artifacts()
        save()
        try:
            test_command = [str(exe)]
            for test_path in args.test_path:
                test_command += ["-p", test_path]
            report["test_command"] = test_command
            result = run(test_command, env)
        finally:
            report["artifacts_after_run"] = artifacts()
            save()
        report["original_archive_sha256_after"] = {name: hashlib.sha256((build / name).read_bytes()).hexdigest()
                                                   for name in original_archives}
        if report["original_archive_sha256_after"] != original_archives:
            raise RuntimeError("Original normal-build archives changed")
        report.update(exit_code=0, stdout=result.stdout, stderr=result.stderr)
    except (Exception, KeyboardInterrupt) as error:
        last = report["executions"][-1] if report["executions"] else {}
        report.update(exit_code=last.get("exit_code") or 1,
                      stdout=last.get("stdout", ""), stderr=last.get("stderr", ""),
                      error=f"{type(error).__name__}: {error}")
    finally:
        if "original_archive_sha256_before" in report:
            report["original_archive_sha256_after"] = {name: hashlib.sha256((build / name).read_bytes()).hexdigest()
                                                       for name in report["original_archive_sha256_before"]}
        save()
    print(report["stdout"], end="")
    print(report["stderr"], end="", file=sys.stderr)
    return report["exit_code"]


if __name__ == "__main__":
    sys.exit(main())

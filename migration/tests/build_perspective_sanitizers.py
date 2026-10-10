#!/usr/bin/env python3
"""Build focused model/image/Undo/overlay/tool sanitizer binaries privately.

Hold /workspace/shared/gimp-painter-build.lock throughout the build. The default
still builds all four native harnesses; --test selects just one executable.
Only listed sources are instrumented. Other production C++ owners in the shared
RTTI closure get -frtti only, and dependencies remain uninstrumented.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

from painter_sanitizer_scope import bridge_rtti_sources, CXX_SUFFIXES


DEFAULT_TESTS = (
    "painter-perspective", "painter-perspective-ui",
    "painter-perspective-events", "painter-navigation-events",
)
PRODUCTION_SOURCES = {
    "app/display/gimpdisplayshell-tool-events.c",
    "app/display/gimpdisplayshell-handlers.c",
    "app/display/gimpdisplayshell-autoscroll.c",
    "app/display/gimpdisplayshell.c", "app/display/gimpmotionbuffer.c",
    "app/display/gimpstatusbar.c", "app/tools/gimptool.c",
    "app/core/gimpperspectiveguide.cpp", "app/core/gimpperspectiveguideundo.cpp",
    "app/core/gimpimage-perspective-guide.c", "app/core/gimpimage.c",
    "app/display/gimpcanvasperspectiveguide.cpp",
    "app/tools/gimpperspectiveguidetool.cpp", "app/tools/gimp-tools.c",
    "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp",
    "app/painter/gimp-painter-error.cpp", "app/painter/filter-scheduler.cpp",
    "app/painter/filter-edge.cpp", "app/painter/filter-gauss.cpp",
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--test", choices=DEFAULT_TESTS,
                        help="Build only this harness (default: all four)")
    parser.add_argument("--ninja-file", type=Path,
                        help="Existing Ninja manifest for read-only command extraction")
    args = parser.parse_args()
    build = args.build.resolve()
    root = Path(__file__).resolve().parents[2]
    output = build / "perspective-sanitizers"
    output.mkdir(exist_ok=True)
    tests = [args.test] if args.test else list(DEFAULT_TESTS)
    ninja_file = (args.ninja_file.resolve() if args.ninja_file
                  else build / "build.ninja")
    flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
    instrumented = PRODUCTION_SOURCES | {"app/tests/test-" + test + ".c" for test in tests}
    rtti_only = bridge_rtti_sources(root, build) - instrumented
    wanted = instrumented | rtti_only
    headers = {
        "app/core/gimpperspectiveguide.h", "app/core/gimpperspectiveguide.hpp",
        "app/core/gimpimage-perspective-guide.h", "app/core/gimpimage-private.h",
        "app/core/gimpimage-undo.h", "app/core/gimpundo.h", "app/core/core-types.h",
        "app/core/gimp.h", "app/display/gimpdisplay.h",
        "app/painter/object-ref.hpp", "app/painter/connection.hpp",
        "app/painter/gimp-painter-visibility.h", "app/painter/boundary.hpp",
        "app/tests/tests.h", "app/tests/gimp-app-test-utils.h",
        "migration/fixtures/legacy-perspective/snap-angle-source.inc",
        "migration/tests/build_perspective_sanitizers.py",
        "migration/tests/painter_sanitizer_scope.py",
    }
    headers |= {str(Path(source).with_suffix(suffix)) for source in wanted
                for suffix in (".h", ".hpp")
                if (root / Path(source).with_suffix(suffix)).is_file()}
    hashes = {name: sha256(root / name) for name in sorted(wanted | headers)}
    report = {
        "scope": "Perspective model, image owner, Undo, canvas overlay, editing tool, "
                 "event routing/statusbar, motion buffer, base tool, common bridge "
                 "and selected tests instrumented; listed RTTI-only production "
                 "owners recompiled for compatible vptr metadata without sanitizer "
                 "instrumentation; remaining host/dependencies uninstrumented",
        "sanitizers": ["address", "undefined"], "leak_detection": False,
        "instrumented_cpp_rtti": True, "tests": tests,
        "instrumented_sources": sorted(instrumented),
        "rtti_compatibility_only_sources": sorted(rtti_only),
        "sources": [], "headers": sorted(headers), "sources_sha256": hashes,
        "commands": [], "private_archives": {}, "executables": [],
        "executables_sha256": {}, "build_status": "building",
        "run_status": "Not run by builder; UI/event harnesses require native GTK display",
        "ninja_file": str(ninja_file), "ninja_file_sha256": sha256(ninja_file),
    }
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    production_hashes = {}
    try:
        entries = json.loads((build / "compile_commands.json").read_text())
        selected = {}
        for source in sorted(wanted):
            candidates = [entry for entry in entries
                          if (Path(entry["directory"]) / entry["file"]).resolve() == root / source]
            if not candidates:
                raise RuntimeError("Compile database lacks " + source)
            selected[source] = next(
                (entry for entry in candidates
                 if entry["output"].startswith(str(Path(source).parent) + "/")), candidates[0])

        # -t commands reads the existing manifest; it does not build or regenerate.
        links = {}
        archive_members = {}
        production_paths = {(build / entry["output"]).resolve() for entry in selected.values()}
        for test in tests:
            command = ["ninja", "-f", str(ninja_file), "-t", "commands", "app/tests/" + test]
            link = shlex.split(subprocess.check_output(command, cwd=build, text=True).strip().splitlines()[-1])
            links[test] = link
            normal_executable = build / ("app/tests/" + test)
            if normal_executable.is_file():
                production_paths.add(normal_executable.resolve())
            for arg in link:
                if arg.startswith("-"):
                    continue
                path = (build / arg).resolve()
                if path.is_relative_to(build) and path.is_file():
                    production_paths.add(path)
                if arg.endswith(".a") and path.is_relative_to(build) and arg not in archive_members:
                    members = subprocess.check_output(["ar", "t", arg], cwd=build, text=True).splitlines()
                    archive_members[arg] = [str((build / member).resolve()) for member in members]
                    production_paths.update(Path(member) for member in archive_members[arg])
        # Thin archives alone do not seal their members; retain both hashes.
        # The conservative RTTI closure can include disabled/unbuilt targets.
        # Record their absence too, without requiring an unrelated normal build.
        production_hashes = {str(path): sha256(path) if path.is_file() else None
                             for path in sorted(production_paths)}
        report["production_artifacts_sha256_before"] = production_hashes
        originals = {}
        for source, entry in selected.items():
            command = entry.get("arguments", []) or shlex.split(entry["command"])
            cleaned, skip = [], False
            for arg in command:
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
            print("Compiling " + source + (" (RTTI only)" if source in rtti_only else " (ASan/UBSan)"), flush=True)
            subprocess.run(cleaned, cwd=build, check=True)
            report["sources"].append(source)
            originals[str((build / entry["output"]).resolve())] = str(obj)

        # Replace every archive alias, including Meson's flattened link_whole
        # archives, so a non-RTTI member cannot re-enter through libapptestutils.
        archives = {}
        for archive, members in sorted(archive_members.items()):
            if not any(member in originals for member in members):
                continue
            archive_name = (Path(archive).relative_to(build).as_posix()
                            if Path(archive).is_absolute() else archive)
            private = output / archive_name.replace("/", "_")
            if private.exists():
                private.unlink()
            command = ["ar", "crsT", str(private)] + [originals.get(member, member) for member in members]
            report["commands"].append(command)
            subprocess.run(command, cwd=build, check=True)
            archives[archive] = str(private)
            report["private_archives"][archive] = {
                "path": str(private), "sha256": sha256(private),
                "replaced_members": {member: originals[member] for member in members if member in originals},
            }
        for test, link in links.items():
            exe = build / ("app/tests/" + test + "-asan")
            link[link.index("-o") + 1] = str(exe)
            link = [archives.get(arg, originals.get(str((build / arg).resolve()), arg))
                    if not arg.startswith("-") else arg for arg in link]
            link[1:1] = flags
            report["commands"].append(link)
            print("Linking " + str(exe), flush=True)
            subprocess.run(link, cwd=build, check=True)
            report["executables"].append(str(exe))
            report["executables_sha256"][str(exe)] = sha256(exe)
        report["build_status"] = "passed"
    except Exception as error:
        report["build_status"] = "failed"
        report["build_error"] = str(error)
        raise
    finally:
        report["changed_during_build"] = [name for name, digest in hashes.items()
                                          if not (root / name).is_file() or sha256(root / name) != digest]
        after = {path: sha256(Path(path)) if Path(path).is_file() else None
                 for path in production_hashes}
        report["production_artifacts_sha256_after"] = after
        report["production_artifacts_changed"] = [path for path in production_hashes
                                                   if after[path] != production_hashes[path]]
        report["production_artifacts_unchanged"] = bool(production_hashes) and not report["production_artifacts_changed"]
        if report["changed_during_build"] or report["production_artifacts_changed"]:
            report["build_status"] = "failed"
            report["build_error"] = "Source/header or production artifact changed during build"
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    if report["build_status"] != "passed":
        raise RuntimeError(report["build_error"])
    print("\n".join(report["executables"]))


if __name__ == "__main__":
    main()

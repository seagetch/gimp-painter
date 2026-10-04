#!/usr/bin/env python3
"""Build private, one-shot Filter descriptor/transaction fault tests.

Hold /workspace/shared/gimp-painter-build.lock while invoking this builder.
It reads the current normal painter-layer-ui compile/link recipe, replaces only
its main object, and adds a private fault test object and --wrap references.
No production object, Meson target, installed executable, allocator or library
is replaced. --sanitizer-report reuses a successful current focused UI build's
exact link recipe and private objects; it does not extend that build's scope.
Run on the native GTK display with the painter-layer-ui Meson test environment.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
MAIN = "app/tests/test-painter-layer-ui.c"
FAULT = "app/tests/test-filter-allocation-failures.cpp"
TEMPLATE = "app/tests/test-filter-argument-patch.cpp"
SCRIPT = "migration/tests/build_filter_allocation_tests.py"
WRAPS = ["_Znwm", "_Znam", "g_try_malloc", "g_try_malloc0_n"]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--sanitizer-report", type=Path)
    args = parser.parse_args()
    if not sys.platform.startswith("linux"):
        parser.error("This private wrapper and /proc evidence runner supports Linux only")
    build = args.build.resolve()
    mode = "asan" if args.sanitizer_report else "normal"
    output = build / ("filter-allocation-tests-" + mode)
    output.mkdir(exist_ok=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    entries = json.loads((build / "compile_commands.json").read_text())
    selected = {}
    for source in [MAIN, TEMPLATE]:
        selected[source] = next(entry for entry in entries
                                if (Path(entry["directory"]) / entry["file"]).resolve() == ROOT / source)
    hashes = {source: sha(ROOT / source) for source in [MAIN, FAULT, TEMPLATE, SCRIPT]}
    # Hash all tracked component headers/sources used by the focused build, as
    # well as the normal archive/binary inputs. The integrator owns its build.
    report = {"build_status": "building", "mode": mode, "commands": [],
              "scope": "Recoverable C++ new/new[] and g_try_malloc/g_try_malloc0_n failures only; "
                       "one calling thread, one failure, no GLib fatal allocator or host exhaustion; "
                       "private executable uses unchanged production code",
              "sources_sha256": hashes, "wrapped_symbols": WRAPS,
              "run_status": "not run by builder"}
    flags = []
    try:
        if args.sanitizer_report:
            previous = json.loads(args.sanitizer_report.read_text())
            if previous.get("build_status") != "passed":
                raise RuntimeError("Focused sanitizer builder has not passed")
            previous_hashes = previous["sources_sha256"]
            stale = [name for name, digest in previous_hashes.items() if sha(ROOT / name) != digest]
            if stale:
                raise RuntimeError("Rebuild focused sanitizer objects after source changes: " + repr(stale))
            link = list(previous["commands"][-1])
            old_main = str(build / "painter-ui-sanitizers" / (MAIN.replace("/", "_") + ".o"))
            report["focused_sanitizer_report"] = str(args.sanitizer_report.resolve())
            report["focused_sanitizer_report_sha256"] = sha(args.sanitizer_report)
            report["focused_instrumented_sources"] = previous["instrumented_sources"]
            report["focused_rtti_only_sources"] = previous["rtti_compatibility_only_sources"]
            hashes.update(previous_hashes)
            flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
        else:
            link = shlex.split(subprocess.check_output(
                ["ninja", "-t", "commands", "app/tests/painter-layer-ui"], cwd=build,
                text=True).strip().splitlines()[-1])
            old_main = selected[MAIN]["output"]
            for directory in ["app/core", "app/dialogs", "app/painter", "app/plug-in"]:
                for path in sorted((ROOT / directory).glob("*")):
                    if path.suffix in {".c", ".cpp", ".h", ".hpp"}:
                        hashes[str(path.relative_to(ROOT))] = sha(path)
            for path in sorted((ROOT / "app/tests").glob("test-filter-*")):
                if path.suffix in {".c", ".cpp", ".inc"}:
                    hashes[str(path.relative_to(ROOT))] = sha(path)
        if link.count(old_main) != 1:
            raise RuntimeError("Expected exactly one main object in private link recipe: " + old_main)
        objects = {}
        for source, template in [(MAIN, MAIN), (FAULT, TEMPLATE)]:
            entry = selected[template]
            cleaned, skip = [], False
            for arg in shlex.split(entry["command"]):
                if skip:
                    skip = False
                    continue
                if arg in {"-MF", "-MQ", "-MT"}:
                    skip = True
                    continue
                if arg not in {"-MD", "-MMD"}:
                    cleaned.append(arg)
            destination = output / (Path(source).name + ".o")
            cleaned[cleaned.index("-o") + 1] = str(destination)
            cleaned[cleaned.index("-c") + 1] = str(ROOT / source)
            cleaned += ["-DGIMP_PAINTER_FAULT_TEST=1"] + flags
            if source.endswith(".cpp") and flags:
                cleaned += ["-frtti"]
            report["commands"].append(cleaned)
            subprocess.run(cleaned, cwd=build, check=True)
            objects[source] = str(destination)
        executable = build / "app/tests" / ("painter-filter-allocation-tests-" + mode)
        link[link.index("-o") + 1] = str(executable)
        link = [objects[MAIN] if arg == old_main else arg for arg in link]
        link.insert(link.index(objects[MAIN]) + 1, objects[FAULT])
        link += ["-Wl,--wrap=" + symbol for symbol in WRAPS]
        inputs = {str((build / arg).resolve()): sha((build / arg).resolve()) for arg in link
                  if arg.endswith((".o", ".a", ".so")) and (build / arg).is_file()}
        report["link_inputs_sha256"] = inputs
        report["commands"].append(link)
        subprocess.run(link, cwd=build, check=True)
        report["executable"] = str(executable)
        report["executable_sha256"] = sha(executable)
        report["changed_during_build"] = [name for name, digest in hashes.items() if sha(ROOT / name) != digest]
        if report["changed_during_build"]:
            raise RuntimeError("Sources changed during private build")
        tests = json.loads((build / "meson-info/intro-tests.json").read_text())
        report["test_environment"] = next(item["env"] for item in tests if item["name"] == "painter-layer-ui")
        names = ["descriptors", "gtk_changed_array"] + ["patch_" + family + "_" + kind
                 for family in ["cpp", "try"]
                 for kind in ["scalar", "string", "strv", "double_array", "int32_array", "raw_array"]]
        names += ["route_" + route for route in ["blinds", "small_tiles", "retinex", "convolution"]]
        report["test_paths"] = ["/painter-layer-ui/filter_fault_" + name for name in names]
        report["run_command"] = [str(executable)] + [arg for path in report["test_paths"] for arg in ["-p", path]]
        report["build_status"] = "passed"
    except Exception as error:
        report["build_status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(executable)


if __name__ == "__main__":
    main()

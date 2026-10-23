#!/usr/bin/env python3
"""Prove 04.009's direct JSON dependency with real native objects and links.

Source the selected build's dependency environment first. This never rebuilds
or replaces existing build products. Fresh isolated production Resource code
uses only JSON-GLib/GIO and the real painter error implementation. Full native
GUI/console links are replayed into a private output directory, then repeated
without the direct JSON library as a negative control. No semantic APIs are
stubbed. Reports contain selected facts and hashes, never inherited environment
values or raw compiler/Meson logs. Linux ELF/binutils checks only.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
RESOURCE = "app/paint/painter-mypaint/resource.cpp"
ERROR = "app/painter/gimp-painter-error.cpp"
CONSUMERS = ["app/core/gimplayerpreset.c", "app/core/gimplayerpreset-apply.cpp", RESOURCE]
SMOKE = "migration/tests/json-link/resource-link-smoke.cpp"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(argv, cwd, expected=0):
    result = subprocess.run(argv, cwd=cwd, text=True, capture_output=True, timeout=180)
    if expected == 0 and result.returncode != 0:
        # Deliberately do not publish subprocess output: it may contain arbitrary
        # inherited build flags. Failure metadata identifies the exact operation.
        raise RuntimeError(f"{Path(argv[0]).name} failed with exit {result.returncode}")
    if expected != 0 and result.returncode == 0:
        raise RuntimeError("Negative control unexpectedly succeeded")
    return result


def needed(path):
    output = run(["readelf", "-d", str(path)], ROOT).stdout
    return sorted(re.findall(r"\(NEEDED\).*?\[([^]]+)\]", output))


def json_symbols(text):
    return sorted(set(re.findall(r"\bjson_[A-Za-z0-9_]+\b", text)))


def unresolved_json_symbols(text):
    # Do not mistake a local C++ helper such as json_string() mentioned in a
    # diagnostic's context for an unresolved JSON-GLib API.
    return sorted(set(re.findall(r"undefined reference to [`'‘](json_[A-Za-z0-9_]+)", text)))


def json_library(arg):
    return arg == "-ljson-glib-1.0" or Path(arg).name.startswith("libjson-glib-1.0.so")


def source_contract():
    top = (ROOT / "meson.build").read_text()
    version = re.search(r"^json_glib_minver\s*=\s*'([^']+)'", top, re.M)
    assert version
    assert re.search(r"^json_glib\s*=\s*dependency\('json-glib-1.0',\s*version:\s*'>='\+json_glib_minver\)", top, re.M)
    checks = [
        ("app/core/meson.build", "libappcore", "static_library"),
        ("app/paint/painter-mypaint/meson.build", "painter_mypaint", "static_library"),
        ("app/paint/painter-mypaint/meson.build", "painter_mypaint_dep", "declare_dependency"),
    ]
    for path, variable, call in checks:
        text = re.sub(r"#[^\n]*", "", (ROOT / path).read_text())
        # Stop at the next top-level statement; nested argument lists are fine.
        block = re.search(r"^" + variable + r"\s*=\s*" + call + r"\((.*?)(?=^\w|\Z)", text, re.M | re.S)
        assert block, variable
        deps = re.search(r"dependencies:\s*\[([^]]+)\]", block[1])
        assert deps and "json_glib" in [value.strip() for value in deps[1].split(",")], variable
    routing = json.loads((ROOT / "migration/inventory/configure-hunk-routing-review.json").read_text())
    hunks = [h for h in routing["mappings"] if h["hunk_id"] in ("01.002/001891", "01.002/001894")]
    assert len(hunks) == 2
    for hunk in hunks:
        assert hunk["implementation_tasks"] == ["04.009"]
        assert hashlib.sha256(hunk["payload"].encode()).hexdigest() == hunk["payload_sha256"]
    return {"json_minimum": version[1], "required_dependency": True,
            "direct_targets": [variable for _, variable, _ in checks],
            "original_hunks": [{key: h[key] for key in ("hunk_id", "payload_sha256", "implementation_tasks")} for h in hunks]}


def check(args, report):
    build = args.build.resolve()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    assert not output.is_relative_to(build), "Keep link checks outside the original build"
    info = json.loads((build / "meson-info/meson-info.json").read_text())
    assert Path(info["directories"]["source"]).resolve() == ROOT
    options = {o["name"]: o["value"] for o in json.loads((build / "meson-info/intro-buildoptions.json").read_text())}
    assert options["painter-http"] == ("enabled" if args.configuration == "http" else "disabled")
    report["source_contract"] = source_contract()
    report["json_glib_version"] = run(["pkg-config", "--modversion", "json-glib-1.0"], ROOT).stdout.strip()
    run(["pkg-config", "--atleast-version=" + report["source_contract"]["json_minimum"], "json-glib-1.0"], ROOT)
    source_paths = CONSUMERS + [ERROR, SMOKE, "tools/check_painter_json_link.py", "meson.build", "app/core/meson.build",
                                "app/paint/painter-mypaint/meson.build", "migration/inventory/configure-hunk-routing-review.json"]
    report["source_sha256"] = {p: digest(ROOT / p) for p in source_paths}
    report["checkout_commit"] = run(["git", "rev-parse", "HEAD"], ROOT).stdout.strip()
    database = json.loads((build / "compile_commands.json").read_text())
    entries = {(Path(e["directory"]) / e["file"]).resolve(): e for e in database}
    production = {}
    report["production_consumers"] = []
    for path in CONSUMERS + [ERROR]:
        entry = entries[ROOT / path]
        obj = (build / entry["output"]).resolve()
        production[path] = obj
        if path == ERROR:
            continue
        refs = json_symbols(run(["nm", "-u", str(obj)], ROOT).stdout)
        assert refs, path
        flags = entry.get("arguments") or shlex.split(entry["command"])
        assert any("json-glib-1.0" in arg and arg.startswith("-I") for arg in flags)
        report["production_consumers"].append({"source": path, "object": entry["output"],
            "object_sha256": digest(obj), "undefined_json_symbols": refs,
            "json_include_present": True})
    targets = json.loads((build / "meson-info/intro-targets.json").read_text())
    for name in ("appcore", "painter-mypaint"):
        target = next(t for t in targets if t["name"] == name)
        assert "json-glib-1.0" in target["dependencies"], name

    # Explicitly construct the small include/link set; no GEGL, Soup or GTK flags
    # from application targets enter this independent production compilation.
    pkgflags = shlex.split(run(["pkg-config", "--cflags", "json-glib-1.0", "gio-2.0"], ROOT).stdout)
    pkglibs = shlex.split(run(["pkg-config", "--libs", "json-glib-1.0", "gio-2.0"], ROOT).stdout)
    assert not any(re.search(r"soup|gegl|gtk", flag, re.I) for flag in pkgflags + pkglibs)
    assert any(json_library(flag) for flag in pkglibs)
    cxx = (entries[ROOT / RESOURCE].get("arguments") or shlex.split(entries[ROOT / RESOURCE]["command"]))[0]
    includes = ["-I" + str(ROOT / "app"), "-I" + str(ROOT / "app/paint/painter-mypaint"),
                "-I" + str(build / "app/paint/painter-brush-settings")]
    flags = [cxx, "-std=c++14", "-O2", "-fexceptions", "-fno-rtti", "-fdiagnostics-color=never"] + includes
    fresh = []
    for index, source in enumerate((RESOURCE, ERROR, SMOKE)):
        obj = output / f"isolated-{index}.o"
        run(flags + pkgflags + ["-c", str(ROOT / source), "-o", str(obj)], ROOT)
        fresh.append(obj)
    no_json_include = [arg for arg in pkgflags if "json-glib-1.0" not in arg]
    negative = run(flags + no_json_include + ["-fsyntax-only", str(ROOT / RESOURCE)], ROOT, expected=1)
    assert "json-glib/json-glib.h" in negative.stderr and "No such file" in negative.stderr
    report["isolated_compile"] = {"sources": [RESOURCE, ERROR, SMOKE],
        "pkg_config_modules": ["json-glib-1.0", "gio-2.0"],
        "without_json_include_exit": negative.returncode, "missing_json_header": True,
        "soup_gegl_gtk_flags": []}
    report["isolated_links"] = []
    for kind, objects in (("fresh-production-source", fresh),
                          ("configured-production-objects", [production[RESOURCE], production[ERROR], fresh[2]])):
        executable = output / ("resource-" + kind)
        argv = [cxx, "-Wl,--no-undefined", "-Wl,--no-copy-dt-needed-entries", "-o", str(executable)] + [str(o) for o in objects]
        run(argv + pkglibs, ROOT)
        libraries = needed(executable)
        assert "libjson-glib-1.0.so.0" in libraries
        assert not any(re.search(r"soup|gegl|gtk", name, re.I) for name in libraries)
        resolved = run(["ldd", str(executable)], ROOT).stdout
        closure = sorted(set(re.findall(r"^\s*([\w.+-]+)\s+=>", resolved, re.M)))
        assert "not found" not in resolved and closure
        assert not any(re.search(r"soup|gegl|gtk", name, re.I) for name in closure)
        runtime = run([str(executable)], ROOT)
        assert runtime.stdout.strip() == "PASS: production Resource JSON reader/writer and legacy-v2 link smoke"
        omitted = [arg for arg in pkglibs if not json_library(arg)]
        argv[argv.index("-o") + 1] = str(executable) + "-without-json"
        negative = run(argv + omitted, ROOT, expected=1)
        symbols = unresolved_json_symbols(negative.stderr)
        assert "json_parser_load_from_data" in symbols and "json_generator_to_data" in symbols
        report["isolated_links"].append({"kind": kind, "strict_link_exit": 0, "runtime_exit": runtime.returncode,
            "direct_needed": libraries, "without_json_link_exit": negative.returncode,
            "resolved_dynamic_library_closure": closure,
            "without_json_unresolved_symbols": symbols})

    report["application_links"] = []
    for name in ("app/gimp-3.0", "app/gimp-console-3.0"):
        commands = run(["ninja", "-t", "commands", name], build).stdout.splitlines()
        argv = shlex.split(commands[-1])
        assert "-o" in argv and "-Wl,--no-undefined" in argv
        assert any(json_library(arg) for arg in argv)
        assert any("libsoup-" in arg for arg in argv) == (args.configuration == "http")
        executable = output / (Path(name).name + "-direct-json")
        argv[argv.index("-o") + 1] = str(executable)
        argv.insert(1, "-Wl,--no-copy-dt-needed-entries")
        run(argv, build)
        libraries = needed(executable)
        assert "libjson-glib-1.0.so.0" in libraries
        negative_argv = [arg for arg in argv if not json_library(arg)]
        negative_argv[negative_argv.index("-o") + 1] = str(executable) + "-without-json"
        negative = run(negative_argv, build, expected=1)
        symbols = unresolved_json_symbols(negative.stderr)
        assert symbols and ("undefined reference" in negative.stderr or "DSO missing from command line" in negative.stderr)
        assert "gimplayerpreset" in negative.stderr
        report["application_links"].append({"target": name,
            "strict_positive_link_exit": 0, "direct_needed": libraries,
            "without_direct_json_exit": negative.returncode,
            "without_direct_json_symbols": symbols,
            "core_preset_named_in_negative_diagnostic": "gimplayerpreset" in negative.stderr,
            "command_sha256": hashlib.sha256(json.dumps(argv).encode()).hexdigest(),
            "positive_executable_sha256": digest(executable)})
    assert all(digest(ROOT / p) == value for p, value in report["source_sha256"].items()), "Source changed during checks"
    report["status"] = "PASS"


def main():
    if not __debug__:
        raise SystemExit("Run this verifier without Python optimization so its assertions remain active")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--configuration", choices=("default", "http"), required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    report = {"status": "RUNNING", "task": "04.009/json-dependency", "configuration": args.configuration,
        "build": str(args.build.resolve()),
        "started_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "limitations": ["Linkage and narrowly scoped Resource smoke only; no claim of complete old-file or fixture parity.",
                        "Application production links retain GEGL because native core requires its semantic APIs; the independent Resource check removes GEGL and Soup entirely.",
                        "Linux native ELF evidence only; no cross-platform or sanitizer claim."]}
    try:
        check(args, report)
    except (AssertionError, RuntimeError, OSError, subprocess.TimeoutExpired) as error:
        report["status"] = "FAIL"
        report["failure_type"] = type(error).__name__
        print("JSON dependency verification failed: " + type(error).__name__)
        if isinstance(error, (AssertionError, RuntimeError)):
            report["failure_detail"] = str(error)
        return 1
    finally:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: " + args.configuration + " direct JSON-GLib dependency, isolated resource links, GUI and console links")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

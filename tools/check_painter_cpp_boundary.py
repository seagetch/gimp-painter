#!/usr/bin/env python3
"""Check original 04.005 using native C compiler and Meson install evidence.

Run in the configured dependency environment. This executes syntax checks, not
the application or its plug-ins. Compiler commands come from each build's real
compile_commands.json; generated prerequisites must already exist. All app C
commands are checked in both the default build and its separate HTTP-enabled
configuration. C++ commands establish that private headers remain usable.
"""

import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
CPP_SUFFIXES = {".hpp", ".hh", ".hxx", ".h++"}
SOURCE_SUFFIXES = CPP_SUFFIXES | {".c", ".cc", ".cpp", ".h", ".inc"}
BASELINE = "335c224953530c7a85fdc3144d69f031581ad27f"
LEGACY_TEST = "app/operations/tests/test-operations.c"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def identifier(path):
    path = Path(path).resolve()
    return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)


def dependencies(path, cwd):
    # GCC make dependencies quote spaces, # and backslashes, not shell tokens.
    text = path.read_text().replace("\\\n", "")
    if ":" not in text:
        raise RuntimeError("Compiler did not write a dependency rule")
    words = re.findall(r"(?:\\.|[^\s\\])+", text.split(":", 1)[1])
    return sorted({(cwd / re.sub(r"\\(.)", r"\1", word)).resolve()
                   for word in words})


def forbidden_c_dependencies(paths):
    return [identifier(p) for p in paths
            if p.suffix in CPP_SUFFIXES or "c++" in p.parts]


def private_installations(installed, private_headers):
    failures = []
    for source, destination in installed.items():
        source_path = Path(source).resolve()
        # Directory installs also expose their contained implementation headers.
        contained = [p for p in private_headers
                     if p == source_path or p.is_relative_to(source_path)]
        destinations = destination if isinstance(destination, list) else [destination]
        if contained or source_path.suffix in CPP_SUFFIXES or any(
                Path(d).suffix in CPP_SUFFIXES for d in destinations if isinstance(d, str)):
            failures.append({"source": identifier(source_path),
                             "destination": destination,
                             "private_headers": [identifier(p) for p in contained]})
    return failures


def syntax_command(entry, dependency_file):
    original = entry.get("arguments") or shlex.split(entry["command"])
    argv = []
    skip_next = False
    for arg in original:
        if skip_next:
            skip_next = False
        elif arg in ("-o", "-MF", "-MQ", "-MT"):
            skip_next = True
        elif arg in ("-c", "-MD", "-MMD", "-MP"):
            continue
        elif arg.startswith(("-MF", "-MQ", "-MT")):
            continue
        else:
            argv.append(arg)
    if skip_next:
        raise RuntimeError("Incomplete compiler option")
    return argv + ["-fsyntax-only", "-MD", "-MF", str(dependency_file),
                   "-MT", "boundary-check", "-fdiagnostics-color=never"]


class Verification:
    def __init__(self, args):
        self.args = args
        self.output = args.output_dir.resolve()
        self.output.mkdir(parents=True, exist_ok=True)
        self.inputs = {}
        self.language_guards = {}
        for language, condition in (("c", "ifdef"), ("c++", "ifndef")):
            guard = self.output / ("language-" + language.replace("+", "p") + ".h")
            guard.write_text(f"#{condition} __cplusplus\n#error Incorrect translation-unit language\n#endif\n")
            self.language_guards[language] = guard
        self.report = {"status": "RUNNING", "task": "04.005",
                       "started_at_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                       "units": [], "installations": [], "controls": [],
                       "limitations": ["Native configured targets only; platform and feature acceptance remain separate.",
                                       "C++ syntax checks establish header usability, not runtime behavior."]}
        self.private = sorted(p.resolve() for p in (ROOT / "app").rglob("*")
                              if p.is_file() and p.suffix in CPP_SUFFIXES)
        if not self.private:
            raise RuntimeError("No private application headers found")
        # Capture sources before any compiler runs, including then-unused private
        # headers. Include flags and exact generated inputs are captured below.
        files = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
        self.capture([ROOT / f for f in files if f and
                      (Path(f).suffix in SOURCE_SUFFIXES | {".in"} or
                       Path(f).name in ("meson.build", "meson_options.txt"))])
        self.capture([Path(__file__)] + self.private + list(self.language_guards.values()))
        self.report["private_headers"] = [identifier(p) for p in self.private]

    def capture(self, paths):
        for path in paths:
            path = Path(path).resolve()
            current = digest(path)
            if path in self.inputs and self.inputs[path] != current:
                raise RuntimeError("Input changed during verification: " + str(path))
            self.inputs[path] = current

    def save(self):
        self.args.report.parent.mkdir(parents=True, exist_ok=True)
        self.args.report.write_text(json.dumps(self.report, indent=2) + "\n")

    def build_units(self, build, http_enabled=False):
        build = build.resolve()
        database = build / "compile_commands.json"
        installed_path = build / "meson-info/intro-installed.json"
        targets_path = build / "meson-info/intro-targets.json"
        options_path = build / "meson-info/intro-buildoptions.json"
        build_files_path = build / "meson-info/intro-buildsystem_files.json"
        info_path = build / "meson-info/meson-info.json"
        self.capture([database, installed_path, targets_path, options_path, build_files_path, info_path, build / "config.h"])
        info = json.loads(info_path.read_text())
        if Path(info["directories"]["source"]).resolve() != ROOT:
            raise RuntimeError("Configured build belongs to a different source tree")
        build_files = [Path(path) for path in json.loads(build_files_path.read_text())]
        if any(not path.is_file() or path.stat().st_mtime_ns > installed_path.stat().st_mtime_ns
               for path in build_files):
            raise RuntimeError("Meson build input is newer than install metadata; reconfigure first")
        self.capture(build_files)
        options = {option["name"]: option["value"] for option in json.loads(options_path.read_text())}
        macro_enabled = bool(re.search(r"^#define HAVE_PAINTER_HTTP(?:[ \t]+.*)?$",
                                      (build / "config.h").read_text(), re.M))
        if options.get("painter-http") != ("enabled" if http_enabled else "disabled") or macro_enabled != http_enabled:
            raise RuntimeError("Expected distinct default/HTTP configurations with matching config macro")
        self.capture(p for p in build.rglob("*")
                     if p.is_file() and p.suffix in SOURCE_SUFFIXES)
        installed = json.loads(installed_path.read_text())
        failures = private_installations(installed, self.private)
        if failures:
            raise RuntimeError("Private header installed: " + json.dumps(failures))
        self.report["installations"].append({"build": str(build), "painter_http_enabled": http_enabled,
            "manifest_sha256": digest(installed_path), "entries": len(installed),
            "private_header_installations": failures})
        units = []
        for entry in json.loads(database.read_text()):
            source = (Path(entry["directory"]) / entry["file"]).resolve()
            in_app = any(source.is_relative_to(root / "app") for root in (ROOT, build))
            if not in_app:
                continue
            if source.suffix not in (".c", ".cc", ".cpp"):
                raise RuntimeError("Unknown app source language: " + str(source))
            if not source.is_file():
                raise RuntimeError("Generate configured source first: " + str(source))
            original = entry.get("arguments") or shlex.split(entry["command"])
            if source.suffix == ".c" and (any("c++" in flag or flag in ("-xc++", "-xc++-header")
                                             for flag in original[:1] + original[1:]
                                             if not flag.startswith(("-I", "-D")))):
                raise RuntimeError("C source configured as C++: " + str(source))
            units.append((entry, source, "http" if http_enabled else "default"))
        if not units or not any(s.suffix == ".c" for _, s, _ in units):
            raise RuntimeError("Expected app C compile commands are absent")
        legacy_targets = [target for target in json.loads(targets_path.read_text())
                          if target["name"] == "test-operations"]
        if len(legacy_targets) != 1 or legacy_targets[0]["build_by_default"]:
            raise RuntimeError("Legacy operations test is no longer an optional target")
        self.capture(source for _, source, _ in units)
        return units

    def check_unit(self, indexed):
        index, (entry, source, configuration) = indexed
        cwd = Path(entry["directory"])
        stem = f"unit-{index:04d}"
        depfile = self.output / (stem + ".d")
        depfile.unlink(missing_ok=True)
        argv = syntax_command(entry, depfile)
        language = "c" if source.suffix == ".c" else "c++"
        argv += ["-include", str(self.language_guards[language])]
        started = time.monotonic()
        result = subprocess.run(argv, cwd=cwd, capture_output=True, timeout=120)
        log = self.output / (stem + ".log")
        log.write_bytes(result.stdout + result.stderr)
        deps = dependencies(depfile, cwd) if depfile.exists() else []
        forbidden = forbidden_c_dependencies(deps) if source.suffix == ".c" else []
        row = {"source": identifier(source), "configuration": configuration,
               "language": language,
               "command_sha256": hashlib.sha256(json.dumps(argv).encode()).hexdigest(),
               "exit_code": result.returncode, "seconds": round(time.monotonic() - started, 3),
               "dependency_count": len(deps), "dependency_sha256": digest(depfile) if depfile.exists() else None,
               "private_headers": [identifier(p) for p in deps if p in self.private],
               "forbidden_c_dependencies": forbidden,
               "log_sha256": digest(log), "log_bytes": log.stat().st_size}
        if result.returncode and identifier(source) == LEGACY_TEST and not forbidden:
            baseline = subprocess.check_output(["git", "show", BASELINE + ":" + LEGACY_TEST], cwd=ROOT)
            if source.read_bytes() != baseline:
                raise RuntimeError("Known optional-test source changed; reclassify its failure")
            preprocessed = self.output / (stem + ".i")
            preprocess_argv = [arg for arg in argv if arg != "-fsyntax-only"]
            preprocess_argv += ["-E", "-P", "-o", str(preprocessed)]
            pre = subprocess.run(preprocess_argv, cwd=cwd, capture_output=True, timeout=120)
            text = preprocessed.read_text() if pre.returncode == 0 else ""
            text = re.sub(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', '""', text)
            tokens = re.findall(r"\b(?:template\s*<|std\s*::|namespace\s+\w+|class\s+\w+\s*[:{]|using\s+namespace)", text)
            predeps = dependencies(depfile, cwd) if pre.returncode == 0 else []
            if pre.returncode or tokens or forbidden_c_dependencies(predeps):
                raise RuntimeError("Legacy-test C preprocessing boundary failed")
            row["known_optional_syntax_failure"] = {
                "baseline": BASELINE, "source_sha256": digest(source),
                "preprocess_exit_code": pre.returncode, "cpp_tokens": tokens,
                "preprocessed_sha256": digest(preprocessed),
                "reason": "Existing non-default test lacks Gimp/GEGL/C library declarations; syntax failure remains open."}
            deps = predeps
        return row, deps, argv

    def controls(self, sample, cpp_sample):
        entry, _, _ = sample
        cwd = Path(entry["directory"])
        original_source = (cwd / entry["file"]).resolve()
        dep = self.output / "control.d"
        base = syntax_command(entry, dep)
        base = [arg for arg in base if (cwd / arg).resolve() != original_source]
        source = self.output / "control.c"
        # A transitive private header with currently C-compatible contents must
        # be rejected by the boundary check even though a C compiler accepts it.
        private = self.output / "private-control.hpp"
        private.write_text("typedef int PainterBoundaryControl;\n")
        public = self.output / "public-control.h"
        public.write_text('#include "private-control.hpp"\n')
        source.write_text('#include "public-control.h"\nPainterBoundaryControl value;\n')
        result = subprocess.run(base + [str(source)], cwd=cwd, capture_output=True)
        detected = forbidden_c_dependencies(dependencies(dep, cwd)) if dep.exists() else []
        if result.returncode or str(private) not in detected:
            raise RuntimeError("Transitive private-header negative control was not detected")
        self.report["controls"].append({"name": "transitive-private-header", "compiler_exit": 0,
                                        "boundary_rejected": True})
        # Genuine C++ tokens cannot silently become C implementation input.
        private.write_text("template<class T> struct PainterBoundaryControl { T value; };\n")
        source.write_text('#include "public-control.h"\nPainterBoundaryControl<int> value;\n')
        result = subprocess.run(base + [str(source)], cwd=cwd, capture_output=True)
        if result.returncode == 0:
            raise RuntimeError("C compiler accepted injected template")
        (self.output / "template-negative.log").write_bytes(result.stdout + result.stderr)
        self.report["controls"].append({"name": "template-through-public-header",
                                        "compiler_exit": result.returncode, "compiler_rejected": True})
        cpp_entry, cpp_source, _ = cpp_sample
        cpp_cwd = Path(cpp_entry["directory"])
        cpp_argv = [arg for arg in syntax_command(cpp_entry, self.output / "control-cpp.d")
                    if (cpp_cwd / arg).resolve() != cpp_source]
        cpp_probe = self.output / "control.cpp"
        cpp_probe.write_bytes(source.read_bytes())
        result = subprocess.run(cpp_argv + [str(cpp_probe)], cwd=cpp_cwd, capture_output=True)
        if result.returncode:
            raise RuntimeError("Negative C probe is not valid C++: " + result.stderr.decode()[-2000:])
        self.report["controls"].append({"name": "same-template-probe-cpp-positive", "compiler_exit": 0})
        result = subprocess.run(cpp_argv + ["-include", str(self.language_guards["c"]), str(cpp_probe)],
                                cwd=cpp_cwd, capture_output=True)
        if result.returncode == 0 or b"Incorrect translation-unit language" not in result.stderr:
            raise RuntimeError("Wrong-language C compiler invocation was not rejected")
        self.report["controls"].append({"name": "cpp-driver-for-c-rejected", "compiler_rejected": True})
        for name, source_path in (("private-file-install", self.private[0]),
                                  ("private-directory-install", self.private[0].parent)):
            if not private_installations({str(source_path): "/synthetic/include"}, self.private):
                raise RuntimeError("Private installation control was not detected")
            self.report["controls"].append({"name": name, "boundary_rejected": True})

    def run(self):
        units = self.build_units(self.args.build_dir)
        units += self.build_units(self.args.http_build_dir, http_enabled=True)
        self.report["configured_units"] = len(units)
        self.save()
        all_deps = set()
        commands = []
        failures = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=self.args.jobs) as pool:
            for row, deps, argv in pool.map(self.check_unit, enumerate(units)):
                self.report["units"].append(row)
                all_deps.update(deps)
                commands.append(argv)
                if ((row["exit_code"] and not row.get("known_optional_syntax_failure")) or
                        row["forbidden_c_dependencies"] or not deps):
                    failures.append(row)
                if len(self.report["units"]) % 100 == 0:
                    print(f"checked {len(self.report['units'])}/{len(units)}", flush=True)
                    self.save()
        (self.output / "executed-commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if failures:
            self.report["failures"] = failures
            raise RuntimeError(str(len(failures)) + " compilation/boundary failures")
        # Repo and generated inputs were captured before compilation. Dependency
        # SDK files are recorded now; their package lock validation is separate.
        self.capture(all_deps)
        self.controls(next(u for u in units if u[1].suffix == ".c"),
                      next(u for u in units if u[1].suffix == ".cpp"))
        used_private = sorted({p for row in self.report["units"] for p in row["private_headers"]})
        if not used_private:
            raise RuntimeError("No C++ positive coverage of private headers")
        self.report["private_headers_used_by_cpp"] = used_private
        self.report["private_headers_not_in_configured_closure"] = sorted(
            set(map(identifier, self.private)) - set(used_private))
        self.report["counts"] = {"c": sum(r["language"] == "c" for r in self.report["units"]),
                                "cpp": sum(r["language"] == "c++" for r in self.report["units"]),
                                "private_headers": len(self.private),
                                "private_headers_used_by_cpp": len(used_private)}
        self.report["counts"]["syntax_failures_in_fixed_optional_test"] = sum(
            bool(row.get("known_optional_syntax_failure")) for row in self.report["units"])
        changed = [identifier(p) for p, before in self.inputs.items()
                   if not p.is_file() or digest(p) != before]
        if changed:
            raise RuntimeError("Inputs changed during run: " + ", ".join(changed))
        current_private = sorted(p.resolve() for p in (ROOT / "app").rglob("*")
                                 if p.is_file() and p.suffix in CPP_SUFFIXES)
        if current_private != self.private:
            raise RuntimeError("Private header inventory changed during run")
        self.report["repo_input_sha256"] = {identifier(p): value for p, value in sorted(self.inputs.items())
                                             if p.is_relative_to(ROOT)}
        self.report["external_input_count"] = sum(not p.is_relative_to(ROOT) for p in self.inputs)
        self.report["external_dependency_note"] = "SDK dependency identities are captured after compilation; package archive verification is recorded by the build preparation, not inferred from this check."
        self.report["inputs_unchanged_at_end"] = True
        self.report["status"] = "PASS"
        self.report["completed_at_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--http-build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 16:
        parser.error("--jobs must be between 1 and 16")
    verification = None
    try:
        verification = Verification(args)
        verification.run()
    except Exception as error:
        if verification:
            verification.report.update(status="FAIL", error=str(error))
            verification.save()
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(verification.report["counts"], sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())

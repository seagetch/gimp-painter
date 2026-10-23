#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build and run focused native owner-context / XCF ASan+UBSan acceptance.

The historical default is Sharpen. --operation threshold-alpha adds the point
shadow wrapper and requires verified Threshold Alpha, projection-history and
Sharpen corpora. --operation sobel instruments the independent Sobel raster
plus its changed host invalidation geometry, and requires verified Sobel,
projection-history and Sharpen corpora. Selected
paths still run exactly as requested, with no implicit extra tests.

First build both ordinary app/tests targets in the configured build environment.
Invocation order is secure-Xvfb -> verified fixture wrapper -> locking shell ->
this runner. For example, inside those wrappers (which do not forward lock FDs):

  bash -c 'exec 9>/workspace/shared/gimp-painter-build.lock; flock -x 9;
    python3 migration/tests/run_sharpen_context_sanitizers.py BUILD --lock-fd 9 \
      --report REPORT.json \
      --native-test-path /gimp-filter-layer/sharpen_late_context_routes \
      --xcf-test-path /painter-xcf-open/sharpen_old_writer_roundtrip'

Repeat each path option to select further exact cases. Each case is a separate,
sequential process and must emit one successful, non-skipped TAP result matching
the requested path. No implicit full-suite run or zero-test success is allowed.
DISPLAY, XAUTHORITY, fixture variables and dependency paths are inherited. This
runner neither starts a display nor authenticates it nor materializes fixtures;
the outer wrappers own authentication and fixture provenance verification.

Only the listed translation units are instrumented. Other production no-RTTI
C++ units discovered by painter_sanitizer_scope and present in the two harness
link closures get -frtti only. Unlinked discoveries are reported separately. Remaining GIMP
and dependencies are uninstrumented; leak detection is explicitly disabled.
All objects, depfiles, thin archives and executables are private to a fresh run
directory. Selected ordinary objects, archive containers and original test
executables are sealed and never replaced; other thin-archive members are not
individually sealed. A JSON report is checkpointed before/after every command,
including failures/timeouts.
"""

import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import tempfile
import time
import traceback

sys.dont_write_bytecode = True
from painter_sanitizer_scope import CXX_SUFFIXES, bridge_rtti_sources


ROOT = Path(__file__).resolve().parents[2]
FLAGS = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1"]
TARGETS = {"native": "gimp-filter-layer", "xcf": "painter-xcf-open"}
HARNESS = {kind: "app/tests/test-" + target + ".c"
           for kind, target in TARGETS.items()}
COMMON = {
    "app/config/gimpgeglconfig.c", "app/config/gimppainterfilterconfig.cpp",
    "app/core/gimpfilterlayer.cpp", "app/core/gimpfiltercontext.cpp",
    "app/core/gimpclonelayer.cpp", "app/core/gimpitem.c",
    "app/core/gimpgrouplayer.c", "app/core/gimpimage-undo.c",
    "app/core/gimpimage-duplicate.c",
    "app/painter/binding-store.cpp", "app/painter/gimp-painter-binding.cpp",
    "app/painter/gimp-painter-error.cpp", "app/painter/filter-scheduler.cpp",
    "app/painter/filter-context.cpp", "app/painter/filter-raster.cpp",
    "app/painter/filter-spool.cpp", "app/painter/filter-raster-kernels.cpp",
    "app/painter/filter-native-kernels.cpp", "app/painter/filter-sharpen.cpp",
    "app/painter/filter-sharpen-shadow.cpp",
    "app/xcf/xcf.c", "app/xcf/xcf-load.c", "app/xcf/xcf-save.c",
    "app/xcf/xcf-read.c", "app/xcf/xcf-write.c", "app/xcf/xcf-seek.c",
    "app/xcf/xcf-utils.c", "app/xcf/painter-xcf-load.cpp",
    "app/xcf/painter-xcf-compat.cpp", "app/xcf/painter-xcf-preserve.cpp",
    "app/xcf/painter-xcf-arguments.cpp", "app/xcf/painter-xcf-storage.cpp",
    "app/xcf/painter-xcf-multipart.cpp", "app/xcf/painter-xcf-transport.cpp",
}
INSTRUMENTED = COMMON | set(HARNESS.values())
HEADER_SUFFIXES = {".h", ".hpp", ".hh", ".hxx", ".inc", ".inl", ".ipp", ".def"}
SCRIPTS = {
    "migration/tests/run_sharpen_context_sanitizers.py",
    "migration/tests/painter_sanitizer_scope.py",
    "migration/tests/run_filter_owner_context_fixture_test.py",
    "tools/check_sharpen_evidence.py",
}


def operation_config(name):
    if name not in ("sharpen", "threshold-alpha", "sobel"):
        raise ValueError("Unknown native operation: " + str(name))
    instrumented, scripts = set(INSTRUMENTED), set(SCRIPTS)
    fixtures = ["GIMP_PAINTER_SHARPEN_FIXTURES"]
    if name == "threshold-alpha":
        instrumented.add("app/painter/filter-point-shadow.cpp")
        scripts.update({"tools/check_threshold_alpha_evidence.py",
                        "tools/check_threshold_projection_evidence.py"})
        fixtures.extend(["GIMP_PAINTER_THRESHOLD_ALPHA_FIXTURES",
                         "GIMP_PAINTER_THRESHOLD_PROJECTION_FIXTURES"])
    if name == "sobel":
        instrumented.update({"app/painter/filter-sobel.cpp",
                             "app/core/gimpdrawablestack.c",
                             "app/core/gimplayerstack.c",
                             "app/core/gimpprojection.c"})
        scripts.update({"tools/check_sobel_evidence.py",
                        "tools/check_sobel_projection_evidence.py"})
        fixtures.extend(["GIMP_PAINTER_SOBEL_FIXTURES",
                         "GIMP_PAINTER_SOBEL_PROJECTION_FIXTURES"])
    return {"name": name, "instrumented": instrumented, "scripts": scripts,
            "fixtures": fixtures,
            "report_format": name + "-context-sanitizers-v1"}


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def seals(paths):
    return {str(path): sha(path) for path in sorted(set(map(Path, paths)))}


def reseal(before):
    after, errors = {}, {}
    for name in before:
        try:
            after[name] = sha(name)
        except OSError as error:
            after[name] = None
            errors[name] = str(error)
    return {"before": before, "after": after,
            "changed": [name for name in before if before[name] != after[name]],
            "read_errors": errors}


def merge_options(existing, required):
    # Sanitizer flags are processed in order, with the final assignment winning.
    # Keep the caller's string intact (including quoted/colon-containing values,
    # signal handling, symbolizer paths and suppressions), then apply only our
    # explicit safety/termination controls. Never invent new suppressions.
    overrides = ":".join(key + "=" + value for key, value in required.items())
    return existing + (":" if existing else "") + overrides


def argv_for(entry):
    return list(entry.get("arguments") or shlex.split(entry["command"]))


def output_for(entry):
    command = argv_for(entry)
    output = entry.get("output") or command[command.index("-o") + 1]
    return (Path(entry["directory"]) / output).resolve()


def canonical_entry(entries, source, build, linked_objects):
    candidates = []
    for entry in entries:
        if (Path(entry["directory"]) / entry["file"]).resolve() != ROOT / source:
            continue
        output = output_for(entry)
        if not output.is_relative_to(build) or str(output) not in linked_objects:
            continue
        relative = output.relative_to(build)
        if source in HARNESS.values():
            target = TARGETS[next(kind for kind in TARGETS if HARNESS[kind] == source)]
            accepted = relative.parts[:3] == ("app", "tests", target + ".p")
        else:
            accepted = (relative.parts[0] == "app" and "tests" not in relative.parts
                        and any(part.startswith("lib") and part.endswith(".a.p")
                                for part in relative.parts))
        if accepted:
            candidates.append(entry)
    unique = {}
    for entry in candidates:
        key = str(output_for(entry))
        if key in unique and argv_for(unique[key]) != argv_for(entry):
            raise RuntimeError("Conflicting canonical commands for " + key)
        unique[key] = entry
    if len(unique) != 1:
        raise RuntimeError("Expected one canonical production/harness command for "
                           + source + "; found " + str(sorted(unique)))
    return next(iter(unique.values()))


def private_compile(entry, obj, depfile, instrumented):
    command, cleaned, skip = argv_for(entry), [], False
    for arg in command:
        if skip:
            skip = False
            continue
        if arg in ("-MF", "-MQ", "-MT"):
            skip = True
            continue
        if arg in ("-MD", "-MMD", "-MP", "-MG"):
            continue
        if any(arg.startswith(flag) and arg != flag for flag in ("-MF", "-MQ", "-MT")):
            continue
        cleaned.append(arg)
    cleaned[cleaned.index("-o") + 1] = str(obj)
    cleaned += ["-MD", "-MF", str(depfile), "-MT", str(obj)]
    if instrumented:
        cleaned += FLAGS
    if Path(entry["file"]).suffix in CXX_SUFFIXES:
        cleaned += ["-frtti"]
    return cleaned


def verify_lock(fd, path):
    """Verify the inherited open-file description already owns an exclusive lock."""
    expected, inherited = path.stat(), os.fstat(fd)
    if (expected.st_dev, expected.st_ino) != (inherited.st_dev, inherited.st_ino):
        raise RuntimeError("--lock-fd does not refer to " + str(path))
    with path.open("r") as probe:
        try:
            # An exclusive caller lock blocks even an independent shared lock.
            fcntl.flock(probe, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(probe, fcntl.LOCK_UN)
            raise RuntimeError("Caller must hold the exclusive shared build lock")
    # On the same inherited description this is a no-op. Another holder causes
    # EWOULDBLOCK. Do not unlock or close the caller's description on return.
    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    return {"path": str(path), "inherited_fd": fd,
            "device": inherited.st_dev, "inode": inherited.st_ino,
            "exclusive_caller_lock_verified": True}


def tap_result(stdout, requested):
    plans = re.findall(r"^1\.\.(\d+)(?:\s+#.*)?\s*$", stdout, re.MULTILINE)
    rows = re.findall(r"^(ok|not ok)\s+(\d+)\s+(.+?)\s*$", stdout, re.MULTILINE)
    results = []
    for status, number, description in rows:
        path, _, directive = description.partition(" #")
        results.append({"status": status, "number": int(number),
                        "path": path.strip(), "directive": directive.strip()})
    passed = (plans == ["1"] and len(results) == 1
              and results[0] == {"status": "ok", "number": 1,
                                 "path": requested, "directive": ""}
              and not re.search(r"^Bail out!", stdout, re.MULTILINE))
    return {"plan_counts": list(map(int, plans)), "result_count": len(results),
            "results": results, "exact_positive_result": passed}


def local_headers(build):
    # Include newly added headers, even before the ordinary Ninja dependency
    # database has seen their include edge. Existing transitive dependencies
    # (including generated/system headers) are added from `ninja -t deps` below.
    found = set()
    for base, directories, files in os.walk(ROOT):
        directories[:] = [name for name in directories
                          if name not in (".git", "__pycache__")
                          and (Path(base) / name).resolve() != build]
        found.update(Path(base) / name for name in files
                     if Path(name).suffix in HEADER_SUFFIXES)
    return found


class Run:
    def __init__(self, args):
        self.args = args
        self.operation = operation_config(getattr(args, "operation", "sharpen"))
        self.instrumented = self.operation["instrumented"]
        self.started = time.monotonic()
        self.deadline = self.started + args.total_timeout
        self.report = {"report_format": self.operation["report_format"],
                       "operation": self.operation["name"],
                       "status": "running", "root": str(ROOT),
                       "build": str(args.build), "scope": (
                           self.operation["name"] + " kernels and owner-context capture/merge, FilterLayer, "
                           "scheduler/raster/spool storage, bindings, Undo/duplication, "
                           "XCF load/save/typed metadata/compatibility and both C harnesses. "
                           "Listed RTTI-only units are not sanitizer-instrumented; remaining "
                           "GIMP/dependencies are uninstrumented. Leak detection is disabled."),
                       "sanitizers": ["address", "undefined"], "leak_detection": False,
                       "instrumented_cpp_rtti": True, "commands": [],
                       "tests": [{"kind": kind, "path": path, "passed": False,
                                  "exit_code": None, "process_status": "not_run"}
                                 for kind, paths in (("native", args.native_test_path),
                                                     ("xcf", args.xcf_test_path))
                                 for path in paths],
                       "selected_test_paths": {"native": args.native_test_path,
                                               "xcf": args.xcf_test_path},
                       "timeouts_seconds": {"command": args.command_timeout,
                                            "test": args.test_timeout,
                                            "total": args.total_timeout},
                       "instrumented_sources": sorted(self.instrumented),
                       "source_seals": {}, "ordinary_build_seals": {},
                       "executable_seals": {}, "fixture_seals": {},
                       "ordinary_build_seal_scope": (
                           "Canonical replacement inputs, direct link objects, original test "
                           "executables, compile database/build.ninja and build-local archive "
                           "containers. Other unmodified thin-archive members and shared "
                           "libraries are not individually sealed; the caller owns the build lock."),
                       "archive_replacements": []}
        self.inputs, self.ordinary, self.executables, self.fixtures = {}, {}, {}, {}
        self.report["compiled_sources"] = []

    def save(self):
        self.report["elapsed_seconds"] = round(time.monotonic() - self.started, 3)
        self.args.report.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.args.report.with_name(self.args.report.name + ".tmp")
        temporary.write_text(json.dumps(self.report, indent=2) + "\n")
        temporary.replace(self.args.report)

    def command(self, argv, phase, cwd, timeout=None, env=None, check=True):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Overall sanitizer-run timeout before " + phase)
        record = {"phase": phase, "argv": list(map(str, argv)), "cwd": str(cwd),
                  "timeout_seconds": min(timeout or self.args.command_timeout, remaining),
                  "status": "running"}
        self.report["commands"].append(record)
        self.save()
        started = time.monotonic()
        process = None
        try:
            process = subprocess.Popen(record["argv"], cwd=cwd, env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       text=True, errors="replace", start_new_session=True)
            try:
                stdout, stderr = process.communicate(timeout=record["timeout_seconds"])
                record["status"] = "passed" if process.returncode == 0 else "failed"
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                stdout, stderr = process.communicate(timeout=10)
                record["status"] = "timeout"
            record.update(exit_code=process.returncode, stdout=stdout, stderr=stderr)
        except BaseException as error:
            record.update(status="error", error=str(error))
            if process is not None and process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                stdout, stderr = process.communicate(timeout=10)
                record.update(exit_code=process.returncode, stdout=stdout, stderr=stderr)
            raise
        finally:
            record["elapsed_seconds"] = round(time.monotonic() - started, 3)
            self.save()
        if check and record["status"] != "passed":
            raise RuntimeError(phase + " " + record["status"] + "; see command diagnostics")
        return record

    def run(self):
        args, report, build = self.args, self.report, self.args.build
        report["lock"] = verify_lock(args.lock_fd, args.lock_file)
        if not os.environ.get("DISPLAY") or not os.environ.get("XAUTHORITY"):
            raise RuntimeError("Inherit DISPLAY and XAUTHORITY from authenticated secure-Xvfb")
        authority = Path(os.environ["XAUTHORITY"])
        if not authority.is_file() or not os.access(authority, os.R_OK):
            raise RuntimeError("Inherited XAUTHORITY is not a readable file")
        report["display"] = {"DISPLAY": os.environ["DISPLAY"], "XAUTHORITY": str(authority),
                             "verification": "Authentication is owned by the outer secure-Xvfb wrapper; credential contents are never read"}
        env = dict(os.environ)
        mandatory = {"GIMP_TESTING_ABS_TOP_SRCDIR": str(ROOT),
                     "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build)}
        for key, value in mandatory.items():
            if key in env and env[key] != value:
                raise RuntimeError("Inherited " + key + " disagrees with this build: " + env[key])
            env.setdefault(key, value)
        env.setdefault("GIMP_TESTING_PLUGINDIRS", str(build / "plug-ins/common"))
        env.setdefault("UI_TEST", "yes")
        for key, required in {
                "ASAN_OPTIONS": {"detect_leaks": "0", "halt_on_error": "1", "abort_on_error": "1", "exitcode": "1"},
                "UBSAN_OPTIONS": {"halt_on_error": "1", "print_stacktrace": "1", "exitcode": "1"}}.items():
            previous = env.get(key, "")
            env[key] = merge_options(previous, required)
            report[key] = {"inherited": previous, "effective": env[key]}
        report["test_environment"] = {key: value for key, value in env.items()
                                      if key.startswith("GIMP_TESTING_") or
                                      (key.startswith("GIMP_PAINTER_") and key.endswith("FIXTURES"))}
        report["fixture_verification"] = "Outer verified-fixture wrapper owns provenance; this runner seals every selected operation's consumer files"
        fixture_paths = set()
        for key in self.operation["fixtures"]:
            fixture = env.get(key)
            if not fixture or not (Path(fixture) / "captures.tsv").is_file():
                raise RuntimeError("Inherit " + key + " from the verified fixture wrapper")
            fixture_paths.update(path for path in Path(fixture).rglob("*") if path.is_file())
        self.fixtures = seals(fixture_paths)
        output = Path(tempfile.mkdtemp(prefix="sharpen-context-sanitizers-", dir=build))
        report["private_directory"] = str(output)
        entries = json.loads((build / "compile_commands.json").read_text())
        # Use the actual harness object closures, not library-name guesses.
        # Production Painter support libraries are named libpainter-*.a, while
        # the helper also finds a separate filter-worker executable that must
        # never be pulled into either harness merely for its RTTI flag.
        original_links, archive_members, linked_objects = {}, {}, set()
        for kind, target in TARGETS.items():
            text = self.command(["ninja", "-t", "commands", "app/tests/" + target],
                                "read link command " + target, build)["stdout"]
            link = shlex.split(text.strip().splitlines()[-1])
            if any(arg in ("&&", ";", "||") or arg.startswith("@") for arg in link):
                raise RuntimeError("Unsupported shell/response-file link command; refusing an unsafe rewrite")
            original_links[kind] = link
            for arg in link:
                path = (build / arg).resolve()
                if arg.endswith(".o") and path.is_file():
                    linked_objects.add(str(path))
                    self.ordinary.setdefault(str(path), sha(path))
                elif arg.endswith(".a") and path.is_relative_to(build) and path not in archive_members:
                    self.ordinary[str(path)] = sha(path)
                    with path.open("rb") as stream:
                        thin = stream.read(8) == b"!<thin>\n"
                    if not thin:
                        raise RuntimeError("Expected a build-local thin archive: " + str(path))
                    members = self.command(["ar", "t", str(path)], "read archive " + arg, build)["stdout"].splitlines()
                    archive_members[path] = [str((build / member).resolve()) for member in members]
                    linked_objects.update(archive_members[path])
        linked_sources = {(Path(entry["directory"]) / entry["file"]).resolve()
                          for entry in entries if str(output_for(entry)) in linked_objects}
        discovered = bridge_rtti_sources(ROOT, build) - self.instrumented
        rtti_only = {source for source in discovered if ROOT / source in linked_sources}
        report["unlinked_rtti_discoveries"] = {
            source: {"reason": "No object for this source is in either selected harness link closure",
                     "compile_outputs": [str(output_for(entry)) for entry in entries
                                         if (Path(entry["directory"]) / entry["file"]).resolve() == ROOT / source]}
            for source in sorted(discovered - rtti_only)}
        wanted = self.instrumented | rtti_only
        report["rtti_compatibility_only_sources"] = sorted(rtti_only)
        canonical = {source: canonical_entry(entries, source, build, linked_objects) for source in sorted(wanted)}
        report["canonical_compilation"] = {
            source: {"cwd": entry["directory"], "argv": argv_for(entry),
                     "original_object": str(output_for(entry)),
                     "instrumented": source in self.instrumented}
            for source, entry in canonical.items()}
        ordinary = {output_for(entry) for entry in canonical.values()}
        ordinary.update(build / "app/tests" / target for target in TARGETS.values())
        ordinary.update({build / "compile_commands.json", build / "build.ninja"})
        self.ordinary.update(seals(ordinary))
        dependencies = self.command(["ninja", "-t", "deps", *[
            str(output_for(entry).relative_to(build)) for entry in canonical.values()]],
            "read canonical dependencies", build)["stdout"]
        if "deps not found" in dependencies:
            raise RuntimeError("Build ordinary targets first; canonical dependency records are missing")
        include_paths = set()
        for line in dependencies.splitlines():
            if line.startswith("    "):
                include_paths.add((build / line.strip()).resolve())
        if not include_paths:
            raise RuntimeError("No canonical include dependencies found")
        inputs = {ROOT / source for source in wanted | self.operation["scripts"]} | local_headers(build) | include_paths
        self.inputs = seals(inputs)
        report["seal_scope"] = ("All selected sources/scripts, every repository header/include, "
                                "and canonical Ninja transitive dependencies, including generated/system headers. "
                                "New dependencies missing from this precompile seal fail the run.")
        report["source_seals"] = {"before": self.inputs}
        report["ordinary_build_seals"] = {"before": self.ordinary}
        report["fixture_seals"] = {"before": self.fixtures}
        self.save()
        objects, replacements, source_by_object = {}, {}, {}
        for source, entry in canonical.items():
            obj = output / (source.replace("/", "_") + ".o")
            depfile = obj.with_suffix(".d")
            command = private_compile(entry, obj, depfile, source in self.instrumented)
            self.command(command, "compile " + source, Path(entry["directory"]))
            # -MD writes only our private dependency file. Every dependency must
            # have a precompile seal, so a newly introduced include cannot hide.
            text = depfile.read_text().replace("\\\n", "")
            actual = {(Path(entry["directory"]) / name).resolve()
                      for name in shlex.split(text.split(":", 1)[1])}
            missing = sorted(str(path) for path in actual if str(path) not in self.inputs)
            if missing:
                report["unsealed_compile_dependencies"] = {"source": source, "paths": missing}
                raise RuntimeError("Compile encountered dependencies absent from precompile seals")
            objects[source] = obj
            replacements[str(output_for(entry))] = str(obj)
            source_by_object[str(output_for(entry))] = source
            report["compiled_sources"].append(source)
        archives, consumed = {}, set()
        for kind, target in TARGETS.items():
            link = list(original_links[kind])
            exe = output / target
            link[link.index("-o") + 1] = str(exe)
            for index, arg in enumerate(link):
                path = (build / arg).resolve()
                if str(path) in replacements:
                    link[index] = replacements[str(path)]
                    consumed.add(source_by_object[str(path)])
                elif arg.endswith(".a") and path.is_relative_to(build):
                    self.ordinary.setdefault(str(path), sha(path))
                    if path not in archives:
                        absolute = archive_members[path]
                        replaced = [member for member in absolute if member in replacements]
                        if replaced:
                            rewritten = [replacements.get(member, member) for member in absolute]
                            private = output / (str(path.relative_to(build)).replace("/", "_") + ".a")
                            self.command(["ar", "crsT", str(private), *rewritten], "replace thin archive " + arg, build)
                            archives[path] = private
                            report["archive_replacements"].append({"original": str(path), "private": str(private),
                                "sources": sorted({source_by_object[member] for member in replaced})})
                            consumed.update(source_by_object[member] for member in replaced)
                        else:
                            archives[path] = path
                    link[index] = str(archives[path])
                elif arg.endswith(".o") and path.is_file():
                    self.ordinary.setdefault(str(path), sha(path))
            self.command(link + FLAGS, "link " + target, build)
            self.executables[str(exe)] = sha(exe)
        if consumed != wanted:
            report["unreplaced_sources"] = sorted(wanted - consumed)
            raise RuntimeError("Some selected objects were not substituted in either harness link")
        pretest = reseal(self.inputs)
        report["source_seals_before_tests"] = pretest
        if pretest["changed"]:
            raise RuntimeError("Source/header/include/script changed during compilation")
        report["executable_seals"] = {"before": self.executables}
        self.save()
        for test in report["tests"]:
            kind, path = test["kind"], test["path"]
            verify_lock(args.lock_fd, args.lock_file)
            exe = output / TARGETS[kind]
            test.update(process_status="running", command_index=len(report["commands"]))
            result = self.command([str(exe), "--tap", "-p", path], "test " + path,
                                  build, timeout=args.test_timeout, env=env, check=False)
            tap = tap_result(result.get("stdout", ""), path)
            passed = result["status"] == "passed" and tap["exact_positive_result"]
            test.update(passed=passed, exit_code=result.get("exit_code"),
                        process_status=result["status"], tap=tap, executable_sha256=sha(exe))
            self.save()
            print(("PASS " if passed else "FAIL ") + path, flush=True)
        if not report["tests"] or not all(test["passed"] for test in report["tests"]):
            raise RuntimeError("At least one selected exact test failed, timed out, was skipped, or emitted invalid/empty TAP")
        report["status"] = "passed"

    def finish(self):
        for test in self.report["tests"]:
            if test["process_status"] == "running":
                test["process_status"] = "error"
            if test["process_status"] in ("error", "not_run"):
                test["reason"] = self.report.get("error", "Run ended before this test completed")
        for key, before in (("source_seals", self.inputs), ("ordinary_build_seals", self.ordinary),
                            ("executable_seals", self.executables), ("fixture_seals", self.fixtures)):
            self.report[key] = reseal(before)
            if self.report[key]["changed"]:
                self.report["status"] = "failed"
                self.report.setdefault("seal_failures", []).append(key)
        self.save()


def positive_seconds(value):
    number = int(value)
    if not 1 <= number <= 14400:
        raise argparse.ArgumentTypeError("timeout must be between 1 and 14400 seconds")
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build", type=Path)
    parser.add_argument("--operation", choices=("sharpen", "threshold-alpha", "sobel"), default="sharpen")
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--lock-fd", type=int, required=True)
    parser.add_argument("--lock-file", type=Path, default=Path("/workspace/shared/gimp-painter-build.lock"))
    parser.add_argument("--native-test-path", action="append", required=True)
    parser.add_argument("--xcf-test-path", action="append", required=True)
    parser.add_argument("--command-timeout", type=positive_seconds, default=180)
    parser.add_argument("--test-timeout", type=positive_seconds, default=300)
    parser.add_argument("--total-timeout", type=positive_seconds, default=3600)
    args = parser.parse_args(argv)
    args.build, args.report, args.lock_file = args.build.resolve(), args.report.resolve(), args.lock_file.resolve()
    if args.report.suffix != ".json":
        parser.error("--report must name a dedicated .json report, never a build object or executable")
    if args.report.exists():
        try:
            previous = json.loads(args.report.read_text())
        except (ValueError, OSError):
            previous = {}
        if not isinstance(previous, dict) or previous.get("report_format") != operation_config(args.operation)["report_format"]:
            parser.error("Refusing to overwrite an existing file that is not this runner's report")
    run = Run(args)
    try:
        run.save()
        for kind, paths in (("native", args.native_test_path), ("xcf", args.xcf_test_path)):
            for path in paths:
                if not re.fullmatch("/" + re.escape(TARGETS[kind]) + r"/[A-Za-z0-9_-]+", path):
                    raise ValueError("Expected an exact /" + TARGETS[kind] + "/CASE path: " + path)
        run.run()
    except BaseException as error:
        run.report.update(status="failed", error=str(error), exception=type(error).__name__,
                          traceback=traceback.format_exc())
        print(str(error), file=sys.stderr)
    finally:
        run.finish()
    print("Report: " + str(args.report), flush=True)
    return 0 if run.report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

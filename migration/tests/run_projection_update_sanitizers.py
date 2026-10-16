#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run bounded projection-update geometry acceptance under ASan and UBSan.

Build update-geometry and projection-update-bounds normally first. After the
native lane is released, run inside the existing authenticated display wrapper:

  bash -c 'exec 9>/workspace/shared/gimp-painter-build.lock; flock -x 9;
    python3 migration/tests/run_projection_update_sanitizers.py BUILD \
      --lock-fd 9 --report REPORT.json'

Five C translation units are instrumented: three production invalidation owners
and the pure/native test harnesses. This does not instrument other GIMP sources,
dependencies or C++ RTTI, and disables leak detection. No filter fixtures are
required. Objects, depfiles, replacement thin archives and executables live in a
fresh private directory. Ordinary outputs are never rebuilt/replaced by this
runner. It checks the inherited exclusive lock, seals selected inputs and
ordinary outputs, and requires an exact non-skipped TAP result for every case.
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


ROOT = Path(__file__).resolve().parents[2]
FORMAT = "projection-update-sanitizers-v1"
FLAGS = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
         "-fno-omit-frame-pointer", "-O1"]
SOURCES = {
    "app/core/gimpdrawablestack.c",
    "app/core/gimplayerstack.c",
    "app/core/gimpprojection.c",
    "app/tests/test-update-geometry.c",
    "app/tests/test-projection-update-bounds.c",
}
CASES = {
    "update-geometry": [
        "ordinary-grid-equivalence", "translation-bounds", "extreme-grid-bounds",
        "unrounded-intersection-bounds", "grid-rounding"],
    "projection-update-bounds": [
        "projectable-grid-updates", "extreme-layer-lifecycle",
        "projection-offset-boundaries"],
}


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
    return {"before": before, "after": after, "read_errors": errors,
            "changed": [name for name in before if before[name] != after[name]]}


def verify_lock(fd, path):
    expected, inherited = path.stat(), os.fstat(fd)
    if (expected.st_dev, expected.st_ino) != (inherited.st_dev, inherited.st_ino):
        raise RuntimeError("Inherited FD is not the shared build lock")
    with path.open("r") as probe:
        try:
            fcntl.flock(probe, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(probe, fcntl.LOCK_UN)
            raise RuntimeError("Caller must already hold the exclusive build lock")
    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    return {"path": str(path), "fd": fd, "exclusive_caller_lock_verified": True}


def argv_for(entry):
    return list(entry.get("arguments") or shlex.split(entry["command"]))


def object_for(entry):
    argv = argv_for(entry)
    return (Path(entry["directory"]) /
            (entry.get("output") or argv[argv.index("-o") + 1])).resolve()


def compile_command(entry, obj, depfile):
    cleaned, skip = [], False
    for arg in argv_for(entry):
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
    return cleaned + ["-MD", "-MF", str(depfile), "-MT", str(obj)] + FLAGS


def exact_tap(text, path):
    plans = re.findall(r"^1\.\.(\d+)([^\r\n]*)$", text, re.MULTILINE)
    rows = re.findall(r"^(ok|not ok)\s+(\d+)\s+(.+?)\s*$", text, re.MULTILINE)
    return (len(plans) == 1 and plans[0][0] == "1" and not plans[0][1].strip() and
            rows == [("ok", "1", path)] and
            not re.search(r"^Bail out!", text, re.MULTILINE))


def test_outcome(result, path):
    stdout, stderr = result.get("stdout", ""), result.get("stderr", "")
    diagnostic = bool(re.search(
        r"AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|runtime error:|"
        r"\b(?:CRITICAL|WARNING)\b|Segmentation fault|core dumped|Assertion .* failed|"
        r"\b(?:SIGSEGV|SIGABRT|DEADLYSIGNAL)\b",
        stdout + "\n" + stderr, re.IGNORECASE))
    valid = exact_tap(stdout, path)
    return {"exact_positive_tap": valid, "diagnostic_failure": diagnostic,
            "passed": result["status"] == "passed" and result.get("exit_code") == 0 and
                      valid and not diagnostic}


def merge_options(existing, required):
    # Preserve the caller's options verbatim; final required assignments win.
    return existing + (":" if existing else "") + required


class Run:
    def __init__(self, args):
        self.args = args
        self.started = time.monotonic()
        self.deadline = self.started + args.total_timeout
        self.inputs, self.ordinary, self.executables = {}, {}, {}
        self.report = {
            "report_format": FORMAT, "status": "running", "root": str(ROOT),
            "build": str(args.build), "instrumented_sources": sorted(SOURCES),
            "sanitizers": ["address", "undefined"], "leak_detection": False,
            "scope": "Three production invalidation C units and two C harnesses; "
                     "private inline helper is instrumented in its callers. Other "
                     "GIMP/dependencies, C++ and leak detection are outside coverage. "
                     "Real top-level layer lifecycle plus synthetic projectable "
                     "coordinate boundary tests; no global/group/show-all safety claim.",
            "ordinary_seal_scope": "Canonical replacement objects, direct link objects, "
                                   "build-local archive containers, original executables, "
                                   "compile_commands.json and build.ninja. Other thin "
                                   "archive members and shared dependencies are unsealed.",
            "commands": [], "archive_replacements": [], "compiled_sources": [],
            "tests": [{"target": target, "path": "/" + target + "/" + case,
                       "passed": False, "status": "not_run"}
                      for target, cases in CASES.items() for case in cases],
        }

    def save(self):
        self.report["elapsed_seconds"] = round(time.monotonic() - self.started, 3)
        self.args.report.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.args.report.with_name(self.args.report.name + ".tmp")
        temporary.write_text(json.dumps(self.report, indent=2) + "\n")
        temporary.replace(self.args.report)

    def command(self, argv, phase, cwd, env=None, test=False):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Overall deadline exceeded before " + phase)
        record = {"argv": list(map(str, argv)), "phase": phase, "cwd": str(cwd),
                  "status": "running", "timeout": min(remaining, self.args.command_timeout)}
        self.report["commands"].append(record)
        self.save()
        process = None
        try:
            process = subprocess.Popen(record["argv"], cwd=cwd, env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       text=True, errors="replace", start_new_session=True)
            try:
                stdout, stderr = process.communicate(timeout=record["timeout"])
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
            self.save()
        if not test and record["status"] != "passed":
            raise RuntimeError(phase + " failed; see checkpointed command diagnostics")
        return record

    def run(self):
        args, build = self.args, self.args.build
        self.report["lock"] = verify_lock(args.lock_fd, args.lock_file)
        if not os.environ.get("DISPLAY") or not os.environ.get("XAUTHORITY"):
            raise RuntimeError("Run inside the existing authenticated display wrapper")
        if not os.access(os.environ["XAUTHORITY"], os.R_OK):
            raise RuntimeError("Inherited XAUTHORITY is not readable")
        self.report["display"] = {"DISPLAY": os.environ["DISPLAY"],
                                  "authentication": "Owned by outer wrapper; credentials not read"}
        env = dict(os.environ)
        for key, value in {"GIMP_TESTING_ABS_TOP_SRCDIR": str(ROOT),
                           "GIMP_TESTING_ABS_TOP_BUILDDIR": str(build)}.items():
            if key in env and env[key] != value:
                raise RuntimeError("Inherited " + key + " belongs to another build")
            env[key] = value
        env.setdefault("GIMP_TESTING_PLUGINDIRS", str(build / "plug-ins/common"))
        env.setdefault("UI_TEST", "yes")
        for key, required in {
            "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1:abort_on_error=1:exitcode=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1:exitcode=1",
        }.items():
            previous = env.get(key, "")
            env[key] = merge_options(previous, required)
            self.report[key] = {"inherited": previous, "effective": env[key]}

        output = Path(tempfile.mkdtemp(prefix="projection-update-sanitizers-", dir=build))
        self.report["private_directory"] = str(output)
        entries = json.loads((build / "compile_commands.json").read_text())
        links, archives, linked_objects = {}, {}, set()
        for target in CASES:
            command = self.command(["ninja", "-t", "commands", "app/tests/" + target],
                                   "read link " + target, build)["stdout"]
            link = shlex.split(command.strip().splitlines()[-1])
            if any(arg in ("&&", ";", "||") or arg.startswith("@") for arg in link):
                raise RuntimeError("Unsupported shell/response-file link command")
            links[target] = link
            for arg in link:
                path = (build / arg).resolve()
                if arg.endswith(".o") and path.is_file():
                    linked_objects.add(path)
                    self.ordinary[str(path)] = sha(path)
                elif arg.endswith(".a") and path.is_relative_to(build) and path not in archives:
                    with path.open("rb") as stream:
                        if stream.read(8) != b"!<thin>\n":
                            raise RuntimeError("Expected build-local thin archive: " + str(path))
                    self.ordinary[str(path)] = sha(path)
                    members = self.command(["ar", "t", str(path)], "read archive " + arg,
                                           build)["stdout"].splitlines()
                    archives[path] = [(build / member).resolve() for member in members]
                    linked_objects.update(archives[path])

        canonical = {}
        for source in sorted(SOURCES):
            candidates = {object_for(entry): entry for entry in entries
                          if (Path(entry["directory"]) / entry["file"]).resolve() == ROOT / source
                          and object_for(entry) in linked_objects}
            if len(candidates) != 1:
                raise RuntimeError("Expected one linked canonical command for " + source)
            canonical[source] = next(iter(candidates.values()))
        self.report["canonical_compilation"] = {
            source: {"argv": argv_for(entry), "cwd": entry["directory"],
                     "ordinary_object": str(object_for(entry))}
            for source, entry in canonical.items()}
        self.ordinary.update(seals({object_for(entry) for entry in canonical.values()} |
                                  {build / "app/tests" / target for target in CASES} |
                                  {build / "compile_commands.json", build / "build.ninja"}))
        dependencies = self.command(["ninja", "-t", "deps", *[
            str(object_for(entry).relative_to(build)) for entry in canonical.values()]],
            "read selected dependencies", build)["stdout"]
        if "deps not found" in dependencies:
            raise RuntimeError("Build ordinary targets first; dependency records are missing")
        headers = {(build / line.strip()).resolve() for line in dependencies.splitlines()
                   if line.startswith("    ")}
        if not headers:
            raise RuntimeError("No selected dependency records found")
        self.inputs = seals(headers | {ROOT / source for source in SOURCES} | {
            Path(__file__).resolve(), ROOT / "app/core/gimp-update-geometry.h",
            ROOT / "app/core/meson.build", ROOT / "app/tests/meson.build",
            ROOT / "migration/inventory/additional-c-header-routes.json"})
        self.report["source_seals"] = {"before": self.inputs}
        self.report["ordinary_build_seals"] = {"before": self.ordinary}
        self.report["dependency_seal_scope"] = (
            "Selected Ninja transitive dependencies, including generated/system headers, "
            "selected sources, runner and registration inputs; new compile dependencies fail closed.")
        self.save()

        replacements, by_object = {}, {}
        for source, entry in canonical.items():
            obj = output / (source.replace("/", "_") + ".o")
            depfile = obj.with_suffix(".d")
            self.command(compile_command(entry, obj, depfile), "compile " + source,
                         Path(entry["directory"]))
            dependency_text = depfile.read_text().replace("\\\n", "")
            actual = {(Path(entry["directory"]) / name).resolve()
                      for name in shlex.split(dependency_text.split(":", 1)[1])}
            missing = sorted(str(path) for path in actual if str(path) not in self.inputs)
            if missing:
                self.report["unsealed_dependencies"] = {"source": source, "paths": missing}
                raise RuntimeError("Compile added dependencies outside precompile seals")
            replacements[object_for(entry)] = str(obj)
            by_object[object_for(entry)] = source
            self.report["compiled_sources"].append(source)

        private_archives, consumed = {}, set()
        for target, original in links.items():
            link = list(original)
            exe = output / target
            link[link.index("-o") + 1] = str(exe)
            for index, arg in enumerate(link):
                path = (build / arg).resolve()
                if path in replacements:
                    link[index] = replacements[path]
                    consumed.add(by_object[path])
                elif path in archives:
                    if path not in private_archives:
                        selected = [member for member in archives[path] if member in replacements]
                        if selected:
                            private = output / (str(path.relative_to(build)).replace("/", "_") + ".a")
                            rewritten = [replacements.get(member, str(member)) for member in archives[path]]
                            self.command(["ar", "crsT", str(private), *rewritten],
                                         "replace archive " + str(path), build)
                            private_archives[path] = private
                            consumed.update(by_object[member] for member in selected)
                            self.report["archive_replacements"].append({
                                "original": str(path), "private": str(private),
                                "sources": sorted({by_object[member] for member in selected})})
                        else:
                            private_archives[path] = path
                    link[index] = str(private_archives[path])
            self.command(link + FLAGS, "link " + target, build)
            self.executables[str(exe)] = sha(exe)
        if consumed != SOURCES:
            raise RuntimeError("Not every instrumented source was substituted in link closures")
        self.report["source_seals_before_tests"] = reseal(self.inputs)
        if self.report["source_seals_before_tests"]["changed"]:
            raise RuntimeError("Source/dependency changed during build")
        self.report["executable_seals"] = {"before": self.executables}
        self.save()
        for test in self.report["tests"]:
            verify_lock(args.lock_fd, args.lock_file)
            test.update(status="running", command_index=len(self.report["commands"]))
            result = self.command([str(output / test["target"]), "--tap", "-p", test["path"]],
                                  "test " + test["path"], build, env=env, test=True)
            test.update(status=result["status"], exit_code=result.get("exit_code"),
                        **test_outcome(result, test["path"]))
            self.save()
            print(("PASS " if test["passed"] else "FAIL ") + test["path"], flush=True)
        if not all(test["passed"] for test in self.report["tests"]):
            raise RuntimeError("Selected test failed, timed out, skipped, or emitted invalid TAP")
        self.report["status"] = "passed"

    def finish(self):
        for name, before in (("source_seals", self.inputs),
                             ("ordinary_build_seals", self.ordinary),
                             ("executable_seals", self.executables)):
            self.report[name] = reseal(before)
            if self.report[name]["changed"]:
                self.report["status"] = "failed"
        for test in self.report["tests"]:
            if test["status"] in ("running", "not_run"):
                test.update(status="not_completed", reason=self.report.get("error"))
        self.save()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("build", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--lock-fd", type=int, required=True)
    parser.add_argument("--lock-file", type=Path, default=Path("/workspace/shared/gimp-painter-build.lock"))
    parser.add_argument("--command-timeout", type=int, default=180)
    parser.add_argument("--total-timeout", type=int, default=1800)
    args = parser.parse_args()
    args.build, args.report, args.lock_file = args.build.resolve(), args.report.resolve(), args.lock_file.resolve()
    if not 1 <= args.command_timeout <= 14400 or not 1 <= args.total_timeout <= 14400:
        parser.error("Timeouts must be between 1 and 14400 seconds")
    if args.report.suffix != ".json":
        parser.error("Use a dedicated .json report, never a build object or executable")
    if args.report.exists():
        try:
            previous = json.loads(args.report.read_text())
        except (ValueError, OSError):
            previous = {}
        if not isinstance(previous, dict) or previous.get("report_format") != FORMAT:
            parser.error("Refusing to overwrite a file that is not this runner's report")
    run = Run(args)
    try:
        run.save()
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

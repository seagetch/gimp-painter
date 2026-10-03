#!/usr/bin/env python3
"""Prove console Quit drains real, already-running Blinds jobs.

The batch script first announces readiness. The external observer checks there
are no startup workers, acknowledges readiness, and only then allows explicit
source.update() invalidations. Quit is sent only after matching live helper and
native Blinds descendants. One-image and two-image trials use separate consoles.
This Linux test inspects only descendants of its own wrapper and recorded PIDs.
Use --executables to observe an instrumented console/helper/Blinds overlay;
fixture generation and runtime resources still come from the ordinary build.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
SOURCE_FILES = ("app/tests/test-filter-quit-fixture.cpp", "app/core/gimpfilterlayer.cpp",
                "app/core/gimpfilterexit.cpp", "app/core/gimpfilterprocedure.cpp",
                "app/painter/filter-process.cpp", "app/painter/filter-lifetime.cpp",
                "app/painter/filter-scheduler.cpp", "app/painter/filter-spool.cpp",
                "app/painter/filter-raster.cpp", "app/painter/filter-wire.cpp",
                "app/painter/filter-procedure.hpp", "app/core/gimpfilterprocedure.hpp",
                "app/config/gimppainterfilterconfig.cpp", "tools/check_filter_active_quit.py",
                "tools/in-build-gimp.py", "app/painter-filter-worker.cpp", "app/app.c",
                "app/core/gimp-batch.c", "app/core/gimp.c", "app/gimpcoreapp.c")
SANITIZER_DIAGNOSTICS = ("ERROR: AddressSanitizer", "SUMMARY: AddressSanitizer",
                         "AddressSanitizer:DEADLYSIGNAL", "runtime error:",
                         "WARNING: ThreadSanitizer", "ERROR: LeakSanitizer")


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def record(pid):
    root = Path("/proc") / str(pid)
    try:
        if root.stat().st_uid != os.getuid():
            return None
        values = (root / "stat").read_text().rsplit(")", 1)[1].split()
        command = (root / "cmdline").read_bytes().split(b"\0")
        return dict(pid=pid, state=values[0], parent=int(values[1]), group=int(values[2]),
                    start_ticks=int(values[19]), exe=os.readlink(root / "exe"),
                    argv=[part.decode(errors="replace") for part in command if part])
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        return None


def descendants(root_pid):
    result, pending = {}, [root_pid]
    while pending:
        parents = []
        for pid in pending:
            if pid in result:
                continue
            item = record(pid)
            if item is not None:
                result[pid] = item
                parents.append(pid)
        if not parents:
            break
        # Some kernels omit task/<tid>/children. Ask ps only for the PIDs of
        # children of already-established descendants; read executable/argv
        # details through /proc only after this relationship is established.
        query = subprocess.run(["/usr/bin/ps", "-o", "pid=", "--ppid",
                                ",".join(str(pid) for pid in parents)],
                               capture_output=True, text=True, check=False)
        if query.returncode not in (0, 1):
            raise RuntimeError("Cannot inventory owned child PIDs: " + query.stderr)
        pending = [int(pid) for pid in query.stdout.split()]
    return result


def still_same(item):
    current = record(item["pid"])
    return current is not None and current["start_ticks"] == item["start_ticks"]


def group_alive(group):
    try:
        os.killpg(group, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def events(path):
    if not path.exists():
        return []
    result = []
    for line in path.read_text().splitlines():
        try:
            result.append(json.loads(line))
        except json.JSONDecodeError:
            pass  # writer has not yet flushed the last complete record
    return result


def batch_code(event_path, start_path, quit_path, jobs):
    return f'''import json, os, time
from pathlib import Path
from gi.repository import Gimp
EVENTS = Path({str(event_path)!r})
def emit(kind, **extra):
    with EVENTS.open("a") as stream:
        stream.write(json.dumps(dict(event=kind, monotonic=time.monotonic(), pid=os.getpid(), **extra)) + "\\n")
        stream.flush()
def await_file(filename):
    deadline = time.monotonic() + 35
    while not Path(filename).exists():
        if time.monotonic() >= deadline:
            emit("BATCH_TIMEOUT", waiting=filename)
            raise RuntimeError("External active Quit observer did not authorize next step")
        time.sleep(0.002)
images = Gimp.get_images()
if len(images) != {jobs}:
    raise RuntimeError("Expected {jobs} independent loaded images, found " + str(len(images)))
sources = []
for image in images:
    source = next((layer for layer in image.get_layers() if layer.get_name() == "quit source"), None)
    effect = next((layer for layer in image.get_layers() if layer.get_name() == "quit Blinds"), None)
    if source is None or effect is None:
        raise RuntimeError("Fixture did not restore its two named layers")
    sources.append((image, source))
emit("READY", images=[image.get_id() for image, source in sources])
await_file({str(start_path)!r})
for image, source in sources:
    if not source.update(0, 0, image.get_width(), image.get_height()):
        raise RuntimeError("Explicit source.update failed")
    emit("INVALIDATED", image=image.get_id(), drawable=source.get_id())
emit("ALL_INVALIDATED", jobs={jobs})
await_file({str(quit_path)!r})
emit("QUIT_REQUEST", jobs={jobs})
quit_procedure = Gimp.get_pdb().lookup_procedure("gimp-quit")
quit_config = quit_procedure.create_config()
quit_config.set_property("force", True)
quit_procedure.run(quit_config)
emit("QUIT_RETURNED")
'''


def run_trial(jobs, fixtures, out, environment, executables, dimension, timeout):
    folder = out / f"jobs-{jobs}"
    folder.mkdir()
    with tempfile.TemporaryDirectory(prefix=f"active-quit-{jobs}-") as temporary:
        runtime = Path(temporary)
        swap = runtime / "swap"
        swap.mkdir()
        event_path, start_path, quit_path = (runtime / name for name in ("events.jsonl", "start", "quit"))
        script = folder / "batch.py"
        script.write_text(batch_code(event_path, start_path, quit_path, jobs))
        rc = runtime / "gimprc"
        rc.write_text("(temp-path " + json.dumps(str(swap)) + ")\n" +
                      "(swap-path " + json.dumps(str(swap)) + ")\n" +
                      '(script-fu-path "${gimp_dir}/scripts")\n')
        command = [sys.executable, str(ROOT / "tools/in-build-gimp.py"), "--new-instance",
                   "--no-interface", "--no-data", "--no-fonts", "--no-splash", "--gimprc", str(rc),
                   "--batch-interpreter=python-fu-eval", "-b",
                   "exec(compile(open(" + repr(str(script)) + ").read(), " + repr(str(script)) + ", 'exec'))"]
        command += [str(path) for path in fixtures[:jobs]]
        helper_exe = str(executables["helper"])
        blinds_exe = str(executables["blinds"])
        began = time.monotonic()
        observed, matching, profiles = {}, [], set()
        ready_seen = False
        quit_authorized = None
        main_pid = None
        errors = []
        with (folder / "console.log").open("wb") as log:
            process = subprocess.Popen(command, env=environment, stdout=log,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                while process.poll() is None:
                    now = time.monotonic()
                    if now - began > timeout:
                        errors.append("Console did not reach bounded completion")
                        break
                    current = descendants(process.pid)
                    observed.update({(item["pid"], item["start_ticks"]): item for item in current.values()})
                    mains = [item for item in current.values() if item["exe"] == environment["GIMP_SELF_IN_BUILD"]]
                    if mains:
                        main_pid = mains[0]["pid"]
                    helpers = [item for item in current.values() if item["exe"] == helper_exe and
                               item["state"] != "Z" and len(item["argv"]) == 3 and
                               item["argv"][1] == "--filter-worker-v1"]
                    for item in helpers:
                        profiles.add(item["argv"][2])
                    seen = events(event_path)
                    if any(item["event"] == "BATCH_TIMEOUT" for item in seen):
                        errors.append("Batch observer handshake timed out")
                        break
                    if not ready_seen and any(item["event"] == "READY" for item in seen):
                        if helpers:
                            errors.append("Worker was already active at READY; would be startup-only proof")
                            break
                        ready_seen = True
                        start_path.write_text("explicit update authorized\n")
                    invalidations = [item for item in seen if item["event"] == "INVALIDATED"]
                    if quit_authorized is None and len(invalidations) == jobs and len(helpers) >= jobs:
                        native = [item for item in current.values() if item["exe"] == blinds_exe and
                                  item["state"] != "Z" and item["parent"] in {h["pid"] for h in helpers}]
                        parents = {item["parent"] for item in native}
                        live = [item for item in helpers if item["pid"] in parents]
                        if len(live) >= jobs:
                            matching = live
                            native_at_quit = native
                            quit_authorized = now
                            quit_path.write_text("live private/native children observed; Quit authorized\n")
                    if quit_authorized is not None and now - quit_authorized > 4.5:
                        errors.append("Quit did not finish before the 5-second fallback boundary")
                        break
                    time.sleep(0.005)
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGKILL)
                code = process.wait(timeout=5)
            finally:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
        exited = time.monotonic()
        recorded_events = events(event_path)
        (folder / "events.json").write_text(json.dumps(recorded_events, indent=2) + "\n")
        quit_events = [item for item in recorded_events if item["event"] == "QUIT_REQUEST"]
        elapsed_after_quit = exited - quit_events[0]["monotonic"] if quit_events else None
        if not ready_seen or not matching or quit_authorized is None or not quit_events:
            errors.append("Missing READY, explicit invalidation, actual active child, or Quit evidence")
        if code != 0:
            errors.append(f"Console/wrapper exit status {code}")
        if elapsed_after_quit is None or elapsed_after_quit >= 4.5:
            errors.append("Measured Quit exceeded bound or was not measured")
        text = (folder / "console.log").read_text(errors="replace")
        for forbidden in ("Filter cleanup exceeded", "explicit fallback", "Cannot schedule Filter cleanup drain",
                          "batch command experienced", "Traceback (most recent call last)", "Segmentation fault") + SANITIZER_DIAGNOSTICS:
            if forbidden in text:
                errors.append("Console diagnostic: " + forbidden)
        survivors = [item for item in observed.values() if still_same(item)]
        groups = sorted({item["group"] for item in observed.values()})
        live_groups = [group for group in groups if group_alive(group)]
        leftovers = [path for path in profiles if Path(path).exists()]
        private_leftovers = [str(path) for path in swap.glob("gimp-filter-*")]
        if survivors or live_groups:
            errors.append("Observed child processes or their process groups survived Quit")
        if leftovers or private_leftovers:
            errors.append("Private worker profiles survived Quit")
        # Failure cleanup is limited to process groups created by this test.
        if errors:
            for group in live_groups:
                if not any(item["group"] == group and still_same(item) for item in observed.values()):
                    continue  # never signal a group whose known identities all vanished
                try:
                    os.killpg(group, signal.SIGKILL)
                except ProcessLookupError:
                    pass
        return dict(jobs=jobs, dimension=dimension, status="passed" if not errors else "failed",
                    command=command, main_pid=main_pid, wrapper_pid=process.pid, exit_code=code,
                    quit_seconds=elapsed_after_quit, total_seconds=exited - began,
                    active_helpers_at_quit=matching,
                    native_children_at_quit=native_at_quit if matching else [],
                    observed_processes=list(observed.values()), observed_groups=groups,
                    survivors=survivors, surviving_groups=live_groups,
                    private_profiles=sorted(profiles), leftover_profiles=leftovers + private_leftovers,
                    events_file=f"jobs-{jobs}/events.json", console_log=f"jobs-{jobs}/console.log",
                    console_log_sha256=sha(folder / "console.log"),
                    batch_source_sha256=sha(script), errors=errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "build-debian13")
    parser.add_argument("--executables", type=Path,
                        help="Directory containing gimp-console-3.0, gimp-painter-filter-worker, and blinds")
    parser.add_argument("--output", type=Path, default=ROOT / "migration/tests/filter-active-quit")
    parser.add_argument("--dimension", type=int, choices=(2048, 4096), default=4096)
    parser.add_argument("--timeout", type=float, default=75)
    parser.add_argument("--lock", type=Path, default=Path("/workspace/shared/gimp-painter-build.lock"))
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    if out.exists():
        raise SystemExit("Use a new --output directory; prior Quit evidence is never overwritten")
    out.mkdir(parents=True)
    tests = json.loads((build / "meson-info/intro-tests.json").read_text())
    configuration = next(test["env"] for test in tests if test["name"] == "script-fu-startup")
    env = os.environ.copy()
    for key, value in configuration.items():
        if key in ("LD_LIBRARY_PATH", "GI_TYPELIB_PATH"):
            env[key] = value + os.pathsep + env.get(key, "")
        else:
            env[key] = value
    generator = build / "app/tests/filter-quit-fixture"
    executable_directory = args.executables.resolve() if args.executables else None
    executables = dict(fixture_generator=generator,
                       console=build / "app/gimp-console-3.0",
                       helper=build / "app/gimp-painter-filter-worker",
                       blinds=build / "plug-ins/common/blinds")
    if executable_directory is not None:
        executables.update(console=executable_directory / "gimp-console-3.0",
                           helper=executable_directory / "gimp-painter-filter-worker",
                           blinds=executable_directory / "blinds")
        # Keep caller-supplied sanitizer options, including detect_leaks=0.
        # The console/helper must have been linked to these exact overlay paths;
        # descendant matching below rejects any fallback to ordinary binaries.
        env["ASAN_OPTIONS"] = os.environ.get("ASAN_OPTIONS", "detect_leaks=0")
    env["GIMP_SELF_IN_BUILD"] = str(executables["console"])
    binaries = list(executables.values())
    def binary_hashes():
        return {str(path.relative_to(build) if path.is_relative_to(build) else path): sha(path)
                for path in binaries}
    with args.lock.open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        started = datetime.now(timezone.utc).isoformat()
        for path in binaries:
            if not path.is_file() or not os.access(path, os.X_OK):
                raise SystemExit("Missing executable: " + str(path))
        hashes = binary_hashes()
        source_hashes = {name: sha(ROOT / name) for name in SOURCE_FILES}
        fixtures = out / "fixtures"
        fixtures.mkdir()
        with tempfile.TemporaryDirectory(prefix="quit-fixture-profile-") as temporary:
            generator_env = env.copy()
            generator_env.update(GIMP3_DIRECTORY=temporary, GIMP3_DATADIR=temporary,
                                 GIMP3_CACHEDIR=temporary, GIMP3_TEMPDIR=temporary,
                                 GIMP_TESTING_PLUGINDIRS=temporary,
                                 GIMP_TESTING_INTERPRETER_DIRS=temporary,
                                 GIMP_TESTING_ENVIRON_DIRS=temporary)
            command = [str(generator), str(fixtures), str(args.dimension)]
            with (out / "fixture-generator.log").open("wb") as log:
                generated = subprocess.run(command, env=generator_env, stdout=log,
                                           stderr=subprocess.STDOUT, timeout=90)
        if generated.returncode != 0:
            raise SystemExit("Quit fixture generation failed; inspect " + str(out / "fixture-generator.log"))
        fixture_files = [fixtures / f"quit-blinds-{index}.xcf" for index in (1, 2)]
        trials = [run_trial(jobs, fixture_files, out, env, executables, args.dimension, args.timeout)
                  for jobs in (1, 2)]
        base_command = [sys.executable, str(ROOT / "tools/in-build-gimp.py"), "--new-instance",
                        "--no-interface", "--no-data", "--no-fonts", "--no-splash"]
        definitions = (
            ("explicit-quit-no-job", 0, ["--batch-interpreter=python-fu-eval", "-b",
                "from gi.repository import Gimp; p=Gimp.get_pdb().lookup_procedure('gimp-quit'); c=p.create_config(); c.set_property('force', True); print('QUIT_EMPTY_CONTROL_READY', flush=True); p.run(c)"]),
            ("successful-batch", 0, ["--batch-interpreter=python-fu-eval", "-b",
                "print('QUIT_SUCCESSFUL_BATCH_CONTROL', flush=True)", "--quit"]),
            ("failed-batch", 64, ["--batch-interpreter=python-fu-eval", "-b",
                "raise RuntimeError('QUIT_EXPECTED_BATCH_FAILURE')", "--quit"]),
            ("missing-interpreter", 69, ["--batch-interpreter=gimp-painter-nonexistent-interpreter",
                "-b", "no-op", "--quit"]),
        )
        controls = []
        for name, expected, arguments in definitions:
            control_command = base_command + arguments
            control_start = time.monotonic()
            control_timeout = False
            with (out / (name + ".log")).open("wb") as log:
                control = subprocess.Popen(control_command, env=env, stdout=log,
                                           stderr=subprocess.STDOUT, start_new_session=True)
                try:
                    control.wait(timeout=60)
                except subprocess.TimeoutExpired:
                    control_timeout = True
                    os.killpg(control.pid, signal.SIGKILL)
                    control.wait()
            control_text = (out / (name + ".log")).read_text(errors="replace")
            controls.append(dict(name=name, command=control_command, expected_exit_code=expected,
                                 actual_exit_code=control.returncode, seconds=time.monotonic() - control_start,
                                 timed_out=control_timeout,
                                 log_sha256=sha(out / (name + ".log")),
                                 sanitizer_diagnostics=[item for item in SANITIZER_DIAGNOSTICS
                                                        if item in control_text],
                                 fallback="Filter cleanup exceeded" in control_text or "explicit fallback" in control_text,
                                 explicit_quit_ready=(name != "explicit-quit-no-job" or
                                                      "QUIT_EMPTY_CONTROL_READY" in control_text.splitlines())))
        after_hashes = binary_hashes()
        after_source_hashes = {name: sha(ROOT / name) for name in SOURCE_FILES}
        finished = datetime.now(timezone.utc).isoformat()
    errors = [f"jobs-{trial['jobs']}: {error}" for trial in trials for error in trial["errors"]]
    # The real batch driver preserves EX_USAGE64 for Python CALLING_ERROR and
    # EX_UNAVAILABLE69 for a missing interpreter, including with --no-fonts.
    for control in controls:
        if control["actual_exit_code"] != control["expected_exit_code"] or control["fallback"] or not control["explicit_quit_ready"]:
            errors.append(f"Batch control failed: {control['name']} expected {control['expected_exit_code']}, got {control['actual_exit_code']}")
        if control["sanitizer_diagnostics"]:
            errors.append(f"Batch control sanitizer diagnostic: {control['name']}: {control['sanitizer_diagnostics']}")
    if hashes != after_hashes:
        errors.append("Test executables changed while proof was running")
    if source_hashes != after_source_hashes:
        errors.append("Test sources changed while proof was running")
    report = dict(schema_version=1, status="passed" if not errors else "failed",
                  started_utc=started, finished_utc=finished,
                  executable_directory=str(executable_directory) if executable_directory else None,
                  executable_paths={name: str(path) for name, path in executables.items()},
                  sanitizer_environment={name: env[name] for name in ("ASAN_OPTIONS", "UBSAN_OPTIONS")
                                         if name in env},
                  executable_sha256=hashes, executable_sha256_after=after_hashes,
                  source_sha256=source_hashes, source_sha256_after=after_source_hashes,
                  observer_sha256=source_hashes["tools/check_filter_active_quit.py"], fixture_command=command,
                  fixture_sha256={path.name: sha(path) for path in fixture_files}, trials=trials,
                  batch_controls=controls,
                  errors=errors)
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"], [dict(jobs=t["jobs"], quit_seconds=t["quit_seconds"], errors=t["errors"]) for t in trials])
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())

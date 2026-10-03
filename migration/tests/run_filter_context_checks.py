#!/usr/bin/env python3
"""Run focused scalar/context-fixture checks with exact source provenance."""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

from filter_context_fixture_bundle import materialize

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ("app/painter/filter-context.cpp", "app/painter/filter-context.hpp",
           "app/painter/tests/test-filter-context.cpp")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "migration/tests/filter-context-native.json")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit("Report already sealed; choose another --output")
    archive = ROOT / "migration/fixtures/legacy-filter-context.tar.gz"
    manifest = archive.with_suffix(".manifest.json")
    temporary = tempfile.TemporaryDirectory(prefix="gimp-context-check-", dir="/workspace/shared")
    materialize(archive, manifest, Path(temporary.name))
    directory = Path(temporary.name) / "legacy-filter-context"
    fixture_report = directory / "capture-report.json"
    capture = json.loads(fixture_report.read_text())
    if capture["status"] != "passed" or capture["native_merge_count"] != 280 or capture["live_merge_count"] != 24:
        raise SystemExit("Incomplete old native fixture capture")
    for name, digest in capture["files_sha256"].items():
        if sha(directory / name) != digest:
            raise SystemExit("Old native fixture changed: " + name)
    before = {name: sha(ROOT / name) for name in SOURCES}
    executable = Path("/workspace/shared/test-filter-context" + ("-sanitized" if args.sanitize else "-normal"))
    command = ["g++", "-std=c++14", "-g", "-O2", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "app/painter"),
               str(ROOT / SOURCES[0]), str(ROOT / SOURCES[2]), "-o", str(executable)]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=all"]
    log = args.output.with_suffix(".log")
    env = dict(os.environ)
    if args.sanitize:
        # LeakSanitizer's thread scan is unsupported under this executor.
        env.update(ASAN_OPTIONS="detect_leaks=0:abort_on_error=1", UBSAN_OPTIONS="halt_on_error=1")
    with Path("/workspace/shared/gimp-painter-build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        subprocess.run(command, check=True)
        with log.open("w") as stream:
            result = subprocess.run([str(executable), str(directory)], env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=90)
    after = {name: sha(ROOT / name) for name in SOURCES}
    report = dict(status="passed" if result.returncode == 0 and before == after else "failed",
                  exit_code=result.returncode, source_sha256=before, sources_unchanged=before == after,
                  completed_utc=datetime.now(timezone.utc).isoformat(), sanitizer=args.sanitize,
                  leak_sanitizer=False,
                  sanitizer_options={key: env.get(key) for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")},
                  coverage="Independent scalar adapter and fixture harness only; no full application sanitizer claim",
                  fixture_report_sha256=sha(fixture_report), command=command,
                  fixture_archive_sha256=sha(archive), fixture_manifest_sha256=sha(manifest),
                  executable_sha256=sha(executable), output=log.read_text())
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    temporary.cleanup()
    print(report["status"], report["output"].strip())
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run a native test binary with verified old context, SmallTiles and Retinex corpora.

Usage: python3 run_filter_owner_context_fixture_test.py -- BINARY [ARG ...]
Build orchestration, sanitizer settings, test selection, and timeouts belong to
the caller. This wrapper only materializes immutable evidence and runs BINARY.
"""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from filter_context_fixture_bundle import materialize

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_small_tiles_evidence import verify as verify_small_tiles
from check_retinex_evidence import verify as verify_retinex
COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"
BUNDLES = (
    ("legacy-filter-context", "GIMP_PAINTER_CONTEXT_FIXTURES", 280, 24,
     "1f3b0bf7ddd08ac00d2e1582016599100d249e0a3657b7e96f94b12ba52c3019"),
    ("legacy-filter-context-expansion", "GIMP_PAINTER_CONTEXT_EXPANSION_FIXTURES", 0, 6,
     "364ce86d1517e99d3e399126b38dedac03cf1430a3150add11b4f2d6b1b48841"),
)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def materialize_owner_context_fixtures(destination):
    """Return the exact environment additions after verifying both corpora."""
    destination = Path(destination)
    additions, captures = {}, []
    for name, variable, native_count, live_count, pinned_archive in BUNDLES:
        archive = ROOT / "migration/fixtures" / (name + ".tar.gz")
        if sha(archive) != pinned_archive:
            raise ValueError("Unrecognized old context capture: " + name)
        isolated = destination / name
        materialize(archive, archive.with_suffix(".manifest.json"), isolated)
        directory = isolated / name
        capture = json.loads((directory / "capture-report.json").read_text())
        if (capture.get("status") != "passed" or capture.get("errors") or
                capture.get("source_commit") != COMMIT or capture.get("exit_code") != 0 or
                capture.get("native_merge_count") != native_count or
                capture.get("live_merge_count") != live_count):
            raise ValueError("Incomplete or invalid old context capture: " + name)
        for filename, digest in capture["files_sha256"].items():
            if sha(directory / filename) != digest:
                raise ValueError("Changed old context evidence: " + filename)
        for filename, key in (("capture-harness.c", "harness_sha256"),
                              ("capture-script.py", "capture_script_sha256"),
                              ("observation-hooks.patch", "instrumentation_sha256")):
            if sha(directory / filename) != capture[key]:
                raise ValueError("Changed old context provenance: " + filename)
        rows = (directory / "merges.tsv").read_text().splitlines()
        if (sum(row.startswith("native-") for row in rows) != native_count or
                sum(not row.startswith("native-") for row in rows) != live_count):
            raise ValueError("Incomplete old context merge table: " + name)
        additions[variable] = str(directory)
        captures.append(capture)
    for source, digest in captures[0]["source_sha256"].items():
        if captures[1]["source_sha256"].get(source) != digest:
            raise ValueError("Context corpora disagree about the old source: " + source)
    return additions


def main(argv=None):
    command = list(sys.argv[1:] if argv is None else argv)
    if command and command[0] == "--":
        command.pop(0)
    if not command:
        raise SystemExit("Usage: run_filter_owner_context_fixture_test.py -- BINARY [ARG ...]")
    with tempfile.TemporaryDirectory(prefix="gimp-owner-context-") as temporary:
        environment = dict(os.environ)
        environment.update(materialize_owner_context_fixtures(temporary))
        small_tiles = Path(temporary) / "small-tiles"
        verify_small_tiles(extract=small_tiles)
        environment["GIMP_PAINTER_SMALL_TILES_FIXTURES"] = str(small_tiles / "small-tiles-evidence")
        retinex = Path(temporary) / "retinex"
        verify_retinex(extract=retinex)
        environment["GIMP_PAINTER_RETINEX_FIXTURES"] = str(retinex / "retinex-evidence")
        return subprocess.run(command, env=environment).returncode


if __name__ == "__main__":
    raise SystemExit(main())

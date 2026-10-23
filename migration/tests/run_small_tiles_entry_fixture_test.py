#!/usr/bin/env python3
"""Materialize verified old Small Tiles and Retinex bytes for the registered native C-entry test."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from check_small_tiles_evidence import verify
from check_retinex_evidence import verify as verify_retinex


def main():
    if len(sys.argv) < 3 or sys.argv[1] != "--":
        raise SystemExit("Usage: run_small_tiles_entry_fixture_test.py -- BINARY [ARG ...]")
    with tempfile.TemporaryDirectory(prefix="gimp-small-tiles-entry-") as temporary:
        verify(extract=Path(temporary))
        retinex = Path(temporary) / "retinex-isolated"
        retinex.mkdir()
        verify_retinex(extract=retinex)
        environment = dict(os.environ)
        environment["GIMP_PAINTER_SMALL_TILES_FIXTURES"] = str(Path(temporary) / "small-tiles-evidence")
        environment["GIMP_PAINTER_RETINEX_FIXTURES"] = str(retinex / "retinex-evidence")
        return subprocess.run(sys.argv[2:], env=environment).returncode


if __name__ == "__main__":
    raise SystemExit(main())

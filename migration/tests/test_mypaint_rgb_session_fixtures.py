#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate complete independent RGB session records, without running old code."""
import gzip
import hashlib
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
fixture = root / "migration/fixtures/legacy-mypaint-rgb-session"
manifest = json.loads((fixture / "manifest.json").read_text())
for name, record in manifest["files"].items():
    raw = (fixture / name).read_bytes()
    assert len(raw) == record["bytes"]
    assert hashlib.sha256(raw).hexdigest() == record["sha256"], name
data = gzip.decompress((fixture / "session-values.tsv.gz").read_bytes())
assert hashlib.sha256(data).hexdigest() == "64cdb3add5ad92af01fe93f079a0378b4dc7a7caefe56406fd29c67747ee00d7"
rows = data.splitlines()
assert len(rows) == 385
assert rows[-1] == b"RGB_SESSION_CAPTURE_COMPLETE"
initial = bytes(c for y in range(96) for x in range(130)
                for c in ((x*19+y*7) % 256, (x*3+y*29) % 256, (x*13+y*11) % 256))
images = {}
undos = set()
for row in rows[:-1]:
    fields = row.split()
    scenario = int(fields[1])
    assert 0 <= scenario < 96
    if fields[0] == b"RGB_SESSION_PIX":
        assert len(fields) == 4
        phase = fields[2]
        assert phase in (b"finish", b"undo", b"redo")
        key = scenario, phase
        assert key not in images
        pixels = bytes.fromhex(fields[3].decode())
        assert len(pixels) == len(initial)
        images[key] = pixels
    else:
        assert fields[0] == b"RGB_SESSION_UNDO"
        assert scenario not in undos
        assert fields[2:] == [b"1"] * 5
        undos.add(scenario)
assert len(images) == 288 and len(undos) == 96
for scenario in range(96):
    assert images[scenario, b"finish"] == images[scenario, b"redo"]
    assert images[scenario, b"undo"] == initial
    # Full alpha lock over an initially transparent floating stroke has no
    # visible effect in the pinned implementation, while still producing Undo.
    assert (images[scenario, b"finish"] == initial) == (scenario >= 64 and scenario % 2 == 1)
print("96 independent warmed RGB scenes: all 288 complete images, Undo/Redo and record hash verified")

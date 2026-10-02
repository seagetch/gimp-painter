#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the sealed old Gray reference without executing old code."""
import gzip
import hashlib
import json
from pathlib import Path
fixture = Path(__file__).resolve().parents[2] / 'migration/fixtures/legacy-mypaint-gray-session'
manifest = json.loads((fixture/'manifest.json').read_text())
for name, entry in manifest['files'].items():
    data = (fixture/name).read_bytes()
    assert len(data) == entry['bytes']
    assert hashlib.sha256(data).hexdigest() == entry['sha256'], name
values = gzip.decompress((fixture/'session-values.tsv.gz').read_bytes())
assert hashlib.sha256(values).hexdigest() == '28cc932d137e760f4b6d5200994cbe6d22779443b23875e585c24ea58de6e7dc'
rows = values.splitlines()
assert len(rows) == 193 and rows[-1] == b'GRAY_SESSION_CAPTURE_COMPLETE'
initial = bytes((x*19+y*7)%256 for y in range(96) for x in range(130))
images = {}; undos = set()
for row in rows[:-1]:
    fields = row.split(); scenario = int(fields[1]); assert 0 <= scenario < 48
    if fields[0] == b'GRAY_SESSION_PIX':
        assert len(fields) == 4 and fields[2] in (b'finish', b'undo', b'redo')
        key = scenario, fields[2]; assert key not in images
        images[key] = bytes.fromhex(fields[3].decode()); assert len(images[key]) == len(initial)
    else:
        assert fields[0] == b'GRAY_SESSION_UNDO' and fields[2:] == [b'1']*5
        assert scenario not in undos; undos.add(scenario)
assert len(images) == 144 and len(undos) == 48
for scenario in range(48):
    assert images[scenario, b'finish'] == images[scenario, b'redo']
    assert images[scenario, b'undo'] == initial
    assert images[scenario, b'finish'] != initial
print('48 independent warmed Gray scenes: all 144 native one-byte images, Undo/Redo and record hash verified')

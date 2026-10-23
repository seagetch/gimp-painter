#!/usr/bin/env python3
"""Verify exact bounded legacy-site coverage without claiming runtime parity."""
import csv
from pathlib import Path
root = Path(__file__).resolve().parents[3]
with (root / 'migration/inventory/signal-source-inventory.tsv').open() as source:
    expected = {row['legacy_site'] for row in csv.DictReader(source, delimiter='\t')
                if row['followup'] == '07.008/untracked-signals'}
with Path(__file__).with_name('source-map.tsv').open() as source:
    rows = list(csv.DictReader(source, delimiter='\t'))
actual = [row['legacy_site'] for row in rows]
assert len(actual) == len(set(actual)) == len(expected) == 60
assert set(actual) == expected
assert all(all(row.values()) for row in rows)
print('PASS: exact 60-row source mapping; runtime acceptance is separate')

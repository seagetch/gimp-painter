#!/usr/bin/env python3
"""Verify the immutable old-helper census and committed provider checkpoint."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
record = json.loads((root / 'migration/contracts/filter-temporary-gvalue-sites.json').read_text())
def blob(revision, path):
    return subprocess.check_output(['git', 'show', revision + ':' + path], cwd=root)
old = blob(record['old_commit'], record['old_path'])
assert hashlib.sha256(old).hexdigest() == record['old_sha256']
assert [s.decode() for s in re.findall(rb'g_value_init\s*\(\s*&([A-Za-z_]\w*)', old)] == record['initialized_conversion_temporaries']
for path, expected in record['providers'].items():
    assert hashlib.sha256(blob(record['provider_checkpoint'], path)).hexdigest() == expected, path
print('PASS: two old initialized diagnostic temporaries; four pinned provider hashes; source audit only')

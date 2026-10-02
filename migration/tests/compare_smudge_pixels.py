#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip
from pathlib import Path
import subprocess
import sys
expected = gzip.decompress(Path(sys.argv[1]).read_bytes())
actual = subprocess.check_output(sys.argv[2:])
assert actual == expected, 'Legacy Smudge accumulation/shading mismatch'
print(f'Exact legacy Smudge arithmetic: {len(expected)} bytes, 1024 cases')

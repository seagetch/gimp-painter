#!/usr/bin/env python3
"""Report original C-entry source tokens and optional native linked symbols.

This is an observation tool, not a semantic acceptance gate. A token is not a
provider and absence from one link is not absence from all enabled libraries.
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MAP = 'migration/contracts/cpp-c-entry-map.tsv'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(root, binaries):
    with (root / MAP).open() as source:
        rows = list(csv.DictReader(source, delimiter='\t'))
    names = {row['c_entry'] for row in rows}
    if len(names) != len(rows):
        raise ValueError('Duplicate canonical entry name')
    occurrences = {name: [] for name in names}
    # These are expressly lexical source observations, including macros and
    # inactive text; do not mistake headers/callers/comments for definitions.
    pattern = re.compile(r'\b(?:' + '|'.join(map(re.escape, sorted(names))) + r')\b')
    for path in sorted((root / 'app').rglob('*')):
        if path.suffix not in {'.c', '.cpp', '.h', '.hpp', '.inc'} or not path.is_file():
            continue
        for number, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
            for name in sorted(set(pattern.findall(line))):
                occurrences[name].append({'path': str(path.relative_to(root)), 'line': number})
    links = []
    for binary in binaries:
        path = Path(binary)
        result = subprocess.run(['nm', '-g', '--defined-only', str(path)],
                                check=True, text=True, capture_output=True)
        symbols = {line.split()[-1] for line in result.stdout.splitlines() if line.split()}
        links.append({'file': path.name, 'sha256': digest(path),
                      'present': sorted(names & symbols),
                      'absent': sorted(names - symbols)})
    return {'schema': 1, 'scope': '08.018 source/link observations; not acceptance',
            'map_sha256': digest(root / MAP), 'entry_count': len(rows),
            'entries': [{**row, 'source_tokens': occurrences[row['c_entry']]} for row in rows],
            'links': links,
            'limitations': ['Token occurrences include declarations, calls and inactive text.',
                            'G_DEFINE_TYPE registration expands its get_type provider.',
                            'Link garbage collection can omit unused providers.',
                            'Renamed routes require ownership and semantic review.',
                            'No source/ABI compatibility or runtime result follows from this census.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--binary', action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.write_text(json.dumps(audit(args.root, args.binary), indent=2) + '\n')

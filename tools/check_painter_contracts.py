#!/usr/bin/env python3
"""Check exhaustive common C++ type/entry contracts without claiming a port."""
import csv
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    with (ROOT / path).open() as handle:
        return list(csv.DictReader(handle, delimiter='\t'))


def entry_sources():
    result = {}
    for row in read('migration/inventory/cpp-definition-review.tsv'):
        if row['classification'] in ('C_HEADER_ENTRY', 'LOCAL_C_DECLARED_GET_TYPE'):
            result[row['symbol']] = row['cpp_site']
    for row in read('migration/inventory/generated-gtype-review.tsv'):
        result[row['symbol']] = row['macro_site']
    for row in read('migration/inventory/non-gimp-c-entry-review.tsv'):
        if row['classification'] in ('C_DIRECT', 'C_CALLBACK'):
            result[row['symbol']] = row['cpp_definition']
    return result


def verify():
    errors = []
    handles = read('migration/contracts/cpp-handle-registry.tsv')
    types = {row['legacy_type'] for row in handles}
    source = json.loads((ROOT / 'migration/baseline/baseline.json').read_text())['source']['commit']
    declaration_lines = subprocess.check_output(
        ['git', 'grep', '-n', '-E',
         '^[[:space:]]*__(DECLARE_GTK_(CLASS|CAST|IFACE)|DECLARE_GIMP_INTERFACE)__',
         source, '--', 'app'], cwd=ROOT, text=True)
    declarations = {match.group(1).lstrip('_') for match in
                    re.finditer(r'__(?:DECLARE_GTK_(?:CLASS|CAST|IFACE)|DECLARE_GIMP_INTERFACE)__\s*\(\s*(\w+)', declaration_lines)}
    if not declarations <= types:
        errors.append('missing declared standard/custom types: ' + str(declarations - types))
    for path, field in [('cpp-types.tsv', 'runtime_gtype'),
                        ('generated-gtype-review.tsv', 'instance_type')]:
        expected = {row[field] for row in read('migration/inventory/' + path)}
        if not expected <= types:
            errors.append('missing type contracts: ' + str(expected - types))
    if len(types) != len(handles) or len({r['handle'] for r in handles}) != len(handles):
        errors.append('duplicate type or handle contract')
    if any(not r['declared_at'] or not r['contract'] or r['implementation'] != 'TODO' for r in handles):
        errors.append('incomplete contract or overclaimed feature implementation')
    entries = read('migration/contracts/cpp-c-entry-map.tsv')
    expected_entries = entry_sources()
    if len(entries) != len(expected_entries) or {r['c_entry'] for r in entries} != expected_entries.keys():
        errors.append('C-entry coverage differs from audited source')
    if len({r['cpp_operation'] for r in entries}) != len(entries):
        errors.append('more than one C entry assigned to one C++ operation')
    handle_names = {r['handle'] for r in handles}
    for row in entries:
        operation = row['cpp_operation']
        if operation.startswith('TypeTraits<'):
            if operation.split('<', 1)[1].split('>', 1)[0] not in types:
                errors.append('unknown C type in TypeTraits route: ' + operation)
        elif operation.split('::', 1)[0] not in handle_names:
            errors.append('unknown named handle: ' + operation)
        if row['legacy_site'] != expected_entries.get(row['c_entry']) or not row['cpp_operation'] or not row['rule']:
            errors.append('invalid C-entry source or route: ' + row['c_entry'])
    return errors, len(handles), len(entries)


def main():
    errors, handles, entries = verify()
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'{handles} unique typed-handle contracts; {entries} C entries map to a single C++ route; feature implementation not claimed')


if __name__ == '__main__':
    main()

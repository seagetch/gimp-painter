#!/usr/bin/env python3
"""Check the original 05.011 type/slot contract against current source."""
from collections import Counter
import csv
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = ROOT / 'migration/contracts/cpp-vfunc-order.json'
LITERALS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
TYPE = re.compile(r'G_DEFINE_TYPE(?:_WITH_CODE|_WITH_PRIVATE|_EXTENDED)?\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)')
COMPAT = re.compile(r'Painter|PerspectiveGuide|CloneLayer|FilterLayer|LayerPreset|FillBrush')
METADATA = {'default_icon_name', 'default_name', 'handles_changing_brush',
            'handles_transforming_brush', 'handles_dynamic_transforming_brush'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def clean(text):
    return LITERALS.sub(lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text)


def balanced(text, start, opening, closing):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == opening:
            depth += 1
        elif text[i] == closing:
            depth -= 1
            if depth == 0:
                return i + 1
    raise ValueError('unterminated source construct')


def function_body(text, symbol):
    stripped = clean(text)
    matches = list(re.finditer(r'\b' + re.escape(symbol) + r'\s*\([^;{}]*\)\s*\{', stripped))
    if len(matches) != 1:
        raise ValueError('initializer definition missing or ambiguous: ' + symbol)
    start = stripped.index('{', matches[0].start())
    return stripped[start:balanced(stripped, start, '{', '}')]


def discover():
    types = {}
    for path in sorted((ROOT / 'app').rglob('*')):
        if not path.is_file() or path.suffix not in ('.cpp', '.cc', '.c') or 'tests' in path.parts:
            continue
        text = path.read_text(); stripped = clean(text)
        for match in TYPE.finditer(stripped):
            name, prefix, parent = match.groups()
            if path.suffix == '.c' and not COMPAT.search(name):
                continue
            assert name not in types, name
            start = stripped.index('(', match.start())
            definition = stripped[start:balanced(stripped, start, '(', ')')]
            interfaces = re.findall(r'G_IMPLEMENT_INTERFACE\s*\(\s*\w+\s*,\s*(\w+)\s*\)', definition)
            initializers = []
            for symbol in [prefix + '_class_init', *interfaces]:
                body = function_body(text, symbol)
                assignments = re.findall(r'->\s*(\w+)\s*=(?!=)', body)
                callbacks = [{'slot': field, 'callback': '<lambda>' if lam else value}
                             for field, lam, value in re.findall(
                                 r'->\s*(\w+)\s*=(?!=)\s*(?:(\+?\s*\[)|([A-Za-z_]\w*))', body)
                             if field not in METADATA]
                initializers.append({'symbol': symbol, 'assigned_fields': assignments,
                                     'assigned_callbacks': callbacks, 'body_sha256': sha(body.encode())})
            types[name] = {'source': str(path.relative_to(ROOT)), 'parent_macro': parent,
                           'initializers': initializers}
    return types


def main():
    contract = json.loads(CONTRACT.read_text())
    assert contract['task'] == '05.011' and contract['status'] == 'DEFINED'
    actual = discover()
    expected = {t['type']: t for t in contract['types']}
    assert len(expected) == len(contract['types']) and set(actual) == set(expected), (set(actual)-set(expected), set(expected)-set(actual))
    count = 0
    for name, current in actual.items():
        row = expected[name]
        assert row['source'] == current['source'] and row['parent_macro'] == current['parent_macro'], name
        assert row['initializers'] == current['initializers'], name
        assigned = Counter(f for i in current['initializers'] for f in i['assigned_fields'])
        described = Counter(s['slot'].split('.')[-1] for s in row['slots'])
        assert set(row['class_metadata_fields']) <= METADATA, name
        described.update(row['class_metadata_fields'])
        assert assigned == described, (name, assigned, described)
        assigned_callbacks = Counter((s['slot'], s['callback']) for i in current['initializers'] for s in i['assigned_callbacks'])
        described_callbacks = Counter((s['slot'].split('.')[-1], '<lambda>' if s['callback'].endswith(' lambda') else s['callback']) for s in row['slots'])
        assert assigned_callbacks == described_callbacks, (name, assigned_callbacks, described_callbacks)
        assert row['activation'] and row['cleanup'] and row['reentry'], name
        for slot in row['slots']:
            assert slot['parent_order'] and slot['callback'], (name, slot)
        count += len(row['slots'])
    for path, digest in contract['source_sha256'].items():
        assert sha((ROOT / path).read_bytes()) == digest, path
    registry = list(csv.DictReader((ROOT / 'migration/contracts/cpp-handle-registry.tsv').open(), delimiter='\t'))
    custom = {r['legacy_type'] for r in registry if 'custom-runtime-type' in r['kind']}
    mapped = contract['legacy_custom_types']
    inherited = contract['legacy_reference_only_types']
    assert len(mapped) == len(custom) == 23 and {r['legacy_type'] for r in mapped} == custom
    assert len(inherited) == len(set(inherited)) == 67
    assert set(inherited) | custom == {r['legacy_type'] for r in registry}
    for row in mapped:
        assert row['disposition'] and row['contract'] and row['feature_gate']
        assert all(t in expected for t in row['current_types']), row
    evidence = json.loads((ROOT / 'migration/tests/vfunc-order/results.json').read_text())
    assert [(r['name'], r['exit_code'], r['passed']) for r in evidence['results']] == [('hierarchy', 0, 3), ('lifecycle', 0, 1)]
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        assert not any(r['wbs_task'] == '05.011' for r in csv.DictReader(stream, delimiter='\t'))
    print('PASS: original05.011 %d types/%d slots, all90 legacy dispositions, 4 native contract cases' % (len(actual), count))


if __name__ == '__main__':
    main()

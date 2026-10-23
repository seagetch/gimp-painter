#!/usr/bin/env python3
"""Check bounded point dispositions against the generator and accepted children."""
import ast
import csv
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[3]
source = ast.parse((ROOT/'tools/inventory_filter_routes.py').read_text())
constants = {}
for statement in source.body:
    if isinstance(statement, ast.Assign) and len(statement.targets) == 1:
        target = statement.targets[0]
        if isinstance(target, ast.Name) and target.id == 'BOUNDED_CONTEXT_ROUTES':
            constants[target.id] = ast.literal_eval(statement.value)
rows = list(csv.DictReader((ROOT/'migration/inventory/filter-procedure-routes.tsv').open(), delimiter='\t'))
assert len(rows) == len({row['procedure'] for row in rows}) == 108
slugs = {'plug-in-max-rgb':'native-max-rgb-context', 'plug-in-vinvert':'native-value-invert-context',
         'plug-in-sobel':'native-sobel-context', 'plug-in-sharpen':'native-sharpen-context',
         'plug-in-threshold-alpha':'native-threshold-alpha-context'}
assert set(constants['BOUNDED_CONTEXT_ROUTES']) == set(slugs)
for name, slug in slugs.items():
    row = next(row for row in rows if row['procedure'] == name)
    for key, value in constants['BOUNDED_CONTEXT_ROUTES'][name].items():
        assert row[key] == value
    receipt = ROOT/'migration/tests'/f'{slug}.json'
    accepted = json.loads(receipt.read_text())
    assert accepted['status'].startswith('passed')
    assert str(receipt.relative_to(ROOT)) in row['dependency_status']
    assert row['missing_port_work'] and row['dependency_status']
# Exercise the actual generator row body; discovery inputs are deliberately empty.
row_function = next(n for n in source.body if isinstance(n, ast.FunctionDef) and n.name == 'row')
namespace = dict(constants, rows=[], new_files={}, counterparts={}, operations=set(),
                 exact=set(), new_exact={'plug-in-max-rgb','plug-in-vinvert','plug-in-threshold-alpha'}, nonpixel=set(), context=set())
exec(compile(ast.Module(body=[row_function], type_ignores=[]), '<inventory-row>', 'exec'), namespace)
for name in constants['BOUNDED_CONTEXT_ROUTES']:
    namespace['row'](name, [], [], 'test', ['public-source.c'])
    actual = namespace['rows'][-1]
    expected = next(row for row in rows if row['procedure'] == name)
    for key in ('executor_route', 'missing_port_work', 'dependency_status'):
        assert actual[key] == expected[key], (name, key)
missing = sum('required' in row['executor_route'] for row in rows)
assert missing == 93, missing
print('PASS:108 unique procedures;5 source-backed bounded dispositions;93 required-labelled rows; no generic completeness or parent claim')

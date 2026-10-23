#!/usr/bin/env python3
"""Check the defined noncolliding identifier/version registry against source."""
import ast
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
paths = ['app/xcf/xcf-private.h', 'app/xcf/painter-xcf-preserve.cpp',
         'app/xcf/painter-xcf-multipart.cpp', 'app/xcf/painter-xcf-transport.cpp',
         'migration/contracts/xcf-extension-identifiers.md']
text = {p: (ROOT / p).read_text() for p in paths}
private, preserve, multipart, transport, contract = (text[p] for p in paths)
assert re.search(r'PROP_PARASITES\s*=\s*21\b', private)
for namespace in ['image', 'item', 'origin']:
    assert f'const char {namespace}_name[] = "gimp-painter-{namespace}";' in preserve
    assert f'`gimp-painter-{namespace}`' in contract

def magic(source, name):
    match = re.search(r'const guint8 ' + name + r'\[12\]\s*=\s*\{([^}]+)\}', source)
    assert match, name
    values = [ast.literal_eval(v.strip()) for v in match[1].split(',')]
    return bytes(ord(v) if isinstance(v, str) else v for v in values)

expected = {'magic': '475058434600000001000000', 'manifest_magic': '475058434600000002000000',
            'chunk_magic': '475058434300000001000000', 'inert_magic': '475058434900000001000000'}
for name, value in expected.items():
    assert magic(preserve if name == 'magic' else multipart, name).hex() == value
    assert value in contract
assert magic(multipart, 'logical_magic') == magic(preserve, 'magic')
assert magic(transport, 'v2_magic') == magic(multipart, 'manifest_magic')
for kind in ['chunk', 'inert']:
    assert f'"gimp-painter-{kind}-v1/"' in multipart
assert '"version", "u", &version) || version != 1' in preserve
assert '"gimp-painter-standard-v1"' in preserve
assert 'GUINT32_TO_BE (PROP_PARASITES)' in preserve
assert 'GIMP_PARASITE_PERSISTENT' in preserve
print(json.dumps({'task': '10.011', 'result': 'PASS source-consistency only',
                  'files_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths},
                  'magic_hex': expected}, indent=2))

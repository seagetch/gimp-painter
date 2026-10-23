#!/usr/bin/env python3
"""Source-level record definition consistency; not a serialization runtime test."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
paths = ['app/xcf/painter-xcf-preserve.cpp', 'app/xcf/painter-xcf-arguments.cpp',
         'migration/contracts/xcf-record-definition.md']
preserve, arguments, contract = [(ROOT / p).read_text() for p in paths]
fields = {'version': 'uint32', 'id': 'uint32', 'source-id': 'uint32',
          'source-state': 'uint32', 'source-expired': 'boolean', 'allow-name-lookup': 'boolean',
          'has-definition': 'boolean', 'has-arguments': 'boolean', 'generation': 'uint64',
          'cache-generation': 'uint64', 'cache-complete': 'boolean', 'saved-state': 'uint32'}
for name, constructor in fields.items():
    assert f'dict.put ("{name}", g_variant_new_{constructor} (' in preserve, name
    assert f'`{name}`' in contract, name
for name in ['pending-name', 'source-name', 'original-source-name', 'procedure']:
    assert f'nullable_string (dict, "{name}"' in preserve
    assert f'`{name}`' in contract
assert 'std::strlen (value) + 1' in preserve
assert 'else g_variant_dict_remove (&dict.dict, key)' in preserve
assert 'Builder b ("a(sbv)")' in arguments
assert 'Builder refs ("a(uuxsbb)")' in arguments
assert '"(uuxsbb)", kind, tattoo, ref.id, g_type_name (ref.object_type), ref.was_set, ref.expired' in arguments
for key in ['original-owner','original-header','original-properties','original-extension',
            'opaque-record-sequences','original-argument-model','original-argument-procedure','external-reference-origins']:
    assert '"'+key+'"' in preserve
    assert '`'+key+'`' in contract
print(json.dumps({'task': '10.012', 'result': 'PASS source definition consistency only',
                  'files_sha256': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in paths}}, indent=2))

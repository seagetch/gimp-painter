#!/usr/bin/env python3
"""Validate this bounded presence successor's saved evidence, not all migration work."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--check-source', action='store_true')
parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parent)
args = parser.parse_args()
root = args.root
repo = Path(__file__).resolve().parents[3]
read = lambda name: json.loads((root / name).read_text())
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
manifest = read('manifest.json')
for name, digest in manifest['artifacts'].items():
    assert sha(root / name) == digest, name
summary = read('build-summary.json')
assert summary['full_build_returncode'] == 0
assert summary['after_test_fixture_correction_rebuild_returncode'] == 0
assert summary['full_build_steps'] == 3715
assert not summary['regular_ci_changed']
assert '[3715/3715]' in gzip.decompress((root / 'compile.log.gz').read_bytes()).decode()
assert 'ninja: build stopped:' not in gzip.decompress((root / 'compile.log.gz').read_bytes()).decode()
expected = {'gimp-layer-presets': 12, 'layer-presets-ui': 18,
            'preset-gui-entries': 6, 'preset-catalog': 31, 'preset-catalog-ui': 5}
normal = read('normal-results.json')
assert len(normal) == 7
assert all(item['result'] == 'OK' and item['returncode'] == 0 for item in normal)
seen = set()
for item in normal:
    name = item['name'].split('/')[-1].strip()
    seen.add(name)
    assert '# SKIP' not in item['stdout']
    if name in expected:
        assert sum(line.startswith('ok ') for line in item['stdout'].splitlines()) == expected[name], name
assert seen == set(expected) | {'painter-public-header-compilation', 'painter-additional-header-routes'}
scoped = read('sanitizer-scope.json')
assert len(scoped['instrumented_sources']) == 11 and not scoped['changed_during_build']
assert not any(scoped[key] for key in ('vptr_sanitizer', 'leak_detection',
                                     'dependencies_instrumented', 'whole_application_instrumented'))
san = read('sanitizer-results.json')
assert len(san) == 3 and {item['name'] for item in san} == {'preset-catalog', 'preset-catalog-ui', 'preset-gui-entries'}
for item in san:
    assert item['returncode'] == 0 and item['actual'] == item['expected'] == expected[item['name']]
    assert sum(line.startswith('ok ') for line in item['stdout'].splitlines()) == item['expected']
    assert '# SKIP' not in item['stdout']
causal = read('causal-results.json')
assert {item['name'] for item in causal} == {'original-provider', 'missing-stale-lease-presence'}
for item in causal:
    assert item['returncode'] != 0 and item['expected_assertion'] in item['stdout'] + item['stderr']
assert 'the_unit_gimp == NULL' in (root / 'initial-unit-guard-failure.log').read_text()
assert '1080x1920x24' in (root / 'normal.log').read_text()
png = (root / 'selected-catalog-portrait.png').read_bytes()
assert png[:8] == b'\x89PNG\r\n\x1a\n' and struct.unpack('>II', png[16:24]) == (1080, 1920)
if args.check_source:
    for name, digest in summary['source_sha256'].items():
        assert sha(repo / name) == digest, name
        assert scoped['sources_sha256'][name] == digest, name
print('PASS: full build; native72 plus two header groups; selected eleven-unit sanitizer42; two causal rejections; portrait/oncanvas evidence')

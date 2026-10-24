#!/usr/bin/env python3
"""Portable, read-only validation of the bounded original15.003 receipt."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import tarfile

CASE = '/gimp-filter-layer/filter_gaussian_property_contract'
FIXTURE = 'app/tests/test-filter-property-contract.inc'

def require(condition, message):
    if not condition:
        raise ValueError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def check_result(result, label):
    require(result['exit_code'] == 0, label + ': failed result')
    require(re.findall(r'^ok \d+ (\S+)', result['stdout'], re.M) == [CASE],
            label + ': wrong test count/path')
    require('6 publications x4 coherent property notifications' in result['stdout'],
            label + ': missing six-by-four assertion receipt')

def check_fixture(source):
    text = source.decode()
    require('for(guint i=0;i<4;i++)g_assert_cmpuint(state->counts[i],==,expected)' in text,
            'missing four-property count assertion')
    require(re.findall(r'filter_property_counts\(&state,(\d)\)', text) == list('123456'),
            'missing six publication assertions')
    require('g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),==,opaque_runs)' in text,
            'opaque non-execution claim lacks assertion')
    require('g_assert_cmpuint(gimp_filter_layer_get_generation(state->filter),==,generation)' in text,
            'getter scheduling invariant absent')
    require('filter_new(' not in text and 'plug-in-edge' not in text,
            'fixture escaped Gaussian-only scope')

def check_source_hashes(hashes, archive):
    for path, digest in hashes.items():
        require(path in archive and sha(archive[path]) == digest, 'wrong source: ' + path)

def validate(package, readiness_path):
    manifest = json.loads((package / 'artifact-sha256.json').read_text())
    actual = {str(p.relative_to(package)) for p in package.rglob('*')
              if p.is_file() and p.name != 'artifact-sha256.json' and '__pycache__' not in p.parts}
    require(set(manifest) == actual, 'artifact list differs')
    for path, digest in manifest.items():
        require(sha((package / path).read_bytes()) == digest, 'artifact changed: ' + path)
    before = json.loads((package / 'source-before.json').read_text())
    after = json.loads((package / 'source-after.json').read_text())
    require(before['sha256'] == after['sha256'] and after['changed_after_run'] == [],
            'source drift in native run')
    native = json.loads((package / 'native.json').read_text())
    san = json.loads((package / 'sanitizer.json').read_text())
    check_result(native, 'native'); check_result(san, 'sanitizer')
    require(san['selected_test_paths'] == [CASE] and san['changed_after_compile'] == [],
            'sanitizer case/source drift')
    require(san['sanitizers'] == ['address', 'undefined'] and san['leak_detection'] is False,
            'wrong sanitizer policy')
    require(len(san['instrumented_sources']) == 20 and
            len(san['rtti_compatibility_only_sources']) == 53, 'wrong instrumentation scope')
    archive = {}
    with tarfile.open(package / 'sources.tar.gz', 'r:gz') as tar:
        for entry in tar:
            require(entry.isfile() and not Path(entry.name).is_absolute() and
                    '..' not in Path(entry.name).parts and entry.name not in archive,
                    'unsafe or duplicate archive member')
            archive[entry.name] = tar.extractfile(entry).read()
    owned = json.loads((package / 'owned-source-sha256.json').read_text())
    all_hashes = dict(san['source_sha256']); all_hashes.update(before['sha256'])
    require(len(archive) == 133 and set(archive) == set(all_hashes), 'archive input count/list')
    check_source_hashes(all_hashes, archive); check_source_hashes(owned, archive)
    check_fixture(archive[FIXTURE])
    excerpt = json.loads((package / 'readiness-overlay-excerpt.json').read_text())
    full_bytes = readiness_path.read_bytes(); full = json.loads(full_bytes)
    require(sha(full_bytes) == excerpt['full_manifest_sha256'], 'readiness full manifest anchor')
    require(full['base'] == before['committed_base'], 'readiness base')
    for key in ('owned_sha256', 'input_sha256', 'held_exact_base'):
        require(full[key] == excerpt[key], 'readiness excerpt mismatch: ' + key)
    check_source_hashes(full['input_sha256'], archive)
    for path, digest in before['sha256'].items():
        if path not in owned:
            require(full['complete_source_sha256'][path] == digest, 'baseline drift: ' + path)
    return native, san, archive, owned

def controls(native, san, archive, owned):
    checks = []
    failed = copy.deepcopy(native); failed['exit_code'] = 1
    checks.append(('failed-result', lambda: check_result(failed, 'mutant')))
    wrong_case = copy.deepcopy(san); wrong_case['stdout'] = wrong_case['stdout'].replace(CASE, CASE+'-other')
    checks.append(('wrong-case', lambda: check_result(wrong_case, 'mutant')))
    wrong_source = dict(archive); wrong_source[FIXTURE] += b'\n/* changed */\n'
    checks.append(('wrong-source', lambda: check_source_hashes(owned, wrong_source)))
    opaque = archive[FIXTURE].replace(b'==,opaque_runs', b'>,opaque_runs')
    checks.append(('opaque-execution-claim', lambda: check_fixture(opaque)))
    counts = archive[FIXTURE].replace(b'filter_property_counts(&state,6)', b'filter_property_counts(&state,5)')
    checks.append(('missing-sixth-publication', lambda: check_fixture(counts)))
    for name, test in checks:
        try:
            test()
        except ValueError:
            continue
        raise ValueError('negative control accepted: ' + name)
    return len(checks)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--package', type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument('--readiness-manifest', type=Path)
    args = parser.parse_args()
    path = args.readiness_manifest or args.package.parent / 'options-readiness-entry/green/source-manifest.json'
    native, san, archive, owned = validate(args.package, path)
    count = controls(native, san, archive, owned)
    print(f'PASS native1/sanitizer1;6x4 publications;133 input seals;20+53 scope;readiness anchor;{count} negative controls')

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Check WBS14 evidence without treating old snapshots as current aggregate tests."""
import hashlib
import json
from pathlib import Path
import re
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate(root=ROOT, matrix=None):
    root = Path(root)
    m = matrix if matrix is not None else json.loads(
        (root / 'migration/acceptance/clone.json').read_text())
    errors = []
    tasks = {}
    for line in (root / 'tasks.md').read_text().splitlines():
        columns = [part.strip() for part in line.split('|')]
        if len(columns) == 7 and re.fullmatch(r'14\.\d{3}(?:/[\w.-]+)?', columns[1]):
            tasks[columns[1]] = columns
    rows = m['rows']
    ids = [row['id'] for row in rows]
    if len(ids) != len(set(ids)) or set(ids) != set(tasks):
        errors.append('WBS14 coverage is incomplete or duplicated')

    for path, digest in m['reports'].items():
        if not (root / path).is_file() or sha(root / path) != digest:
            errors.append('Changed/missing evidence: ' + path)
    core = json.loads((root / 'migration/tests/clone-layer-sanitizers.json').read_text())
    normal = json.loads((root / 'migration/tests/clone-layer-testlog.json').read_text())
    if core['exit_code'] != 0 or normal['result'] != 'OK':
        errors.append('Clone core execution failed')
    for path, digest in m['current_core_sources_sha256'].items():
        if core['verified_input_sha256'].get(path) != digest or sha(root / path) != digest:
            errors.append('Core source/evidence mismatch: ' + path)

    snapshots = {}
    for label, snapshot in m['historical_snapshots'].items():
        archive = root / snapshot['archive']
        if not archive.is_file() or sha(archive) != snapshot['sha256']:
            errors.append('Historical archive mismatch: ' + label)
            continue
        with tarfile.open(archive) as tf:
            stream = tf.extractfile(snapshot['test_source'])
            content = stream.read() if stream else b''
        if hashlib.sha256(content).hexdigest() != snapshot['test_source_sha256']:
            errors.append('Historical test source mismatch: ' + label)
        report = json.loads((root / snapshot['source_report']).read_text())
        if report['sources_sha256'].get(snapshot['test_source']) != snapshot['test_source_sha256']:
            errors.append('Historical compiled test input mismatch: ' + label)
        if report.get('changed_during_build') or report.get('changed_during_run'):
            errors.append('Historical compiled input changed: ' + label)
        snapshots[label] = content.decode()

    native_xcf = [json.loads(line) for line in
                  (root / 'migration/tests/provenance-native.jsonl').read_text().splitlines()]
    native_xcf = [row for row in native_xcf if row['name'].endswith('painter-xcf-roundtrip')]
    sanitizer_xcf = json.loads((root / 'migration/tests/provenance-sanitizers.json').read_text())[
        'results']['painter-xcf-roundtrip']
    if len(native_xcf) != 1 or native_xcf[0]['result'] != 'OK' or sanitizer_xcf['exit_code'] != 0:
        errors.append('Historical XCF execution failed')
    dialog = json.loads((root / 'migration/tests/layer-dialog-store-results.json').read_text())

    def passed(test, output, context):
        if not re.search(r'^ok \d+ ' + re.escape(test) + r'$', output, re.M):
            errors.append(context + ': test not passed ' + test)

    test_source = (root / 'app/tests/test-gimp-clone-layer.c').read_text()
    for row in rows:
        name = row['id']
        task = tasks.get(name)
        if not task or row['dependencies'] != task[4] or row['acceptance_condition'] != task[5]:
            errors.append(name + ': stale task dependency/acceptance snapshot')
        if row['state'] not in ('COMPONENT_VERIFIED', 'PARTIAL', 'OPEN') or not row['component_scope']:
            errors.append(name + ': missing component state/scope')
        if row['state'] == 'COMPONENT_VERIFIED' and row['gaps']:
            errors.append(name + ': verified component still has a declared gap')
        if row['state'] != 'COMPONENT_VERIFIED' and not row['gaps']:
            errors.append(name + ': incomplete component must retain an explicit gap')
        if not row['named_tests']:
            errors.append(name + ': no named core evidence')
        for source in row['source']:
            if source not in m['current_core_sources_sha256']:
                errors.append(name + ': unsealed claimed core source ' + source)
        for test in row['named_tests']:
            passed(test, normal['stdout'], name + ' normal')
            passed(test, core['stdout'], name + ' sanitizers')
            if test.rsplit('/', 1)[1] not in test_source:
                errors.append(name + ': missing core test registration')
        for test in row['additional_roundtrip_tests']:
            if native_xcf:
                passed(test, native_xcf[0]['stdout'], name + ' historical XCF normal')
            passed(test, sanitizer_xcf['stdout'], name + ' historical XCF sanitizers')
            if test.rsplit('/', 1)[1] not in snapshots.get('xcf', ''):
                errors.append(name + ': missing historical XCF test registration')
        for test in row['additional_dialog_tests']:
            for mode, run in dialog['runtime'].items():
                if run['exit'] != 0 or test not in run['cases']:
                    errors.append(name + ': historical dialog ' + mode + ' failed')
                passed(test, (root / run['log']).read_text(), name + ' historical dialog ' + mode)
            if test.rsplit('/', 1)[1] not in snapshots.get('dialog', ''):
                errors.append(name + ': missing historical dialog test registration')
    return errors


if __name__ == '__main__':
    errors = validate()
    if errors:
        raise SystemExit('\n'.join(errors))
    print('WBS14: all 26 component rows reconciled; core source and historical XCF/GUI evidence verified. Dependency, remaining fixture and current aggregate gates stay open.')

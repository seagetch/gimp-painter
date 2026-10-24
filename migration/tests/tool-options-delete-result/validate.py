#!/usr/bin/env python3
"""Sealed native delete result proof; optional exact committed source check."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
P = Path(__file__).resolve().parent
R = P.parents[2]

def validate_results(data):
    assert data['clean_commit'] == '2ef2dd32eeb7201e8e0318f169c6005a21c10410'
    assert data['clean_source_count'] == 17096 and data['clean_source_changed'] == []
    assert data['linked_inputs_changed'] == []
    assert len(data['linked_inputs_sha256']) == 1322
    assert len(data['runtime_dependency_sha256']) == 127
    runs = {r['name']: r for r in data['results'] if r['name'].endswith('-run')}
    assert set(runs) == {'red-run', 'normal-run', 'asan-run'}
    assert runs['red-run']['returncode'] != 0
    assert "'error' should not be nullptr" in runs['red-run']['stdout'] + runs['red-run']['stderr']
    for name in ('normal-run', 'asan-run'):
        row = runs[name]
        assert row['returncode'] == 0
        assert '1..10' in row['stdout']
        assert sum(line.startswith('ok ') for line in row['stdout'].splitlines()) == 10
        assert 'direct-permission' in row['stdout'] and 'caller-permission' in row['stdout']
        assert 'SKIP' not in row['stdout']
    units = [r for r in data['results'] if r['name'].startswith('asan-compile-')]
    assert len(units) == 4
    assert all(r['returncode'] == 0 and '-fsanitize=address,undefined' in r['command'] for r in units)


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--commit'); args = parser.parse_args()
    hashes = json.loads((P / 'artifacts.json').read_text())
    assert set(hashes) == {f.name for f in P.iterdir() if f.is_file() and f.name != 'artifacts.json'}
    for name, digest in hashes.items():
        assert hashlib.sha256((P / name).read_bytes()).hexdigest() == digest, name
    data = json.loads((P / 'results.json').read_text()); validate_results(data)
    with tarfile.open(P / 'tested-sources.tar.gz') as tar:
        before = tar.extractfile('gimp-tools-before.c').read()
        after = tar.extractfile('gimp-tools-after.c').read()
        assert before.count(b'gimp_tool_options_delete (tool_info->tool_options, NULL)') == 1
        assert before.replace(b'gimp_tool_options_delete (tool_info->tool_options, NULL)', b'gimp_tool_options_delete (tool_info->tool_options, error)') == after
        assert hashlib.sha256(before).hexdigest() == data['source_sha256']['before-tools']
        assert hashlib.sha256(after).hexdigest() == data['source_sha256']['after-tools']
        assert hashlib.sha256((P / 'test.cpp').read_bytes()).hexdigest() == data['source_sha256']['test.cpp']
        for name in ('gimptoolinfo', 'gimptooloptions'):
            source = tar.extractfile('app_core_' + name + '.c').read()
            assert hashlib.sha256(source).hexdigest() == data['source_sha256']['app/core/' + name + '.c']
            if args.commit:
                assert subprocess.check_output(['git', 'show', args.commit + ':app/core/' + name + '.c'], cwd=R) == source
    if args.commit:
        assert subprocess.check_output(['git', 'show', args.commit + ':app/tools/gimp-tools.c'], cwd=R) == after
    print('PASS: causal red; native10 / selected4-unit ASan10; unchanged exact2ef source and link seals' + ('; committed source bound' if args.commit else '; receipt only'))

if __name__ == '__main__':
    main()

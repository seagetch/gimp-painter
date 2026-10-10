#!/usr/bin/env python3
"""Verify original 07.014 against four exact native CI report snapshots."""
import gzip
import hashlib
import json
from pathlib import Path
import re
import sys
sys.dont_write_bytecode = True
from test_painter_platform_link import foundation_results

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = Path('migration/tests/platform-link')
TARGETS = {'linux-x86_64': ('linux', 'x86_64', 'ELF', 8),
           'windows-x86_64': ('windows', 'x86_64', 'PE', 4),
           'macos-arm64': ('darwin', 'arm64', 'Mach-O', 8),
           'macos-x86_64': ('darwin', 'x86_64', 'Mach-O', 8)}


def validate(root=ROOT, index=None, reports=None, foundation_row=None):
    root = Path(root)
    index = index if index is not None else json.loads((root / DIRECTORY / 'native-acceptance.json').read_text())
    errors = []
    def require(condition, message):
        if not condition:
            errors.append('07.014: ' + message)
    rows = index['targets']
    require(len(rows) == 4 and {r['target'] for r in rows} == set(TARGETS), 'native target coverage differs')
    shared_tests = shared_sources = None
    for row in rows:
        name = row['target']
        if name not in TARGETS:
            continue
        system, arch, binary_format, long_size = TARGETS[name]
        if reports is None:
            raw = gzip.decompress((root / row['report']).read_bytes())
            require(hashlib.sha256(raw).hexdigest() == row['report_sha256'], name + ' report digest differs')
            data = json.loads(raw)
        else:
            data = reports[name]
        require(data['task'] == '07.014' and data['status'] == 'PASS' and not data.get('failure'), name + ' failed report')
        require((data['host']['system'], data['host']['architecture']) == (system, arch), name + ' wrong native host')
        require(data['ci']['GITHUB_SHA'] == index['source_commit'] and str(data['ci']['GITHUB_RUN_ID']) == str(index['run_id']), name + ' CI source/run differs')
        require(row['job_conclusion'] == 'success' and re.fullmatch('[0-9a-f]{64}', row['artifact_sha256']), name + ' artifact/job provenance missing')
        commands = data['commands']
        require(all(c.get('exit_code') == 0 and not c.get('error') for c in commands), name + ' command failed')
        outputs = [c['output'] for c in commands if len(c['argv']) == 1 and c['argv'][0].replace('\\', '/').split('/')[-1] in ('painter-foundation', 'painter-foundation.exe')]
        try:
            tests = foundation_results(outputs[0]) if len(outputs) == 1 else []
        except RuntimeError:
            tests = []
        require(bool(tests) and tests == data['foundation_tests'], name + ' foundation execution differs')
        if shared_tests is None:
            shared_tests = tests
        require(tests == shared_tests, name + ' cross-platform case set differs')
        expected_abi = f'PLATFORM_ABI_PASS pointer=8 long={long_size} object=24 value=24 error=16'
        require(data['abi_result'] == expected_abi and any(c['output'].strip() == expected_abi for c in commands), name + ' ABI result differs')
        require('painter_platform_export_control' in data['exports']['names'] and not data['exports']['private_cpp_exports'], name + ' public/private export control failed')
        require('GimpPainter::platform_export_leak_control()' in data['negative_export_control'], name + ' negative export control missing')
        require(set(data['unmangled_c_symbols']) == {'gimp_painter_binding_close', 'gimp_painter_error_quark', 'painter_platform_cpp_layout', 'painter_platform_roundtrip'}, name + ' C linkage differs')
        require(len(data['binaries']) == 3 and all(b['format'] == binary_format and b['architecture'] == arch and re.fullmatch('[0-9a-f]{64}', b['sha256']) for b in data['binaries'].values()), name + ' executable identity differs')
        require(('libc++' if system == 'darwin' else 'libstdc++') in data['runtime_dependencies'] and 'gobject-2.0' in data['runtime_dependencies'], name + ' runtime dependency differs')
        sources = data['source_sha256']
        if shared_sources is None:
            shared_sources = set(sources)
        require(set(sources) == shared_sources and len(sources) == 30, name + ' source set differs')
        transformed = []
        for path, expected in sources.items():
            raw = (root / path).read_bytes()
            if hashlib.sha256(raw).hexdigest() == expected:
                continue
            if system == 'windows' and b'\r\n' not in raw and hashlib.sha256(raw.replace(b'\n', b'\r\n')).hexdigest() == expected:
                transformed.append(path)
            else:
                require(False, name + ' report/source mismatch ' + path)
        require(sorted(transformed) == sorted(row['exact_LF_to_CRLF_checkout_matches']), name + ' checkout transformation record differs')
    if foundation_row is not None:
        require(set(foundation_row['reports']) == {r['report'] for r in rows}, 'foundation report index differs')
        require(set(foundation_row['implementation_paths']) == shared_sources, 'foundation source coverage differs')
        require(foundation_row['test_ids'] == shared_tests, 'foundation case index differs')
    return errors


if __name__ == '__main__':
    errors = validate()
    if errors:
        raise SystemExit('\n'.join(errors))
    print('Original 07.014: four native source/runtime/ABI/visibility reports verified')

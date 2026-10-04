#!/usr/bin/env python3
"""Verify XCF multipart evidence against current inputs and seal a compact result."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--check', action='store_true', help='Verify the existing seal without replacing it')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
folder = root / 'migration/tests/xcf-multipart-storage'
names = ['native-normal', 'native-sanitizers', 'isolated-normal', 'isolated-sanitizers',
         'arguments-storage', 'resources-256', 'resources-512']
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def require(condition, message):
    if not condition:
        raise RuntimeError(message)
reports = {name: json.loads((folder / (name + '.json')).read_text()) for name in names}
inputs, build_inputs = {}, {}
for name, report in reports.items():
    require(not report.get('changed_during_run') and not report.get('changed_during_build'), name + ': inputs changed')
    hashes = report.get('source_sha256', report.get('sources_sha256', report.get('source_sha256_before', {})))
    require(hashes, name + ': missing source seal')
    for path, digest in hashes.items():
        store = build_inputs if path.startswith('build-') else inputs
        require(path not in store or store[path] == digest, name + ': inconsistent input ' + path)
        store[path] = digest
        actual = root / path
        if actual.exists() or not (a.check and store is build_inputs):
            require(actual.exists() and sha(actual) == digest, name + ': current input differs: ' + path)
normal = reports['native-normal']
expected = {'xcf': 4, 'painter-xcf-open': 14, 'painter-xcf-adversarial': 247,
            'painter-provenance': 12, 'painter-xcf-roundtrip': 48}
require(set(normal['results']) == set(expected), 'Native target set differs')
for name, count in expected.items():
    result = normal['results'][name]
    require(result['exit_code'] == 0 and result['passed'] == count, 'Native failure/count: ' + name)
require(normal['results']['painter-xcf-roundtrip']['skipped'] == 1, 'Only the separately measured large case may skip')
required_cases = ['native-argument-budget', 'native-stream-failures', 'native-inert-ownership-changes',
                  'native-owner-duplicate-edit', 'native-callback-argument-leases', 'native-midprepare-cancel']
for name in required_cases:
    require(any(line.startswith('ok ') and name in line and '# SKIP' not in line
                for line in normal['results']['painter-xcf-roundtrip']['tap']), 'Missing native case: ' + name)
san = reports['native-sanitizers']
require(set(san['results']) == {'painter-xcf-adversarial', 'painter-provenance', 'painter-xcf-roundtrip'}, 'Sanitizer target set differs')
sanitizer_count = 0
for name, result in san['results'].items():
    count = sum(line.startswith('ok ') and '# SKIP' not in line for line in result['stdout'].splitlines())
    require(result['exit_code'] == 0 and count == expected[name], 'Sanitizer failure/count: ' + name)
    sanitizer_count += count
for name in ['isolated-normal', 'isolated-sanitizers']:
    for test, result in reports[name]['results'].items():
        require(result['exit_code'] == 0, name + ': failed ' + test)
    count = sum(line.startswith('ok ') for test in ['storage', 'multipart']
                for line in reports[name]['results'][test]['stdout'].splitlines())
    require(count == 12, name + ': expected twelve test groups')
args = reports['arguments-storage']
require(args['status'] == 'passed' and args['sources_unchanged'] and
        args['source_sha256_before'] == args['source_sha256_after'], 'Argument helper input failure')
require(all(check['exit_code'] == 0 for check in args['checks']), 'Argument helper check failure')
resource_summary = {}
for size in [256, 512]:
    report = reports['resources-' + str(size)]
    require(report['exit_code'] == 0 and not report['diagnostics'] and not report['transport_leftovers_after_exit'], 'Resource failure')
    require(report['payload_bytes'] == size * 1024 * 1024 + 1, 'Wrong resource payload')
    require(report['executable_sha256'] == normal['results']['painter-xcf-roundtrip']['executable_sha256'], 'Resource/normal executable mismatch')
    require(any(line.startswith('# multipart public-pdb payload=') for line in report['stdout_summary']), 'Resource run is not the ordinary PDB route')
    resource_summary[str(size) + '-MiB-plus-1'] = {key: report[key] for key in [
        'payload_bytes', 'elapsed_seconds', 'sampled_peak_rss_bytes', 'rss_components_at_sampled_peak',
        'sampled_peak_temp_fd_count', 'sampled_peak_temp_mapping_count',
        'sampled_peak_filesystem_allocated_delta_bytes', 'transport_leftovers_after_exit', 'stdout_summary']}
report_hashes = {str((folder / (name + '.json')).relative_to(root)): sha(folder / (name + '.json')) for name in names}
fixtures = ['migration/fixtures/legacy-runtime/manifest.json', 'migration/fixtures/legacy-xcf-fields/manifest.json']
fixture_hashes = {path: sha(root / path) for path in fixtures if (root / path).exists()}
seal_path = folder / 'acceptance.json'
if a.check:
    seal = json.loads(seal_path.read_text())
    require(seal['report_sha256'] == report_hashes, 'Acceptance report hashes differ')
    require(seal['source_sha256'] == inputs, 'Acceptance source union differs')
    require(seal['fixture_manifest_sha256'] == fixture_hashes, 'Fixture provenance differs')
else:
    seal = {
        'task': '12.015/multipart-storage', 'status': 'accepted',
        'baseline_commit': '6597d7b66a8b15ab6281c3f475a45cbc08f5ee27',
        'verified_utc': datetime.now(timezone.utc).isoformat(),
        'ordinary_writer_enabled': True,
        'normal_native_passed': sum(expected.values()), 'focused_native_sanitizer_passed': sanitizer_count,
        'isolated_groups_normal_and_sanitized': 12, 'argument_helper_checks': len(args['checks']),
        'instrumented_units': len(san['instrumented_sources']), 'rtti_only_units': len(san['rtti_compatibility_only_sources']),
        'source_sha256': inputs, 'build_input_sha256': build_inputs,
        'report_sha256': report_hashes, 'fixture_manifest_sha256': fixture_hashes,
        'resource_runs': resource_summary,
        'limits': [
            'Linux x86_64 native application API coverage; no interactive GUI or platform release claim',
            'Anonymous scratch remains small in measured unknown-byte cases; file-backed RSS scales with payload',
            'GLib whole-map validation and native mutable models are not a fixed-total-memory or latency guarantee',
            'Mutable strings/arrays retain a 256 MiB aggregate payload budget; oversized arguments remain opaque with cache/raw data',
            'FD allocation excludes inaccessible closed-FD mappings; filesystem delta includes other allocations on the same filesystem',
            'Focused application instrumentation only; other upstream/dependency units uninstrumented and LeakSanitizer disabled',
            'Forced-endian helper checks are not big-endian hardware validation',
            'A compact Filter provenance Save/Open precedes three measured large cycles; no byte-identical new-object first-save claim',
            'Complete field ledger, arbitrary malformed graphs, GUI responsiveness and Windows/macOS remain global gates'],
        'reproduce': [
            'Source the documented pinned Debian13 build environment and hold /workspace/shared/gimp-painter-build.lock',
            'Build native targets in app/tests; run run_xcf_multipart_native.py without an X server',
            'Run run_xcf_multipart_sanitizers.py --run and isolated storage/argument helper runners',
            'Run run_native_xcf_multipart_resources.py with --payload-mib 256 and 512',
            'Run this verifier with --check after generating the seal']}
    seal_path.write_text(json.dumps(seal, indent=2) + '\n')
print('XCF multipart evidence verified:', sum(expected.values()), 'native;', sanitizer_count, 'focused sanitizer cases')

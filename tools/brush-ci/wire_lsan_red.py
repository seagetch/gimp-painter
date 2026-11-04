#!/usr/bin/env python3
"""Strict C5 old-provider wire LSan negative control; never candidate acceptance."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

OLD_COMMIT = 'da070df3717723e75f0757113c2da2768949d676'
OLD_TREE = '544a91ca352614a58f024d1e3cd813ae0226df31'
TEST = 'libgimpbase/test-choice-lifecycle.c'
TEST_SHA = '46807f0fe5b7c64572da7be56855b2a41e23eb089c8fb990793b27bdd28ac8d3'
OLD_TEST_SHA = 'e778bcc8c940c9aa452cc1b28f5d7c8b1bc60f01fc8b31bc704f5842729f9b06'
CASE = '/choice-lifecycle/wire-roundtrip'
LOCK = 'migration/baseline/debian13-package-lock.json'
OLD_FILES = {
    'libgimpbase/gimpprotocol.c': '42b00612efd048ec5e0c1ae278ad1c6244d16bf018308c8685852017a93e07bc',
    'libgimpbase/gimpwire.c': '3ddd68f2d5d8c4315a32026e0d13954b45cc1d97a1193d7cfd568af1e926a366',
    'libgimpbase/gimpchoice.c': 'b40d9929d0b4758cd56667ffc1d33507172cdd2a3e156fb14be6f148d4172441',
    'libgimpbase/meson.build': '77271d9df8a60438edf7d4d96918a75f4cd747dee27fe8c743c625b0bcac072a',
    'meson.build': '83d371852ebeeede92caee7e0280f35dfc28a2a05cc8af6d00a84b67177d5b0f',
    LOCK: '553f4051b91626bc2f5470373fd3e8d67c8d61ec62103b2addcab7a16492129f',
}


def sha(data): return hashlib.sha256(data).hexdigest()
def git(root, *args): return subprocess.check_output(['git', '-C', str(root), *args])
def write(path, data): path.write_text(json.dumps(data, indent=2) + '\n')


def verify(source, receipt):
    if (receipt['old_commit'] != OLD_COMMIT or receipt['old_tree'] != OLD_TREE or
            receipt['test_overlay_sha256'] != TEST_SHA or receipt['production_imported'] is not False):
        raise ValueError('Control receipt differs from immutable reviewed identities')
    if git(source, 'rev-parse', 'HEAD').decode().strip() != OLD_COMMIT or git(source, 'rev-parse', 'HEAD^{tree}').decode().strip() != OLD_TREE:
        raise ValueError('Old-provider checkout identity changed')
    if (set(git(source, 'diff', '--name-only', 'HEAD').decode().splitlines()) != {TEST} or
            git(source, 'diff', '--cached', '--name-only').strip() or
            git(source, 'ls-files', '--others', '--exclude-standard').strip()):
        raise ValueError('Old-provider overlay exceeds the sole reviewed test file')
    for name, digest in dict(OLD_FILES, **{TEST: TEST_SHA}).items():
        path = source / name
        if path.is_symlink() or not path.is_file() or sha(path.read_bytes()) != digest:
            raise ValueError('Old-provider source identity changed: ' + name)
    if git(source, 'diff', '--submodule=short', '--', 'gimp-data').strip():
        raise ValueError('Old-provider submodule changed')
    if sha(git(source, 'diff', '--binary', 'HEAD', '--', TEST)) != receipt['test_overlay_patch_sha256']:
        raise ValueError('Old-provider overlay patch changed')


def prepare(source, candidate, pins, output):
    current = pins['candidate_final_commit']
    if pins.get('draft_only') is not False or not re.fullmatch('[0-9a-f]{40}', str(current)):
        raise ValueError('Automatic sealed candidate pins required')
    if git(candidate, 'rev-parse', 'HEAD').decode().strip() != current:
        raise ValueError('Candidate checkout differs from runtime seal')
    for root in (source, candidate):
        if git(root, 'status', '--porcelain=v1', '--untracked-files=all').strip():
            raise ValueError('Fresh clean checkouts required')
    if git(source, 'rev-parse', 'HEAD').decode().strip() != OLD_COMMIT or git(source, 'rev-parse', 'HEAD^{tree}').decode().strip() != OLD_TREE:
        raise ValueError('Wrong old-provider checkout')
    if sha(git(source, 'show', OLD_COMMIT + ':' + TEST)) != OLD_TEST_SHA:
        raise ValueError('Historical test source changed')
    overlay = candidate / TEST
    if overlay.is_symlink() or sha(overlay.read_bytes()) != TEST_SHA:
        raise ValueError('Candidate test differs from reviewed wire test')
    if sha((candidate / LOCK).read_bytes()) != OLD_FILES[LOCK]:
        raise ValueError('Candidate dependency lock differs from immutable old-provider lock')
    for name, digest in OLD_FILES.items():
        if sha((source / name).read_bytes()) != digest:
            raise ValueError('Wrong old-provider source: ' + name)
    (source / TEST).write_bytes(overlay.read_bytes())
    patch = git(source, 'diff', '--binary', 'HEAD', '--', TEST)
    output.with_suffix('.patch').write_bytes(patch)
    receipt = {'old_commit': OLD_COMMIT, 'old_tree': OLD_TREE,
               'candidate_commit': current, 'test_overlay_path': TEST,
               'test_overlay_sha256': TEST_SHA, 'old_test_sha256': OLD_TEST_SHA,
               'test_overlay_patch_sha256': sha(patch), 'production_imported': False,
               'old_source_sha256': OLD_FILES, 'gimp_data_commit': git(source, 'rev-parse', 'HEAD:gimp-data').decode().strip(),
               'scope': 'Only exact new choice lifecycle test over immutable C5 production; separately built real Meson provider, no binary substitution',
               'negative_control': True, 'acceptance_eligible': False}
    verify(source, receipt)
    write(output, receipt)


def check(rows):
    report = {'control_passed': False, 'negative_control': True, 'acceptance_eligible': False,
              'scope': 'Expected old-provider wire choice decoder LSan RED, not candidate PASS; candidate Choice4 sanitizer GREEN remains required',
              'errors': [], 'tests': [], 'selected_case': CASE, 'raw_environment_exported': False}
    errors = report['errors']
    if len(rows) != 1 or not isinstance(rows[0], dict):
        errors.append('Expected exactly one old-provider registration')
        return report
    row = rows[0]
    report['tests'] = [{key: row.get(key) for key in ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')}]
    command = row.get('command', [])
    if not isinstance(command, list) or not all(isinstance(x, str) for x in command): command = []
    selected = [command[i + 1] for i, value in enumerate(command[:-1]) if value == '-p']
    forbidden = ('-s', '-x', '--skip-prefix', '-r', '--run-prefix', '-l', '--list', '-m')
    if (selected != [CASE] or command.count('-p') != 1 or
            not any(Path(arg).name == 'test-choice-lifecycle' for arg in command) or
            any(x.split('=')[0] in forbidden or x.startswith(('--GTestSkipCount', '--GTestSubprocess')) for x in command) or
            any(x.startswith('-p') and x != '-p' for x in command)):
        errors.append('Must select only exact wire-roundtrip without skip controls')
    if (row.get('name', '').rsplit(' / ', 1)[-1] != 'choice-lifecycle' or
            row.get('result') != 'FAIL' or type(row.get('returncode')) is not int or row['returncode'] != 1):
        errors.append('Expected native LSan FAIL1, not pass/timeout/abort/setup failure')
    stdout, stderr = str(row.get('stdout', '')), str(row.get('stderr', ''))
    output = stdout + '\n' + stderr
    if (re.findall(r'^(?:not )?ok\b[^\n]*', stdout, re.M) != ['ok 1 ' + CASE] or
            re.findall(r'^1\.\.(\d+)\s*$', stdout, re.M) != ['1'] or
            re.search(r'#\s*(?:SKIP|TODO)\b|Bail out|assertion|SIGSEGV|Segmentation fault|CRITICAL|runtime error:', output, re.I)):
        errors.append('Wire functional TAP case must complete without unrelated native failures')
    header = re.findall(r'^==test-choice-lifecycle==\d+==ERROR: LeakSanitizer: detected memory leaks\s*$', stderr, re.M)
    summary = re.findall(r'^SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)\.\s*$', stderr, re.M)
    diagnostics = [line.strip() for line in output.splitlines() if re.search(r'(?:ERROR|WARNING|SUMMARY):\s*(?:AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer)|DEADLYSIGNAL|fatal error|CHECK failed', line)]
    if len(header) != 1 or len(summary) != 1 or len(diagnostics) != 2 or any('WARNING:' in line for line in diagnostics):
        errors.append('Expected only one real named LeakSanitizer report and its leak summary')
    blocks = re.findall(r'((?:Direct|Indirect) leak of (\d+) byte\(s\) in (\d+) object\(s\) allocated from:\n)(.*?)(?=\n(?:Direct|Indirect) leak of |\nSUMMARY:|\Z)', stderr, re.S)
    if not blocks:
        errors.append('Missing real allocation stacks')
    total_bytes = total_allocations = 0
    for title, size, count, stack in blocks:
        total_bytes += int(size); total_allocations += int(count)
        if (not title.startswith('Direct leak') or int(size) <= 0 or int(count) <= 0 or
                not re.search(r'#\d+ .*\b_gimp_wire_read_string\b[^\n]*gimpwire\.c:455\b', stack) or
                not re.search(r'#\d+ .*\b_gp_param_def_read\b[^\n]*gimpprotocol\.c:(?:1139|1155|1159|1161)\b', stack) or
                not re.search(r'#\d+ .*\bwire_roundtrip\b[^\n]*test-choice-lifecycle\.c:\d+\b', stack)):
            errors.append('Unrelated or incomplete allocation stack, not old choice decoder')
    if summary and (total_bytes, total_allocations) != tuple(map(int, summary[0])):
        errors.append('Leak summary differs from attributed allocation blocks')
    if re.search(r'LeakSanitizer has encountered|under ptrace|AddressSanitizer:DEADLYSIGNAL|UndefinedBehaviorSanitizer|runtime error:|WARNING|CRITICAL|Traceback', output, re.I):
        errors.append('Unrelated sanitizer/runtime diagnostic')
    report['attributed_leak_bytes'] = total_bytes
    report['attributed_leak_allocations'] = total_allocations
    report['control_passed'] = not errors
    return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path); p.add_argument('--candidate', type=Path)
    p.add_argument('--pins', type=Path); p.add_argument('--output', type=Path)
    p.add_argument('--verify', type=Path); p.add_argument('--log', type=Path)
    a = p.parse_args()
    try:
        if a.log:
            if not a.output or any((a.source, a.candidate, a.pins, a.verify)): raise ValueError('Check requires only raw log and output')
            report = check([json.loads(line) for line in a.log.read_text().splitlines() if line.strip()])
            write(a.output, report)
            print(json.dumps({'control_passed': report['control_passed'], 'negative_control': True}))
            return 0 if report['control_passed'] else 1
        if a.verify:
            if not a.source: raise ValueError('Verify requires old source')
            verify(a.source, json.loads(a.verify.read_text()))
        else:
            if not all((a.source, a.candidate, a.pins, a.output)): raise ValueError('Prepare requires both sources, seal and output')
            prepare(a.source, a.candidate, json.loads(a.pins.read_text()), a.output)
        return 0
    except (OSError, ValueError, TypeError, KeyError, subprocess.CalledProcessError) as error:
        print('Old-provider wire control failed closed: ' + str(error))
        return 1


if __name__ == '__main__': raise SystemExit(main())

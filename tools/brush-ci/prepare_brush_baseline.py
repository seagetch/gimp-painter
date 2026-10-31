#!/usr/bin/env python3
"""DRAFT, fail-closed: final Brush include + two registration lines only.

No baseline preparation is permitted until root pins the accepted task-307 SHA
and final Brush candidate SHA. --verify proves all tracked production remains
at that baseline and the sole untracked file is the frozen test include.
No Meson/header/prerequisite/production edits are allowed for this oracle.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

MAIN = 'app/tests/test-painter-compact-options.c'
TEST = 'app/tests/test-painter-brush-thumbnail-popup.inc'


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inject_main(data):
    text = data.decode()
    for old, extra in (
        ('#include "test-painter-clone-popup.inc"\n', '#include "test-painter-brush-thumbnail-popup.inc"\n'),
        ('  register_clone_popup_tests ();\n', '  register_brush_thumbnail_popup_tests ();\n'),
    ):
        if text.count(old) != 1 or extra in text:
            raise ValueError('Changed/ambiguous Brush registration anchor')
        text = text.replace(old, old + extra, 1)
    return text.encode()


def commits(pins):
    names = ('accepted_task307_baseline_commit', 'candidate_final_commit')
    values = [pins.get(name) for name in names]
    if any(not isinstance(v, str) or not re.fullmatch('[0-9a-f]{40}', v) for v in values):
        raise ValueError('BLOCKED: accepted task-307 baseline SHA and final candidate SHA must be pinned by root')
    if pins.get('draft_only') is not False:
        raise ValueError('BLOCKED: root must explicitly review pins and clear draft_only')
    return values


def verify(repo, report):
    if git(repo, 'rev-parse', 'HEAD').decode().strip() != report['baseline_commit']:
        raise ValueError('Baseline HEAD differs from approved pin')
    changed = set(git(repo, 'diff', '--name-only', 'HEAD').decode().splitlines())
    staged = git(repo, 'diff', '--cached', '--name-only')
    untracked = set(git(repo, 'ls-files', '--others', '--exclude-standard').decode().splitlines())
    if changed != {MAIN} or staged.strip() or untracked != {TEST}:
        raise ValueError('Baseline source changes exceed the sole registration main and new test include')
    expected_main = inject_main(git(repo, 'show', report['baseline_commit'] + ':' + MAIN))
    if (repo / MAIN).read_bytes() != expected_main:
        raise ValueError('Baseline main is not exact original + two insertion lines')
    for name, expected in report['files'].items():
        path = repo / name
        if path.is_symlink() or sha(path.read_bytes()) != expected:
            raise ValueError('Frozen overlay differs: ' + name)
    if git(repo, 'diff', '--submodule=short', '--', 'gimp-data').strip():
        raise ValueError('Submodule differs from baseline')


def prepare(baseline, candidate, pins, output):
    base_commit, final_commit = commits(pins)
    for repo, expected in ((baseline, base_commit), (candidate, final_commit)):
        if git(repo, 'rev-parse', 'HEAD').decode().strip() != expected:
            raise ValueError('Wrong pinned HEAD: ' + str(repo))
        if git(repo, 'status', '--porcelain=v1', '--untracked-files=all').strip():
            raise ValueError('Fresh clean worktree required: ' + str(repo))
    before = git(baseline, 'show', base_commit + ':' + MAIN)
    main = inject_main(before)
    test = (candidate / TEST).read_bytes()
    if sha(test) != pins['test_include_sha256']:
        raise ValueError('Final test include hash changed; re-review/re-pin the oracle')
    if sha(main) != pins['candidate_main_sha256'] or main != (candidate / MAIN).read_bytes():
        raise ValueError('Candidate main differs beyond exact test include/registration')
    for name, data in ((MAIN, main), (TEST, test)):
        if (baseline / name).is_symlink():
            raise ValueError('Refusing symlink')
        (baseline / name).write_bytes(data)
    report = {'baseline_commit': base_commit, 'candidate_commit': final_commit,
              'overlay_scope': 'test include + two main registration lines ONLY',
              'baseline_main_sha256': sha(before), 'files': {MAIN: sha(main), TEST: sha(test)},
              'production_imported': False}
    verify(baseline, report)
    patch = git(baseline, 'diff', '--binary', 'HEAD', '--', MAIN)
    output.with_suffix('.patch').write_bytes(patch)
    report['tracked_overlay_patch_sha256'] = sha(patch)
    output.write_text(json.dumps(report, indent=2) + '\n')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path)
    p.add_argument('--pins', type=Path)
    p.add_argument('--output', type=Path)
    p.add_argument('--verify', type=Path)
    a = p.parse_args()
    if a.verify:
        verify(a.baseline.resolve(), json.loads(a.verify.read_text()))
    else:
        if not a.candidate or not a.pins or not a.output:
            p.error('--candidate, --pins, --output required to prepare')
        prepare(a.baseline.resolve(), a.candidate.resolve(), json.loads(a.pins.read_text()), a.output.resolve())
    print('Exact Brush test-only overlay verified; native execution is a separate gate')


if __name__ == '__main__':
    main()

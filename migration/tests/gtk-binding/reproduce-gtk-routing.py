#!/usr/bin/env python3
"""Reproduce the bounded 06.023 routing correction, never a full-tree audit."""
import argparse
from collections import Counter
import copy
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / 'tools'))
import assign_legacy_hunks as assignment
import audit_legacy_granularity as granularity
from legacy_assignment_rules import route

BASE = '4f2f5de894c556ae0218049913179ac998d8c7ed'
INV = ROOT / 'migration/inventory'
ADDED_SIDS = {'01.002/001641', '01.002/001643', '01.002/001802'}
ASSIGN = 'hunk-wbs.tsv'
WORK = 'legacy-port-work-items.tsv'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def blob_id(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()


def git(*args, **kwargs):
    return subprocess.check_output(['git', *args], cwd=ROOT, **kwargs)


def prior(path):
    return git('show', BASE + ':' + path)


def rows(data):
    return list(csv.DictReader(io.StringIO(data.decode()), delimiter='\t'))


def encode(data):
    return assignment.tsv(tuple(data[0]), data).encode()


def load_baseline():
    paths = git('ls-tree', '-r', '--name-only', BASE, 'migration/inventory').decode().splitlines()
    return {p.removeprefix('migration/inventory/'): prior(p) for p in paths}


def load_cases():
    data = json.loads((HERE / 'routing-cases.json').read_text())
    require(data['baseline'] == BASE, 'Review parent differs')
    cases = data['cases']
    require(len(cases) == len({c['hunk_id'] for c in cases}) == 96, 'Duplicate/missing reviewed hunk')
    require(Counter(c['classification'] for c in cases) == {
        'direct-dsl-ownership': 1, 'bounded-caller-provider': 14,
        'unrelated': 78, 'new-direct-caller': 3}, 'Reviewed classification count differs')
    require({c['hunk_id'] for c in cases if c['classification'] == 'new-direct-caller'} == ADDED_SIDS,
            'New caller set differs')
    return cases


def load_sources():
    with tarfile.open(HERE / 'routing-inputs.tar.gz') as archive:
        members = archive.getmembers()
        require(all(m.isfile() for m in members), 'Evidence contains non-file member')
        require(len(members) == len({m.name for m in members}) == 51, 'Evidence member count differs')
        return {m.name: archive.extractfile(m).read() for m in members}


def verified_sections(cases, baseline, sources):
    """Rebuild zero-context hunks using the existing parser on partial trees."""
    assignments = {r['hunk_id']: r for r in rows(baseline[ASSIGN])}
    hunks = {r['child_id']: r for r in rows(baseline['changed-hunks.tsv'])}
    files = {r['path']: r for r in rows(baseline['changed-files.tsv'])}
    wanted = {}
    for case in cases:
        sid = case['hunk_id']; before = assignments[sid]
        for key in ('path', 'source_blob', 'base_blob', 'old_new_lines', 'payload_sha256'):
            require(case[key] == before[key], sid + ': changed reviewed ' + key)
        require(case['implementation_tasks_before'] == before['implementation_tasks'], sid + ': wrong prior assignment')
        require(case['verification_tasks_unchanged'] == before['verification_tasks'], sid + ': changed verification tasks')
        for role in ('source', 'base'):
            oid = before[role + '_blob']
            require(oid == files[before['path']][role + '_git_blob'], sid + ': file/blob identity differs')
            if oid:
                wanted[role + '/' + before['path']] = oid
    require(set(sources) == set(wanted), 'Evidence source coverage differs')
    for name, oid in wanted.items():
        require(blob_id(sources[name]) == oid, 'Changed source/base blob: ' + name)
    with tempfile.TemporaryDirectory(prefix='painter-gtk-source-proof-') as directory:
        def local_git(*args, **kwargs):
            return subprocess.check_output(['git', *args], cwd=directory, **kwargs)
        local_git('init', '-q')
        for name, data in sources.items():
            require(local_git('hash-object', '-w', '--stdin', input=data).decode().strip() == wanted[name],
                    'Git blob verification failed: ' + name)
        trees = {}
        for role in ('source', 'base'):
            local_git('read-tree', '--empty')
            for name, oid in sorted(wanted.items()):
                if name.startswith(role + '/'):
                    local_git('update-index', '--add', '--cacheinfo', '100644', oid, name[len(role) + 1:])
            trees[role] = local_git('write-tree').decode().strip()
        original_git = assignment.git
        try:
            assignment.git = local_git
            sections = assignment.patch_sections(trees['base'], trees['source'])
        finally:
            assignment.git = original_git
    require(len(sections) == 96, 'Partial source diff must contain exactly the 96 reviewed hunks')
    for case in cases:
        sid = case['hunk_id']; hunk = hunks[sid]; section = sections[(case['path'], hunk['index'])]
        require(section['header'] == case['old_new_lines'] == hunk['old_new_lines'], sid + ': source range differs')
        require(section['kind'] == hunk['kind'], sid + ': hunk kind differs')
        require(section['scope'] == assignments[sid]['source_scope'], sid + ': source scope differs')
        require(digest(section['payload']) == case['payload_sha256'], sid + ': changed zero-context payload')
    return sections, trees


def specify(hunk_bytes, asset_bytes, catalog):
    with tempfile.TemporaryDirectory(prefix='painter-gtk-work-proof-') as directory:
        target = Path(directory)
        (target / ASSIGN).write_bytes(hunk_bytes)
        (target / 'asset-wbs.tsv').write_bytes(asset_bytes)
        return granularity.specification(catalog, target)


def validate_work(expected, actual):
    errors = granularity.validate(expected, actual)
    require(not errors, '\n'.join(errors[:10]))


def regenerate(baseline, cases, sections, catalog):
    """Change one task only; preserve every surviving work row byte for byte."""
    before_assignments = rows(baseline[ASSIGN])
    before_work = rows(baseline[WORK])
    original_spec, _ = specify(baseline[ASSIGN], baseline['asset-wbs.tsv'], catalog)
    validate_work(original_spec, before_work)
    old = {r['work_id']: r for r in before_work}
    require(len(old) == 22943 and Counter(r['status'] for r in before_work) == {'TODO': 22408, 'DONE': 535},
            'Public parent work/status counts differ')
    reviewed = {c['hunk_id']: c for c in cases}
    require({r['hunk_id'] for r in before_assignments if '06.023' in r['implementation_tasks'].split(',')}
            == set(reviewed) - ADDED_SIDS, 'Original 06.023 review coverage differs')
    assignments = copy.deepcopy(before_assignments)
    hunks = rows(baseline['changed-hunks.tsv'])
    by_hunk = {r['child_id']: r for r in hunks}
    removed_ids = {c['work_id'] for c in cases if c['classification'] == 'unrelated'}
    added_ids = {c['work_id'] for c in cases if c['classification'] == 'new-direct-caller'}
    for row in assignments:
        case = reviewed.get(row['hunk_id'])
        if case is None:
            continue
        hunk = by_hunk[row['hunk_id']]
        section = sections[(row['path'], hunk['index'])]
        result = route(hunk, section['added'], section['removed'])
        require(','.join(result['tasks']) == case['proposed_implementation_tasks'], row['hunk_id'] + ': wrong routed assignment')
        for result_key, row_key in (('profile', 'profile'), ('feature', 'feature'), ('reason', 'reason')):
            require(result[result_key] == row[row_key], row['hunk_id'] + ': changed unrelated ' + row_key)
        # Reproduce only this historical 06.023 implementation correction.
        # Later independent verification routing (notably original 07.008)
        # must not rewrite its copied, reviewed baseline verification fields.
        require(row['verification_tasks'] == case['verification_tasks_unchanged'],
                row['hunk_id'] + ': changed historical verification tasks')
        old_tasks = row['implementation_tasks'].split(','); new_tasks = result['tasks']
        require([t for t in old_tasks if t != '06.023'] == [t for t in new_tasks if t != '06.023'],
                row['hunk_id'] + ': changed unrelated implementation task')
        row['implementation_tasks'] = ','.join(new_tasks)
        hunk['disposition_task'] = ','.join(new_tasks)
    # Match audit_legacy_candidates.render's existing last-hunk convention;
    # preserve its special cleanup dispositions and every other field.
    old_last = {r['path']: r for r in before_assignments}
    new_last = {r['path']: r for r in assignments}
    candidates = rows(baseline['cleanup-candidate-review.tsv'])
    for row in candidates:
        previous = old_last[row['path']]; current = new_last[row['path']]
        if previous['implementation_tasks'] != current['implementation_tasks']:
            if row['implementation_tasks'] == previous['implementation_tasks']:
                row['implementation_tasks'] = current['implementation_tasks']
            else:
                require('06.023' not in row['implementation_tasks'].split(','), 'Unreviewed special cleanup routing')
    outputs = {ASSIGN: encode(assignments), 'changed-hunks.tsv': encode(hunks),
               'cleanup-candidate-review.tsv': encode(candidates)}
    expected, checks = specify(outputs[ASSIGN], baseline['asset-wbs.tsv'], catalog)
    wanted = {r['work_id']: r for r in expected}
    require(set(old) - set(wanted) == removed_ids and set(wanted) - set(old) == added_ids,
            'Work additions/removals differ from exact reviewed set')
    require(len(removed_ids) == 78 and len(added_ids) == 3 and len(expected) == 22868, 'Work delta count differs')
    for wid in removed_ids:
        require(old[wid]['wbs_task'] == '06.023' and old[wid]['status'] == 'TODO', 'Removal would discard non-TODO work')
    for row in expected:
        wid = row['work_id']
        if wid in added_ids:
            require(row['wbs_task'] == '06.023' and row['source_id'] in ADDED_SIDS and row['status'] == 'TODO',
                    'New duty must remain TODO')
        else:
            previous = old[wid]
            require(all(row[k] == previous[k] for k in granularity.FIELDS if k not in granularity.MUTABLE),
                    wid + ': changed retained source/action identity')
            row.update({k: previous[k] for k in granularity.MUTABLE})
            require(row == previous, wid + ': changed retained work/evidence')
    validate_work(expected, expected)
    require(sum(r['status'] == 'DONE' for r in expected) == 535, 'Routing changed DONE count')
    other_reviewed = [r for r in before_work if r['source_id'] in set(reviewed) - ADDED_SIDS and r['wbs_task'] != '06.023']
    require(len(other_reviewed) == 372 and all(wanted[r['work_id']] == r for r in other_reviewed),
            'The 372 other reviewed duties must remain unchanged')
    outputs[WORK] = assignment.tsv(granularity.FIELDS, expected).encode()
    outputs['granularity-review.tsv'] = assignment.tsv(granularity.AUDIT_FIELDS, checks).encode()
    for name, field, value in [
        ('wbs-assignment-summary.json', 'hunk_ledger_sha256', digest(outputs[ASSIGN])),
        ('changed-hunks.json', 'inventory_sha256', digest(outputs['changed-hunks.tsv'])),
        ('granularity-summary.json', 'implementation_actions', 16074 - 78 + 3),
    ]:
        summary = json.loads(baseline[name]); summary[field] = value
        outputs[name] = (json.dumps(summary, indent=2) + '\n').encode()
    report = dict(baseline=BASE, reviewed_hunks=96, verified_source_base_blobs=51,
                  removed_todo_work_ids=sorted(removed_ids), added_todo_work_ids=sorted(added_ids),
                  removed_todo_count=78, added_todo_count=3, retained_work_items=len(old) - 78,
                  preserved_other_reviewed_work_items=372, preserved_done_work_items=535,
                  done_added_by_routing=0, work_items_after_routing=len(expected),
                  remaining_06023_assignments=sum('06.023' in r['implementation_tasks'].split(',') for r in assignments),
                  full_historical_generator_pass=False,
                  limitation='Only 96 byte-verified hunks from 29 paths were rerouted. Complete historical trees are absent; no full generator PASS is claimed.',
                  output_sha256={name: digest(data) for name, data in outputs.items()})
    return outputs, report


def check_completed_output(outputs, actual):
    """Permit only valid execution evidence on this gate's 18 exact duties."""
    for name, data in outputs.items():
        if name != WORK:
            require(actual[name] == data, 'Routing output differs: ' + name)
            continue
        expected, observed = rows(data), rows(actual[name])
        validate_work(expected, observed)
        require(len(expected) == len(observed), 'Work row count differs')
        for before, after in zip(expected, observed):
            require(before['work_id'] == after['work_id'], 'Work order differs')
            mutable = granularity.MUTABLE if before['wbs_task'] == '06.023' else ()
            require(all(before[k] == after[k] for k in granularity.FIELDS if k not in mutable),
                    before['work_id'] + ': changed unrelated work or source identity')


def guard_write(baseline, outputs, actual):
    require(set(actual) == set(baseline), 'Refusing later added/removed inventory files')
    for name, data in actual.items():
        require(data in (baseline[name], outputs.get(name, baseline[name])),
                'Refusing to overwrite later inventory work: ' + name)


def reproduce():
    baseline = load_baseline(); cases = load_cases(); sources = load_sources()
    sections, trees = verified_sections(cases, baseline, sources)
    with tempfile.TemporaryDirectory(prefix='painter-gtk-catalog-') as directory:
        path = Path(directory) / 'tasks.md'; path.write_bytes(prior('tasks.md'))
        catalog = assignment.task_catalog(path)
    outputs, report = regenerate(baseline, cases, sections, catalog)
    report['verified_partial_trees'] = trees
    return baseline, outputs, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--output', type=Path, help='Stage expected tables for review')
    mode.add_argument('--check', action='store_true', help='Check routing plus valid acceptance evidence on only the 18 reviewed duties')
    mode.add_argument('--write', action='store_true', help='Apply only to untouched parent or exact routing outputs')
    args = parser.parse_args(); baseline, outputs, report = reproduce()
    if args.write:
        actual = {str(p.relative_to(INV)): p.read_bytes() for p in INV.rglob('*') if p.is_file()}
        guard_write(baseline, outputs, actual)
        for name, data in outputs.items():
            (INV / name).write_bytes(data)
    elif args.check:
        check_completed_output(outputs, {name: (INV / name).read_bytes() for name in outputs})
    elif args.output:
        require(not args.output.resolve().is_relative_to(INV.resolve()),
                'Use guarded --write for the real inventory')
        args.output.mkdir(parents=True, exist_ok=True)
        for name, data in outputs.items():
            (args.output / name).write_bytes(data)
        (args.output / 'routing-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

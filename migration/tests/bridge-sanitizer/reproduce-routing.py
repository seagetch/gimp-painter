#!/usr/bin/env python3
"""Reproduce only the fixed original 07.013 shared-bridge sanitizer routing delta."""
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

BASE = 'afce4dc0c2dc0673a0b8851bed25d2d35223a300'
INV = ROOT / 'migration/inventory'
ASSIGN = 'hunk-wbs.tsv'
WORK = 'legacy-port-work-items.tsv'
TASK = '07.013'
KEEP = {'01.002/000042', '01.002/000047', '01.002/000078', '01.002/000079'}
ARCHIVES = ('migration/tests/bridge-sanitizer/routing-inputs.tar.gz',
            'migration/tests/gtk-binding/caller-census.tar.gz')


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
    require(data['baseline'] == BASE and data['task'] == TASK, 'Review parent/task differs')
    require(set(data['source_archives']) == set(ARCHIVES), 'Source archive set differs')
    for path in ARCHIVES:
        require(data['source_archives'][path] == digest((ROOT / path).read_bytes()), 'Reviewed archive differs: ' + path)
    cases = data['cases']
    require(len(cases) == len({c['source_id'] for c in cases}) == 19, 'Duplicate/missing reviewed hunk')
    require(len({c['work_id'] for c in cases}) == 19, 'Duplicate reviewed work ID')
    require(Counter(c['classification'] for c in cases) == {
        'shared-bridge-helper': 4, 'feature-outside-shared-bridge': 15}, 'Reviewed classification count differs')
    require({c['source_id'] for c in cases if c['recommendation'] == 'retain07.013'} == KEEP,
            'Retained helper set differs')
    for case in cases:
        kept = case['source_id'] in KEEP
        require(case['classification'] == ('shared-bridge-helper' if kept else 'feature-outside-shared-bridge'),
                case['source_id'] + ': classification differs')
        before = case['verification_tasks_before'].split(',')
        expected = before if kept else [t for t in before if t != TASK]
        require(TASK in before and case['verification_tasks_proposed'] == ','.join(expected),
                case['source_id'] + ': review changes other verification tasks')
        require(bool(case['reason'].strip()), case['source_id'] + ': missing source-specific review')
    return cases


def load_sources(cases):
    wanted = {role + '/' + c['path'] for c in cases for role in ('source', 'base') if c[role + '_blob']}
    sources = {}
    for path in ARCHIVES:
        data = (ROOT / path).read_bytes()
        if path != 'migration/tests/bridge-sanitizer/routing-inputs.tar.gz':
            require(data == prior(path), 'Changed historical evidence archive: ' + path)
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            members = archive.getmembers()
            require(all(m.isfile() for m in members), 'Evidence contains non-file member')
            require(len(members) == len({m.name for m in members}), 'Duplicate archive member')
            for member in members:
                if member.name in wanted:
                    content = archive.extractfile(member).read()
                    require(member.name not in sources or sources[member.name] == content,
                            'Conflicting evidence: ' + member.name)
                    sources[member.name] = content
    require(set(sources) == wanted and len(sources) == 26, 'Evidence source/base coverage differs')
    return sources


def verified_sections(cases, baseline, sources):
    """Rebuild the exact 19 zero-context hunks with the existing diff parser."""
    assignments = {r['hunk_id']: r for r in rows(baseline[ASSIGN])}
    hunks = {r['child_id']: r for r in rows(baseline['changed-hunks.tsv'])}
    files = {r['path']: r for r in rows(baseline['changed-files.tsv'])}
    require({r['hunk_id'] for r in assignments.values() if TASK in r['verification_tasks'].split(',')}
            == {c['source_id'] for c in cases}, 'Original 07.013 review coverage differs')
    wanted = {}
    for case in cases:
        sid = case['source_id']; before = assignments[sid]
        for key in ('path', 'source_blob', 'base_blob', 'payload_sha256', 'source_scope'):
            require(case[key] == before[key], sid + ': changed reviewed ' + key)
        require(case['source_range'] == before['old_new_lines'], sid + ': changed reviewed source range')
        require(case['implementation_tasks_unchanged'] == before['implementation_tasks'], sid + ': wrong implementation tasks')
        require(case['verification_tasks_before'] == before['verification_tasks'], sid + ': wrong prior verification tasks')
        for role in ('source', 'base'):
            oid = before[role + '_blob']
            require(oid == files[before['path']][role + '_git_blob'], sid + ': file/blob identity differs')
            if oid:
                wanted[role + '/' + before['path']] = oid
    require(set(sources) == set(wanted), 'Evidence source coverage differs')
    for name, oid in wanted.items():
        require(blob_id(sources[name]) == oid, 'Changed source/base blob: ' + name)
    with tempfile.TemporaryDirectory(prefix='painter-bridge-sanitizer-source-proof-') as directory:
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
    require(len(sections) >= 19, 'Insufficient source diff coverage')
    for case in cases:
        sid = case['source_id']; hunk = hunks[sid]
        section = sections[(case['path'], hunk['index'])]
        require(section['header'] == case['source_range'] == hunk['old_new_lines'], sid + ': source range differs')
        require(section['kind'] == hunk['kind'], sid + ': hunk kind differs')
        require(section['scope'] == case['source_scope'], sid + ': source scope differs')
        require(digest(section['payload']) == case['payload_sha256'], sid + ': changed zero-context payload')
    return sections, trees


def specify(hunk_bytes, asset_bytes, catalog):
    with tempfile.TemporaryDirectory(prefix='painter-bridge-sanitizer-work-proof-') as directory:
        target = Path(directory)
        (target / ASSIGN).write_bytes(hunk_bytes)
        (target / 'asset-wbs.tsv').write_bytes(asset_bytes)
        return granularity.specification(catalog, target)


def validate_work(expected, actual):
    errors = granularity.validate(expected, actual)
    require(not errors, '\n'.join(errors[:10]))


def regenerate(baseline, cases, sections, catalog):
    """Remove one verification duty only; preserve every retained row field."""
    before_assignments = rows(baseline[ASSIGN])
    before_work = rows(baseline[WORK])
    original_spec, _ = specify(baseline[ASSIGN], baseline['asset-wbs.tsv'], catalog)
    validate_work(original_spec, before_work)
    old = {r['work_id']: r for r in before_work}
    require(len(old) == 22778 and Counter(r['status'] for r in before_work) == {'TODO': 22202, 'DONE': 576},
            'Public parent work/status counts differ')
    reviewed = {c['source_id']: c for c in cases}
    require({r['source_id'] for r in before_work if r['wbs_task'] == TASK} == set(reviewed),
            'Original 07.013 work coverage differs')
    removed_ids = {c['work_id'] for c in cases if c['source_id'] not in KEEP}
    retained_ids = {c['work_id'] for c in cases if c['source_id'] in KEEP}
    for case in cases:
        work = old[case['work_id']]
        require(work['source_id'] == case['source_id'] and work['phase'] == 'verification'
                and work['wbs_task'] == TASK and work['status'] == 'TODO',
                case['source_id'] + ': wrong original source-specific verification duty')
    assignments = copy.deepcopy(before_assignments)
    hunks = {r['child_id']: r for r in rows(baseline['changed-hunks.tsv'])}
    for row in assignments:
        case = reviewed.get(row['hunk_id'])
        if case is None:
            continue
        hunk = hunks[row['hunk_id']]
        section = sections[(row['path'], hunk['index'])]
        result = route(hunk, section['added'], section['removed'])
        require(','.join(result['tests']) == case['verification_tasks_proposed'], row['hunk_id'] + ': wrong routed verification')
        for key in ('profile', 'feature', 'reason'):
            require(result[key] == row[key], row['hunk_id'] + ': changed unrelated ' + key)
        require(','.join(result['tasks']) == row['implementation_tasks'], row['hunk_id'] + ': changed implementation tasks')
        old_tests = row['verification_tasks'].split(','); new_tests = result['tests']
        require([t for t in old_tests if t != TASK] == [t for t in new_tests if t != TASK],
                row['hunk_id'] + ': changed unrelated verification task')
        row['verification_tasks'] = ','.join(new_tests)
    # Preserve audit_legacy_candidates.render's established last-hunk convention,
    # including special cleanup dispositions, without rerunning its full audit.
    old_last = {r['path']: r for r in before_assignments}
    new_last = {r['path']: r for r in assignments}
    candidates = rows(baseline['cleanup-candidate-review.tsv'])
    for row in candidates:
        previous = old_last[row['path']]; current = new_last[row['path']]
        if previous['verification_tasks'] != current['verification_tasks']:
            if row['verification_tasks'] == previous['verification_tasks']:
                row['verification_tasks'] = current['verification_tasks']
            else:
                require(TASK not in row['verification_tasks'].split(','), 'Unreviewed special cleanup routing')
    outputs = {ASSIGN: encode(assignments), 'cleanup-candidate-review.tsv': encode(candidates)}
    expected, checks = specify(outputs[ASSIGN], baseline['asset-wbs.tsv'], catalog)
    wanted = {r['work_id']: r for r in expected}
    require(set(old) - set(wanted) == removed_ids and not set(wanted) - set(old),
            'Work additions/removals differ from exact reviewed set')
    require(len(removed_ids) == 15 and len(retained_ids) == 4 and len(expected) == 22763, 'Work delta count differs')
    for wid in removed_ids:
        require(old[wid]['wbs_task'] == TASK and old[wid]['phase'] == 'verification' and old[wid]['status'] == 'TODO',
                'Removal would discard non-TODO or unrelated work')
    for row in expected:
        previous = old[row['work_id']]
        require(all(row[k] == previous[k] for k in granularity.FIELDS if k not in granularity.MUTABLE),
                row['work_id'] + ': changed retained source/action identity')
        row.update({k: previous[k] for k in granularity.MUTABLE})
        require(row == previous, row['work_id'] + ': changed retained work/evidence')
    validate_work(expected, expected)
    require(sum(r['status'] == 'DONE' for r in expected) == 576, 'Routing changed DONE count')
    other_reviewed = [r for r in before_work if r['source_id'] in reviewed and r['wbs_task'] != TASK]
    require(all(wanted[r['work_id']] == r for r in other_reviewed), 'Changed other reviewed duties')
    outputs[WORK] = assignment.tsv(granularity.FIELDS, expected).encode()
    outputs['granularity-review.tsv'] = assignment.tsv(granularity.AUDIT_FIELDS, checks).encode()
    for name, field, value in [
        ('wbs-assignment-summary.json', 'hunk_ledger_sha256', digest(outputs[ASSIGN])),
        ('granularity-summary.json', 'verification_actions', 6779 - 15),
    ]:
        summary = json.loads(baseline[name]); summary[field] = value
        outputs[name] = (json.dumps(summary, indent=2) + '\n').encode()
    require(all(data != baseline[name] for name, data in outputs.items()), 'Unexpected unchanged output')
    report = dict(baseline=BASE, task=TASK, reviewed_hunks=19, verified_source_paths=15,
                  verified_source_base_blobs=26,
                  removed_todo_work_ids=sorted(removed_ids), retained_gate_work_ids=sorted(retained_ids),
                  removed_todo_count=15, added_work_count=0, retained_work_items=len(expected),
                  preserved_other_reviewed_work_items=len(other_reviewed), preserved_done_work_items=576,
                  done_added_by_routing=0, work_items_after_routing=len(expected),
                  remaining_original_07013_assignments=4,
                  implementation_actions_unchanged=15999, verification_actions_after=6764,
                  full_historical_generator_pass=False,
                  limitation='Only 19 byte-verified hunks from 15 paths were rerouted. Complete historical trees are absent; no full generator PASS, feature equivalence or source-duty completion is claimed.',
                  output_sha256={name: digest(data) for name, data in outputs.items()})
    return outputs, report


def check_completed_output(baseline, outputs, actual):
    """Allow valid execution evidence only on the four exact retained duties."""
    require(set(actual) == set(baseline), 'Inventory file set differs')
    for name, data in dict(baseline, **outputs).items():
        if name != WORK:
            require(actual[name] == data, 'Routing or preserved inventory output differs: ' + name)
            continue
        expected, observed = rows(data), rows(actual[name])
        validate_work(expected, observed)
        require(len(expected) == len(observed), 'Work row count differs')
        for before, after in zip(expected, observed):
            require(before['work_id'] == after['work_id'], 'Work order differs')
            mutable = granularity.MUTABLE if (before['source_id'] in KEEP
                      and before['wbs_task'] == TASK and before['phase'] == 'verification') else ()
            require(all(before[k] == after[k] for k in granularity.FIELDS if k not in mutable),
                    before['work_id'] + ': changed unrelated work or source identity')


def guard_write(baseline, outputs, actual):
    require(set(actual) == set(baseline), 'Refusing later added/removed inventory files')
    for name, data in actual.items():
        require(data in (baseline[name], outputs.get(name, baseline[name])),
                'Refusing to overwrite later inventory work: ' + name)


def reproduce():
    baseline = load_baseline(); cases = load_cases(); sources = load_sources(cases)
    sections, trees = verified_sections(cases, baseline, sources)
    with tempfile.TemporaryDirectory(prefix='painter-bridge-sanitizer-catalog-') as directory:
        path = Path(directory) / 'tasks.md'; path.write_bytes(prior('tasks.md'))
        catalog = assignment.task_catalog(path)
    outputs, report = regenerate(baseline, cases, sections, catalog)
    report['verified_partial_trees'] = trees
    report['cases_sha256'] = digest((HERE / 'routing-cases.json').read_bytes())
    report['routing_policy_sha256'] = digest((ROOT / 'tools/legacy_assignment_rules.py').read_bytes())
    return baseline, outputs, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--output', type=Path, help='Stage expected tables for review')
    mode.add_argument('--check', action='store_true', help='Check routing and valid evidence on only the four retained duties')
    mode.add_argument('--write', action='store_true', help='Apply only to untouched parent or exact routing outputs')
    args = parser.parse_args(); baseline, outputs, report = reproduce()
    if args.write or args.check:
        actual = {str(p.relative_to(INV)): p.read_bytes() for p in INV.rglob('*') if p.is_file()}
        if args.write:
            guard_write(baseline, outputs, actual)
            for name, data in outputs.items():
                (INV / name).write_bytes(data)
        else:
            check_completed_output(baseline, outputs, actual)
    elif args.output:
        require(not args.output.resolve().is_relative_to(INV.resolve()), 'Use guarded --write for the real inventory')
        args.output.mkdir(parents=True, exist_ok=True)
        for name, data in outputs.items():
            (args.output / name).write_bytes(data)
        (args.output / 'routing-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

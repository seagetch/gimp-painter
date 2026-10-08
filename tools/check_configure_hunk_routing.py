#!/usr/bin/env python3
"""Reproduce the bounded configure.ac routing correction for original 04.007.

Only the thirteen reviewed configure.ac payloads are regenerated from archived,
byte-verified source/base blobs. All other rows are compared with the recorded
pre-correction commit. This is not a full historical source-tree generator pass.
"""
import argparse
from collections import Counter
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import tempfile

from assign_legacy_hunks import ROOT, INV, HUNK, sha, tsv, task_catalog
import audit_legacy_granularity as granularity
from legacy_assignment_rules import route

EVIDENCE = ROOT / 'migration/tests/configure-hunk-routing'
REVIEW = INV / 'configure-hunk-routing-review.json'
TABLES = ('changed-hunks.tsv', 'hunk-wbs.tsv', 'cleanup-candidate-review.tsv',
          'auxiliary-script-review.tsv', 'legacy-port-work-items.tsv',
          'granularity-review.tsv')
UNCHANGED = ('changed-files.tsv', 'asset-wbs.tsv', 'auxiliary-generator-relations.tsv')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def parse(data):
    return list(csv.DictReader(io.StringIO(data.decode()), delimiter='\t'))


def old_file(revision, path):
    return subprocess.check_output(['git', 'show', revision+':'+str(path)], cwd=ROOT)


def frozen_tasks(revision, review):
    """Status changes do not change an original task's source action contract."""
    before = old_file(revision, 'tasks.md').decode().splitlines()
    after = (ROOT/'tasks.md').read_text().splitlines()
    def rows(lines):
        result = {}
        for line in lines:
            cells = [c.strip() for c in line.split('|')[1:-1]]
            if len(cells) == 5 and cells[0] in review['task_catalog']:
                result[cells[0]] = [cells[0], *cells[2:]]
        return result
    require(rows(before) == rows(after), 'An original routed WBS row changed')


def verified_sections(review):
    """Reproduce Git's original zero-context grouping without fetching history."""
    with tarfile.open(ROOT/review['verified_blob_archive']) as archive:
        members = archive.getmembers()
        require({m.name for m in members} == {'base/configure.ac', 'source/configure.ac'}
                and len(members) == 2 and all(m.isfile() for m in members),
                'Unexpected configure blob archive members')
        blobs = {m.name: archive.extractfile(m).read() for m in members}
    with tempfile.TemporaryDirectory(prefix='painter-configure-routing-') as directory:
        tmp = Path(directory)
        for role, record in review['verified_blobs'].items():
            data = blobs[role+'/configure.ac']
            blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            require(blob == record['git_blob'] and sha(data) == record['sha256']
                    and len(data) == record['bytes'], 'Unverified '+role+' blob')
            (tmp/role).mkdir()
            (tmp/role/'configure.ac').write_bytes(data)
        result = subprocess.run(['git', 'diff', '--no-index', '--no-ext-diff',
            '--no-textconv', '--no-color', '--unified=0',
            str(tmp/'base/configure.ac'), str(tmp/'source/configure.ac')], capture_output=True)
        require(result.returncode == 1, 'Could not reproduce configure.ac diff')
    sections = []
    for raw in result.stdout.splitlines(keepends=True):
        if raw.startswith(b'@@ '):
            header = raw.decode().rstrip('\n')
            m = HUNK.fullmatch(header)
            require(m is not None, 'Malformed configure.ac diff header')
            old = '-'+m[1]+(','+m[2] if m[2] is not None else '')
            new = m[3]+(','+m[4] if m[4] is not None else '')
            sections.append(dict(header=header, range=old+' -> '+new,
                                 scope=m[5].strip(), payload=b''))
        elif sections and raw[:1] in (b'+', b'-'):
            sections[-1]['payload'] += raw
    require(len(sections) == 13, 'configure.ac hunk count changed')
    return sections


def reproduce():
    review = json.loads(REVIEW.read_text())
    revision = review['baseline_commit']
    prior = json.loads((EVIDENCE/'prior-work-items.json').read_text())
    require(prior['baseline_commit'] == revision, 'Prior snapshot revision changed')
    baseline = {name: old_file(revision, 'migration/inventory/'+name)
                for name in TABLES+UNCHANGED+
                ('changed-hunks.json', 'wbs-assignment-summary.json', 'granularity-summary.json')}
    rows = {name: parse(data) for name, data in baseline.items() if name.endswith('.tsv')}
    mappings = review['mappings']
    ids = {r['hunk_id'] for r in mappings}
    require(ids == {'01.002/'+str(n).zfill(6) for n in range(1888, 1901)}
            and len(mappings) == 13, 'Correction scope changed')
    frozen_tasks(revision, review)
    original_work = rows['legacy-port-work-items.tsv']
    reviewed = [r for r in original_work if r['source_id'] in ids]
    unrelated = [r for r in original_work if r['source_id'] not in ids]
    require(sha(baseline['legacy-port-work-items.tsv']) == prior['previous_ledger_sha256'],
            'Pre-correction ledger digest changed')
    require(reviewed == prior['reviewed_sources'] and len(reviewed) == 91
            and all(r['status'] == 'TODO' for r in reviewed), 'Prior duties changed')
    require(len(unrelated) == prior['unreviewed_count'] == 22899
            and sha(json.dumps(unrelated, sort_keys=True, ensure_ascii=False).encode())
            == prior['unreviewed_sha256'], 'Prior unrelated-work digest changed')
    h_by = {r['child_id']: r for r in rows['changed-hunks.tsv']}
    a_by = {r['hunk_id']: r for r in rows['hunk-wbs.tsv']}
    configure_file = next(r for r in rows['changed-files.tsv'] if r['path'] == 'configure.ac')
    for role in ('source', 'base'):
        require(configure_file[role+'_git_blob'] == review['verified_blobs'][role]['git_blob'],
                'Pinned file inventory blob changed')
    for mapping, section in zip(mappings, verified_sections(review)):
        sid = mapping['hunk_id']
        h, a = h_by[sid], a_by[sid]
        require(h['path'] == a['path'] == 'configure.ac'
                and int(h['index']) == mapping['index'], 'Hunk identity/index changed')
        require(section['header'] == mapping['diff_header']
                and section['range'] == mapping['old_new_lines'] == a['old_new_lines'] == h['old_new_lines']
                and section['scope'] == mapping['source_scope_unchanged_context'] == a['source_scope']
                and section['payload'].decode() == mapping['payload']
                and sha(section['payload']) == mapping['payload_sha256'] == a['payload_sha256'],
                'Source payload/range/context drift: '+sid)
        require(a['source_blob'] == mapping['source_blob'] == configure_file['source_git_blob']
                and a['base_blob'] == mapping['base_blob'] == configure_file['base_git_blob'],
                'Assignment blob changed: '+sid)
        lines = section['payload'].decode().splitlines()
        r = route(h, [l[1:] for l in lines if l.startswith('+')],
                  [l[1:] for l in lines if l.startswith('-')])
        require(r == dict(profile=mapping['proposed_profile'], feature=mapping['feature'],
                          tasks=mapping['implementation_tasks'], tests=mapping['verification_tasks'],
                          reason=mapping['rationale']), 'Exact reviewed route drift: '+sid)
        a.update(profile=r['profile'], feature=r['feature'],
                 implementation_tasks=','.join(r['tasks']), verification_tasks=','.join(r['tests']),
                 reason=r['reason'])
        require(a['implementation_state'] == 'TODO', 'Source assignment is not TODO')
        h.update(feature_or_base=r['feature'], disposition_task=','.join(r['tasks']))
    configure = [a for a in rows['hunk-wbs.tsv'] if a['path'] == 'configure.ac']
    require(len(configure) == 13, 'Extra configure assignments')
    for candidate in rows['cleanup-candidate-review.tsv']:
        if candidate['path'] == 'configure.ac':
            for key in ('implementation_tasks', 'verification_tasks'):
                candidate[key] = ','.join(sorted({t for a in configure for t in a[key].split(',')}))
    for auxiliary in rows['auxiliary-script-review.tsv']:
        if auxiliary['path'] == 'configure.ac':
            a = a_by[auxiliary['hunk_id']]
            auxiliary.update(input_output_contract=a['feature']+'. '+a['reason'],
                implementation_tasks=a['implementation_tasks'], verification_tasks=a['verification_tasks'])
            require(auxiliary['runtime_state'] == 'NOT_PORTED', 'Auxiliary runtime state changed')
    outputs = {name: tsv(tuple(rows[name][0]), rows[name]).encode() for name in TABLES[:4]}
    summary = json.loads(baseline['wbs-assignment-summary.json'])
    summary.update(profiles=dict(sorted(Counter(a['profile'] for a in a_by.values()).items())),
                   hunk_ledger_sha256=sha(outputs['hunk-wbs.tsv']))
    outputs['wbs-assignment-summary.json'] = (json.dumps(summary, indent=2)+'\n').encode()
    changed_summary = json.loads(baseline['changed-hunks.json'])
    changed_summary['inventory_sha256'] = sha(outputs['changed-hunks.tsv'])
    outputs['changed-hunks.json'] = (json.dumps(changed_summary, indent=2)+'\n').encode()
    with tempfile.TemporaryDirectory(prefix='painter-configure-granularity-') as directory:
        tmp = Path(directory)
        (tmp/'hunk-wbs.tsv').write_bytes(outputs['hunk-wbs.tsv'])
        (tmp/'asset-wbs.tsv').write_bytes(baseline['asset-wbs.tsv'])
        expected, checks = granularity.specification(task_catalog(), tmp)
    previous = {r['work_id']: r for r in original_work}
    for row in expected:
        old = previous.get(row['work_id'])
        if old:
            require(all(row[k] == old[k] for k in granularity.FIELDS if k not in granularity.MUTABLE),
                    'An existing source/action contract changed: '+row['work_id'])
            row.update({k: old[k] for k in granularity.MUTABLE})
    current_configure = [r for r in expected if r['source_id'] in ids]
    require(len(current_configure) == 49 and all(r['status'] == 'TODO' for r in current_configure),
            'Expected exactly 49 TODO configure duties')
    require([r for r in expected if r['source_id'] not in ids] == unrelated,
            'An unrelated work row changed')
    for mapping in mappings:
        require([r['work_id'] for r in current_configure if r['source_id'] == mapping['hunk_id']]
                == mapping['corrected_source_work_ids'], 'Reviewed source-work IDs changed')
    outputs['legacy-port-work-items.tsv'] = tsv(granularity.FIELDS, expected).encode()
    outputs['granularity-review.tsv'] = tsv(granularity.AUDIT_FIELDS, checks).encode()
    granularity_summary = json.loads(baseline['granularity-summary.json'])
    granularity_summary.update(implementation_actions=sum(r['phase'] == 'implementation' for r in expected),
                               verification_actions=sum(r['phase'] == 'verification' for r in expected))
    outputs['granularity-summary.json'] = (json.dumps(granularity_summary, indent=2)+'\n').encode()
    for name in UNCHANGED:
        require((INV/name).read_bytes() == baseline[name], 'Unrelated inventory changed: '+name)
    historical = ['tools/check_cpp_registration.py']
    historical += subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', revision,
                    '--', 'migration/tests/cpp-registration'], cwd=ROOT, text=True).splitlines()
    for name in historical:
        require((ROOT/name).read_bytes() == old_file(revision, name), 'Historical evidence changed: '+name)
    evidence = dict(baseline_commit=revision, validation='bounded configure.ac source-ledger correction',
        full_historical_generator_pass=False,
        limitation='At correction time, original source/base commits and their complete trees were unavailable in the shallow checkout; unrelated inventory is preserved from the baseline, not regenerated from those trees.',
        configure_hunks=13, prior_configure_work_items=91, corrected_configure_work_items=49,
        corrected_state='TODO', unrelated_work_items_unchanged=len(unrelated),
        unrelated_work_sha256=prior['unreviewed_sha256'],
        done_work_items_preserved=sum(r['status'] == 'DONE' for r in unrelated),
        unchanged_hunks=2489-13, unchanged_assets=402, changed_paths=947,
        statuses=dict(sorted(Counter(r['status'] for r in expected).items())),
        historical_registration_files_unchanged=historical,
        output_sha256={name: sha(data) for name, data in sorted(outputs.items())})
    return outputs, evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--check', action='store_true', help='Verify the bounded correction (default)')
    mode.add_argument('--regenerate', action='store_true', help='Write only the nine reviewed inventory outputs')
    args = parser.parse_args()
    try:
        outputs, evidence = reproduce()
        # Reject drift before writing any output. In particular, never reset
        # newer execution evidence to the historical checkpoint during a rerun.
        if args.regenerate:
            for name, data in outputs.items():
                before = old_file(evidence['baseline_commit'], 'migration/inventory/'+name)
                require((INV/name).read_bytes() in (before, data),
                        'Refusing to overwrite post-checkpoint changes: '+name)
        for name, data in outputs.items():
            path = INV/name
            if args.regenerate:
                path.write_bytes(data)
            require(path.read_bytes() == data, 'Bounded correction drift: '+name)
        # Run the existing full-ledger structural/action validation without
        # requiring unavailable historical source trees or closing TODO work.
        granularity.render(check=True)
        evidence_text = json.dumps(evidence, ensure_ascii=False, indent=2)+'\n'
        path = EVIDENCE/'evidence.json'
        if args.regenerate:
            path.write_text(evidence_text)
        require(path.read_text() == evidence_text, 'Configure correction evidence drift')
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        raise SystemExit(str(error))
    print('13 configure.ac hunks: 91 blanket duties -> 49 TODO duties; '
          '22,899 unrelated rows and all DONE evidence unchanged; bounded check PASS')


if __name__ == '__main__':
    main()

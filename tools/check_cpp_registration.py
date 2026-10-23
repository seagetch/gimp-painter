#!/usr/bin/env python3
"""Verify original04.002's mixed-language registration, without feature acceptance.

The source review is reproducible from the pinned, byte-verified Makefile inputs.
Build mode consumes a completed default build and its focused Meson test report;
it neither rebuilds GIMP nor runs feature/UI suites.
"""
import argparse
from collections import Counter
import hashlib
import gzip
import json
from pathlib import Path
import re
import shlex
import subprocess
import tarfile
import tempfile

from assign_legacy_hunks import read_tsv
from legacy_assignment_rules import path_profile, route, PROFILES

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / 'migration/tests/cpp-registration'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sha(path):
    return digest(path.read_bytes())


def run(args, cwd=ROOT):
    return subprocess.check_output(args, cwd=cwd, text=True)


RECOVERY = EVIDENCE / 'recovery'
ORIGINAL = 'aa8e7fad108269e2c1dd5ebcbf78865fb37d1ee9'
PRIOR = 'a1912fc6b4357827fc7199a0429992d006a76327'


def check_historical():
    pin = json.loads((RECOVERY/'historical-pin.json').read_text())
    assert pin['original_commit'] == ORIGINAL and pin['prior_commit'] == PRIOR
    archive_path = RECOVERY/'historical-checkpoint.tar.gz'
    assert sha(archive_path) == pin['archive_sha256']
    with tempfile.TemporaryDirectory(prefix='painter-registration-history-') as directory:
        root = Path(directory)
        with tarfile.open(archive_path) as archive:
            members = archive.getmembers()
            assert all(m.isfile() and not Path(m.name).is_absolute() and '..' not in Path(m.name).parts for m in members)
            assert {m.name for m in members} == set(pin['files'])
            for member in members:
                data = archive.extractfile(member).read()
                assert digest(data) == pin['files'][member.name], member.name
                path = root/member.name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                path.chmod(0o444)
        prior = json.loads((root/'migration/tests/cpp-registration/prior-work-items.json').read_text())
        assert prior['baseline_commit'] == PRIOR
        assert sha(root/'prior/legacy-port-work-items.tsv') == prior['previous_ledger_sha256']
        # Execute the exact original checker AND original assignment rules.
        # This preserves its unchanged/unreviewed/TODO-at-checkpoint assertions.
        result = subprocess.run(['python3', '-B', str(root/'tools/check_cpp_registration.py')],
                                cwd=root, capture_output=True, text=True)
        assert result.returncode == 0, result.stdout + result.stderr
        assert 'Pinned47-source registration review' in result.stdout
        for name in ('cpp-registration-review.json',):
            assert (ROOT/'migration/inventory'/name).read_bytes() == (root/'migration/inventory'/name).read_bytes()
        for name in ('prior-work-items.json','legacy-build-inputs.tar.gz','current-build.json',
                     'compile-commands.json.gz','progress-consistency.json','strict-gates.json'):
            assert (EVIDENCE/name).read_bytes() == (root/'migration/tests/cpp-registration'/name).read_bytes(), name


def check_current_lineage(review, work, assignments, inv):
    rows = review['reviews']
    pinned_routing = RECOVERY/'successor-routing-review.json'
    assert pinned_routing.read_bytes() == (inv/'enum-generation-routing-review.json').read_bytes()
    reassignment = json.loads(pinned_routing.read_text())['source_registration_review']
    removed = {r['removed_duplicate_work_id']: r for r in reassignment}
    registered = {r['original_work_id'] for r in rows if r['registration_status']=='VERIFIED_AFTER_CURRENT_BUILD'}
    assert {r['work_id'] for r in work.values() if r['wbs_task']=='04.002'} == registered
    assert all(work[wid]['status']=='DONE' for wid in registered)
    asset_rows = {r['child_id']:r for r in read_tsv(inv/'asset-wbs.tsv')}
    for row in rows:
        live = [w for w in work.values() if w['source_id']==row['source_id']]
        assert live, ('source duty lost',row['source_id'])
        assignment = assignments[row['source_id']] if row['source_kind']=='hunk' else asset_rows[row['source_id']]
        expected = {(phase, task) for phase,key in [('implementation','implementation_tasks'),('verification','verification_tasks')]
                    for task in assignment[key].split(',')}
        assert {(w['phase'],w['wbs_task']) for w in live} == expected, ('current owner coverage',row['source_id'])
        for w in live:
            assert w['path']==row['path'] and w['source_blob']==row['source_blob'] and w['source_sha256']==row['source_sha256']
            assert w['status'] in ('TODO','DONE','IN_PROGRESS','BLOCKED','SKIPPED')
            if w['status']=='DONE':
                assert all(w[k] not in ('','-','pending') for k in ('owner','artifact','test','result')), w['work_id']
        for wid in row['corrected_source_work_ids']:
            if wid in work: continue
            # Four duplicate common-generation duties were explicitly retired,
            # not replaced with a blanket allowlist or silently dropped.
            change = removed[wid]
            assert change['source_id']==row['source_id'] and change['source_blob']==row['source_blob']
            assert change['source_payload_sha256']==row['source_sha256']
            assert {w['wbs_task'] for w in live} == set(change['implementation_tasks']+change['verification_tasks'])
            assert any(w['work_id']==wid and w['wbs_task']=='04.013' and w['status']=='TODO' for w in change['existing_duties'])
            assert change['reason'] == assignment['reason']


def check_review():
    inv = ROOT / 'migration/inventory'
    review = json.loads((inv/'cpp-registration-review.json').read_text())
    rows = review['reviews']
    assert len(rows) == 47 and len({r['original_work_id'] for r in rows}) == 47
    assert Counter(r['registration_status'] for r in rows) == {
        'VERIFIED_AFTER_CURRENT_BUILD': 19, 'REASSIGNED_TODO': 28}
    current = {r['hunk_id']: r for r in read_tsv(inv/'hunk-wbs.tsv')}
    hunk_rows = {r['child_id']: r for r in read_tsv(inv/'changed-hunks.tsv')}
    files = {r['path']: r for r in read_tsv(inv/'changed-files.tsv')}
    work = {r['work_id']: r for r in read_tsv(inv/'legacy-port-work-items.tsv')}
    # Original reassignment invariants are evaluated against their immutable
    # aa8e checkpoint below, not against unrelated later ledger completions.
    check_current_lineage(review, work, current, inv)
    with tarfile.open(EVIDENCE/'legacy-build-inputs.tar.gz') as archive:
        blobs = {m.name: archive.extractfile(m).read() for m in archive.getmembers() if m.isfile()}
    assert len(blobs) == 28  # 15 source files, 13 existing base files
    for key, data in blobs.items():
        role, path = key.split('/', 1)
        blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        assert blob == files[path][role+'_git_blob'], key
    # git's zero-context diff preserves its original hunk grouping and EOF marker.
    with tempfile.TemporaryDirectory(prefix='painter-registration-') as directory:
        tmp = Path(directory)
        for key, data in blobs.items():
            path = tmp/key
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        for path in sorted({r['path'] for r in rows}):
            base, source = tmp/'base'/path, tmp/'source'/path
            result = subprocess.run(['git','diff','--no-index','--unified=0',
                str(base) if base.exists() else '/dev/null',str(source)], capture_output=True)
            assert result.returncode == 1, path
            payloads = []
            for line in result.stdout.splitlines(keepends=True):
                if line.startswith(b'@@ '): payloads.append(b'')
                elif payloads and line[:1] in (b'+',b'-'): payloads[-1] += line
            matching = [r for r in rows if r['path']==path and r['source_kind']=='hunk']
            assert len(payloads)==len(matching)
            for row, payload in zip(matching, payloads):
                assert digest(payload)==row['source_sha256']
                assert payload.decode()==row['payload']
                hunk = hunk_rows[row['source_id']]
                parsed = payload.decode().splitlines()
                actual = route(hunk, [s[1:] for s in parsed if s.startswith('+')],
                               [s[1:] for s in parsed if s.startswith('-')])
                recorded = current[row['source_id']]
                for key, value in [('profile',actual['profile']),('feature',actual['feature']),
                    ('implementation_tasks',','.join(actual['tasks'])),
                    ('verification_tasks',','.join(actual['tests'])),('reason',actual['reason'])]:
                    assert recorded[key]==value, (row['source_id'],key)
    asset = next(r for r in read_tsv(inv/'asset-wbs.tsv') if r['path']=='menus/Makefile.am')
    profile = PROFILES[path_profile(asset['path'])]
    assert asset['implementation_tasks']==','.join(profile['tasks'])
    assert asset['verification_tasks']==','.join(profile['tests'])
    assert asset['preservation']==profile['reason']
    # Auxiliary inventories recorded file-wide unions at the original checkpoint;
    # later reviewed rows may be hunk-scoped. Their original invariance is tested
    # in check_historical, while current obligations are checked against the
    # canonical assignment/work ledgers above (no future-TODO requirement).
    for row in rows:
        for path in row['current_paths']:
            assert (ROOT/path).is_file(), path
    return review


def build_report(review, build, foundation_log, build_log):
    database = json.loads((build/'compile_commands.json').read_text())
    records = []
    counts = Counter()
    selected = {p for row in review['reviews'] if row['registration_status']=='VERIFIED_AFTER_CURRENT_BUILD'
                for p in row['current_paths'] if Path(p).suffix in ('.c','.cc','.cpp')}
    common = json.loads((ROOT/'migration/tests/painter-foundation-acceptance-native.json').read_text())
    selected.update(common['compiled_sources'])
    selected.add('app/main.c')
    covered = set()
    archives = {}
    for entry in database:
        source = (Path(entry['directory'])/entry['file']).resolve()
        args = entry.get('arguments') or shlex.split(entry['command'])
        suffix = source.suffix
        if suffix not in ('.c','.cc','.cpp'): continue
        cpp = suffix != '.c'
        assert Path(args[0]).name == ('c++' if cpp else 'cc'), entry['file']
        explicit = [args[i+1] for i,a in enumerate(args[:-1]) if a=='-x']
        assert all(value==('c++' if cpp else 'c') for value in explicit)
        assert not any(a.startswith('-x') and a not in ('-x','-xc++' if cpp else '-xc') for a in args)
        if cpp: assert '-std=c++14' in args, entry['file']
        else: assert not any('c++' in a for a in args if a.startswith('-std=') or a.startswith('-x'))
        counts['cpp' if cpp else 'c'] += 1
        try: relative = source.relative_to(ROOT).as_posix()
        except ValueError: continue
        if relative not in selected: continue
        covered.add(relative)
        output = build / (entry.get('output') or args[args.index('-o')+1])
        assert output.is_file(), output
        # These objects were rebuilt or accepted as up-to-date by the completed
        # default Ninja build, not fabricated through a standalone compiler probe.
        record = dict(source=relative, language='cpp' if cpp else 'c',
            compiler=args[0], standard='c++14' if cpp else 'compiler-default-C',
            source_sha256=sha(source), object=output.relative_to(build).as_posix(),
            object_sha256=sha(output), command=args, command_sha256=digest('\0'.join(args).encode()))
        if '.a.p/' in output.as_posix():
            archive = Path(output.as_posix().split('.a.p/',1)[0]+'.a')
            name = archive.relative_to(build).as_posix()
            if name not in archives:
                archives[name] = dict(sha256=sha(archive), members=run(['ar','t',str(archive)]).splitlines())
            assert output.name in {Path(member).name for member in archives[name]['members']}, output
            record['archive'] = name
        records.append(record)
    assert covered == selected, sorted(selected-covered)
    raw_commands = [dict(source=r['source'],object=r['object'],arguments=r.pop('command')) for r in records]
    command_file = EVIDENCE/'compile-commands.json.gz'
    command_file.write_bytes(gzip.compress(json.dumps(raw_commands,indent=2).encode()+b'\n',mtime=0))
    links=[]
    targets=('app/gimp-3.0','app/gimp-console-3.0','app/painter/painter-foundation')
    commands = run(['ninja','-C',str(build),'-t','commands',*targets]).splitlines()
    for target in targets:
        choices=[shlex.split(line) for line in commands]
        choices=[a for a in choices if '-o' in a and a[a.index('-o')+1]==target]
        assert len(choices)==1, target
        args=choices[0]
        assert args[0]=='c++' and 'app/painter/libapppainter.a' in args
        binary=build/target
        needed=run(['readelf','-d',str(binary)])
        assert 'libstdc++.so.6' in needed
        symbols=run(['nm','-C',str(binary)])
        expected=['main','painter_test_c_callback','painter_test_cpp_roundtrip'] if target.endswith('painter-foundation') else ['gimp_painter_binding_close']
        assert all(re.search(r' [Tt] '+re.escape(s)+r'$',symbols,re.M) for s in expected)
        links.append(dict(target=target,sha256=sha(binary),command=args,
                          required_symbols=expected,cxx_runtime='libstdc++.so.6'))
    meson=[json.loads(line) for line in foundation_log.read_text().splitlines()]
    assert len(meson)==1 and meson[0]['returncode']==0 and meson[0]['result']=='OK'
    test={k:meson[0][k] for k in ('name','stdout','result','returncode','duration','command')}
    assert len(re.findall(r'^ok \d+ ',test['stdout'],re.M))==34
    sources=set(selected)
    sources.update(common['source_sha256'])
    sources.update(['meson.build','meson_options.txt','tools/check_cpp_registration.py'])
    for row in review['reviews']: sources.update(row['current_paths'])
    for path in (ROOT/'app').rglob('meson.build'): sources.add(path.relative_to(ROOT).as_posix())
    source_hashes={p:sha(ROOT/p) for p in sorted(sources)}
    retained={}
    for name in ('painter-foundation-acceptance-native.json','painter-foundation-acceptance-sanitizers.json','painter-integration.json'):
        old=json.loads((ROOT/'migration/tests'/name).read_text())
        drift=[p for p,h in old['source_sha256'].items() if sha(ROOT/p)!=h]
        retained[name]=dict(report_sha256=sha(ROOT/'migration/tests'/name),changed_sources=drift,
                           interpretation='historical; not current acceptance' if drift else 'all recorded source hashes match; retained historical run')
    return dict(status='PASS',scope='Original04.002 mixed C/C++ Meson registration only',
        baseline_commit=run(['git','rev-parse','HEAD']).strip(), platform='Linux x86_64 / pinned Debian13 dependencies',
        source_sha256=source_hashes,compile_database_sha256=sha(build/'compile_commands.json'),
        language_counts=dict(counts),registration_duties=19,registered_source_count=len(selected),
        compiled_entries=records,compile_commands_file=command_file.relative_to(ROOT).as_posix(),
        compile_commands_file_sha256=sha(command_file),archives=archives,final_links=links,foundation=test,
        default_build=dict(command=['meson','compile','-C',str(build.relative_to(ROOT)),'-j','4'],
                           result='PASS',log_sha256=sha(build_log),output=build_log.read_text()),
        retained_reports=retained,limitations=['No complete feature behavior, UI, old-file acceptance or full04 gate',
             'Windows/macOS and later platform tasks remain unverified here',
             'No new ASan/UBSan/LSan claim; source-matching retained common foundation reports are identified separately',
             'Existing painter/clone acceptance validators remain strict and currently fail on source drift'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path)
    parser.add_argument('--foundation-log',type=Path)
    parser.add_argument('--build-log',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    check_historical()
    review=check_review()
    if args.build:
        assert args.foundation_log and args.build_log and args.output
        report=build_report(review,args.build.resolve(),args.foundation_log,args.build_log)
        args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        print('Current mixed-language registration,19 duties, C-main34 cases and3 C++ final links: PASS')
    else:
        print('Historical04.002 invariants pinned; current47-source ownership and assignment regeneration: PASS (no fresh build claim)')


if __name__=='__main__':
    main()

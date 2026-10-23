#!/usr/bin/env python3
"""Capture genuine pinned Sharpen PDB, FilterLayer and old-writer XCF evidence.

Only observation hooks are linked into a private harness. The original plug-in,
merge implementation and writer stay unchanged. Source the replay env.sh before
--capture. Without --capture this only prepares the finite case header/plan.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import fcntl
import gzip
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile

import capture_legacy_filter_context as context

ROOT = Path(__file__).resolve().parents[1]
COMMIT = context.COMMIT
SOURCES = tuple(name for name in context.SOURCES if not name.endswith('/blinds.c')) + (
    'plug-ins/common/sharpen.c', 'app/xcf/xcf-save.c', 'app/xcf/xcf-write.c',
    'app/core/gimpparamspecs.c', 'app/plug-in/gimppluginprocedure.c',
    'app/plug-in/gimppluginmanager-call.c', 'libgimp/gimppixelrgn.c', 'libgimp/gimpdrawable.c')
sha = context.sha
COLUMNS = ('id live source_channels output_channels width height image_width image_height '
           'offset_x offset_y percent start_selection final_selection start_components '
           'final_components start_lock final_lock rerun writer starts ends merges status mutated').split()


def cases(large=False):
    result = []
    def add(c, p=50, w=17, h=13, live=0, s=0, fs=-1, bits=None, fb=-1,
            lock=0, fl=-1, zero=0, rerun=0, writer=0, ox=3, oy=2, suffix=''):
        out_c = (2 if c <= 2 else 4) if live else c
        if bits is None: bits = (1 << out_c) - 1
        ident = f"{'live' if live else 'pdb'}-c{c}-{w}x{h}-p{p}-s{s}-f{fs}-a{bits}-l{lock}{suffix}"
        result.append(dict(id=ident, width=w, height=h, components=c, image_width=w+8,
            image_height=h+8, offset_x=ox, offset_y=oy, percent=p, live=live,
            selection=s, final_selection=fs, active_components=bits,
            final_components=fb, lock_alpha=lock, final_lock=fl,
            zero_alpha=zero, rerun=rerun, writer=writer))
    if large:
        add(2, w=1031, h=1021, live=1, s=1, fs=0, rerun=1, suffix='-spill')
        add(4, w=1031, h=1021, live=1, s=1, fs=2, rerun=1, suffix='-spill')
        return result
    # The old defined domain only: no width 1, height <3, or overflowing 100-p.
    for c in range(1, 5):
        for p in (-2147483547, -100, -1, 0, 1, 10, 50, 98, 99, 100, 2147483647): add(c, p)
        for w, h in ((2, 3), (2, 9), (3, 3), (65, 67)): add(c, w=w, h=h)
        for s in (1, 2, 3, 4): add(c, s=s)
        for bits in range(1 << c): add(c, s=2, bits=bits, suffix='-components')
        if c % 2 == 0:
            add(c, zero=1, suffix='-hidden')
            add(c, s=2, lock=1)
        add(c, ox=-2, oy=-1, suffix='-negative-offset')
        for p in (10, 0, -100, 99, 2147483647):
            add(c, p, live=1, rerun=1, writer=1)
        for s in (1, 2, 3, 4): add(c, live=1, s=s)
        for fs in (0, 2, 4): add(c, w=53, h=41, live=1, s=1, fs=fs, rerun=1)
        out_c = 2 if c <= 2 else 4
        for bits in (0, 1, 1 << (out_c - 1)):
            add(c, live=1, s=2, bits=bits, suffix='-components')
        add(c, live=1, s=2, lock=1)
        add(c, live=1, s=1, fb=1, rerun=1, suffix='-late-components')
        add(c, live=1, s=1, fl=1, rerun=1, suffix='-late-lock')
    assert len({c['id'] for c in result}) == len(result)
    return result


def write_header(work, tests):
    fields = ('width height components image_width image_height offset_x offset_y percent live '
              'selection final_selection active_components final_components lock_alpha final_lock '
              'zero_alpha rerun writer').split()
    lines = ['typedef struct { const char *id; gint ' + ', '.join(fields) + '; } CaptureCase;',
             'static const CaptureCase capture_cases[] = {']
    for case in tests:
        lines.append('  {' + ', '.join([json.dumps(case['id'])] + [str(case[k]) for k in fields]) + '},')
    lines.append('};')
    (work/'legacy-sharpen-cases.h').write_text('\n'.join(lines)+'\n')


def build(source, work, out):
    cwd = source/'app/tests'
    dry = subprocess.check_output(['make', '-n', '-W', 'test-xcf.c', 'test-xcf'], cwd=cwd, text=True)
    compile_line = next(line for line in dry.splitlines() if ' -c -o test-xcf.o ' in line)
    flags = shlex.split(compile_line[compile_line.index('/usr/bin/gcc '):].split(' -MT ')[0])[1:]
    includes = [flag for flag in flags if flag.startswith(('-I', '-D')) or flag == '-pthread']
    includes += ['-I'+str(source/'app/core'), '-I'+str(source/'app/pdb'), '-I'+str(work)]
    paths = context.instrument(source, work, out)
    commands, objects = [], []
    for path in [ROOT/'migration/tests/legacy-sharpen-capture.c'] + paths:
        output = work/(path.stem+'.o')
        cmd = (['g++', '-std=c++17', '-include', 'type_traits'] if path.suffix == '.cpp' else ['gcc']) + includes
        cmd += ['-g', '-O1', '-fcommon', '-c', str(path), '-o', str(output)]
        commands.append(cmd); objects.append(str(output))
    link_line = next(line for line in dry.splitlines() if '--mode=link ' in line)
    cmd = shlex.split(link_line[link_line.index('/bin/bash '):])
    index = cmd.index('-o'); executable = work/'legacy-sharpen'
    cmd[index:index+3] = ['-o', str(executable)] + objects
    cmd.insert(cmd.index('../../app/core/libappcore.a'), '../../app/presets/libapppresets.a')
    cmd += ['-lstdc++']; commands.append(cmd)
    with (out/'build.log').open('w') as log:
        for cmd in commands:
            log.write(shlex.join(cmd)+'\n'); log.flush()
            subprocess.run(cmd, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
    return executable, commands


def run(executable, prefix, out, timeout):
    with tempfile.TemporaryDirectory(prefix='gimp-old-sharpen-profile-') as tmp:
        home = Path(tmp)
        for name in ('profile', 'plugins', 'empty', 'temp', 'cache', 'config', 'data'):
            (home/name).mkdir(mode=0o700)
        (home/'plugins/sharpen').symlink_to(prefix/'lib/gimp/2.0/plug-ins/sharpen')
        (home/'profile/gimprc').write_text(''.join(f'({name} {json.dumps(str(home/path))})\n' for name,path in (
            ('plug-in-path', 'plugins'), ('module-path', 'empty'), ('interpreter-path', 'empty'),
            ('environ-path', 'empty'), ('temp-path', 'temp'), ('swap-path', 'temp'))))
        env = dict(os.environ)
        for key in tuple(env):
            if key.startswith('GIMP_TESTING_') or key in ('GIMP_PLUGIN_DEBUG', 'GIMP_PLUGIN_DEBUG_WRAP'): del env[key]
        env.update(HOME=str(home), GIMP2_DIRECTORY=str(home/'profile'), XDG_CONFIG_HOME=str(home/'config'),
                   XDG_CACHE_HOME=str(home/'cache'), XDG_DATA_HOME=str(home/'data'))
        command = [str(executable), str(out)]
        with (out/'capture.log').open('w') as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        return result.returncode, command


def writer_shape(path):
    """Inspect bytes actually emitted by the writer, never generate a capsule."""
    data = path.read_bytes(); literal = b'plug-in-sharpen\0'
    needle = struct.pack('>I', len(literal)) + literal
    start = data.find(needle)
    if start < 8 or data.find(needle, start+1) >= 0: raise ValueError('Nonunique writer filter name')
    outer_tag, outer_size = struct.unpack_from('>II', data, start-8)
    if outer_tag != 32: raise ValueError('Not an old filter property')
    end = start + outer_size
    if end > len(data): raise ValueError('Truncated writer capsule')
    pos = start + len(needle); records = []
    while pos < end:
        tag, size = struct.unpack_from('>II', data, pos); pos += 8
        # Writer-only decoding here deliberately records the observed record,
        # including opaque types. Fixture validity requires declared lengths.
        payload = data[pos:pos+size]
        if len(payload) != size or pos+size > end: raise ValueError('Truncated writer argument')
        records.append(dict(tag=tag, declared_size=size, payload_hex=payload.hex()))
        pos += size
        if tag == 0: break
    if pos != end or not records or records[-1]['tag'] != 0:
        raise ValueError('Writer terminator/length disagreement')
    capsule = data[start:end]
    path.with_suffix('.definition.raw').write_bytes(capsule)
    return dict(file=path.name, property_offset=start-8, payload_offset=start, payload_size=outer_size,
                procedure_bytes_hex=literal.hex(), records=records, capsule_sha256=hashlib.sha256(capsule).hexdigest())


def validate(out, tests, code):
    errors = []; records = []; writers = []
    if code: errors.append('Harness exit '+str(code))
    log = (out/'capture.log').read_text(errors='replace')
    if 'LEGACY_SHARPEN_CAPTURE_COMPLETE' not in log: errors.append('Missing successful completion marker')
    rows = {}
    if (out/'captures.tsv').exists():
        for line in (out/'captures.tsv').read_text().splitlines():
            values = line.split('\t')
            if len(values) != len(COLUMNS): errors.append('Invalid capture row '+line); continue
            if values[0] in rows: errors.append('Duplicate capture id '+values[0])
            rows[values[0]] = dict(zip(COLUMNS, [values[0]] + list(map(int, values[1:]))))
    for test in tests:
        ident = test['id']; row = rows.get(ident)
        if row is None: errors.append('Missing completed case '+ident); continue
        record = dict(test, observed=row, buffers={})
        expected_runs = (1 + test['rerun']) if test['live'] else 0
        expected_merges = 0 if test['selection'] == 3 else 1 + test['rerun']
        if (row['starts'], row['ends']) != (expected_runs, expected_runs): errors.append('Wrong live run counts '+ident)
        if row['merges'] != expected_merges: errors.append('Wrong merge count '+ident)
        if row['status'] != (-1 if test['live'] else 3): errors.append('Wrong observed/sentinel status '+ident)
        if row['mutated'] != int(any(test[k] >= 0 for k in ('final_selection','final_components','final_lock'))):
            errors.append('Missing deterministic late mutation '+ident)
        for path in sorted(out.glob(ident+'-*.raw')):
            label = path.name[len(ident)+1:-4]
            # IDs are prefix-free only by the trailing label: exact expected
            # buffer labels prevent similarly named cases leaking into records.
            allowed = {'source','input','output','start-mask','final-mask','source2','output2',
                       'start1','start2','start1-mask','start2-mask'}
            allowed.update(f'merge{n}-{kind}' for n in (1,2) for kind in ('input','shadow','mask','output'))
            if label not in allowed: continue
            channels = test['components'] if label in ('source','source2') else row['output_channels']
            size = test['width'] * test['height'] * channels
            if label.endswith('mask'):
                size = (test['width']*test['height'] if label.startswith('merge') else test['image_width']*test['image_height'])
            if path.stat().st_size != size: errors.append('Wrong buffer size '+path.name)
            record['buffers'][label] = dict(file=path.name, bytes=path.stat().st_size, sha256=sha(path))
        required = ['source','input','output','start-mask','final-mask']
        if test['live']: required += ['start1','start1-mask']
        if test['rerun']: required += ['source2','output2','start2','start2-mask']
        for n in range(1, expected_merges+1): required += [f'merge{n}-{kind}' for kind in ('input','shadow','mask','output')]
        for label in required:
            if label not in record['buffers']: errors.append('Missing buffer '+ident+' '+label)
        if test['writer']:
            try:
                shape = writer_shape(out/(ident+'.xcf')); writers.append(shape)
                if len(shape['records']) != 5: errors.append('Writer did not emit four args plus terminator '+ident)
                record['writer'] = shape
            except (OSError, ValueError, struct.error) as exc: errors.append('Writer fixture '+ident+': '+str(exc))
        records.append(record)
    (out/'fixtures.json').write_text(json.dumps(dict(schema_version=1,capture_columns=COLUMNS,cases=records,
        selection_kinds={'0':'empty/no selection','1':'center hard','2':'soft','3':'outside drawable','4':'expanded hard'},
        layout_policy='PDB observes Y/YA/RGB/RGBA; genuine FilterLayer always adds alpha to Y/RGB lower input.',
        status_policy='Synchronous PDB status is observed; async FilterLayer status -1 means unavailable, with completion/merge evidence recorded separately.',
        mask_policy='start/final masks are image-sized native selection bytes; merge masks are drawable-local effective coverage.'),indent=2)+'\n')
    (out/'writer-shapes.json').write_text(json.dumps(writers,indent=2)+'\n')
    return errors, len(records)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--case-pattern', default='')
    parser.add_argument('--large', action='store_true', help='Capture only the two genuine spill-size FilterLayer cases')
    parser.add_argument('--output', type=Path, default=ROOT/'migration/fixtures/legacy-sharpen-context')
    parser.add_argument('--legacy-source', type=Path, required=True)
    parser.add_argument('--legacy-prefix', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    parser.add_argument('--timeout', type=float, default=300)
    args = parser.parse_args(); out=args.output.resolve(); work=args.work.resolve(); source=args.legacy_source.resolve()
    if (out/'capture-report.json').exists(): raise SystemExit('Sealed evidence already exists; choose new --output')
    if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip() != COMMIT:
        raise SystemExit('Wrong legacy source commit')
    hashes = {}
    for name in SOURCES:
        pinned = subprocess.check_output(['git','-C',str(source),'show',COMMIT+':'+name])
        hashes[name] = sha(source/name)
        if hashlib.sha256(pinned).hexdigest() != hashes[name]: raise SystemExit('Changed oracle source '+name)
    out.mkdir(parents=True,exist_ok=True); work.mkdir(parents=True,exist_ok=True)
    tests = [c for c in cases(args.large) if args.case_pattern in c['id']]
    if not tests: raise SystemExit('No matching cases')
    write_header(work, tests)
    (out/'capture-plan.json').write_text(json.dumps(tests,indent=2)+'\n')
    (out/'.gitattributes').write_text('*.raw binary\n*.xcf binary\n')
    if not args.capture: print('Prepared',len(tests),'genuine old PDB/FilterLayer cases; no execution'); return 0
    shutil.copyfile(Path(__file__), out/'capture-script.py')
    shutil.copyfile(ROOT/'migration/tests/legacy-sharpen-capture.c', out/'capture-harness.c')
    shutil.copyfile(Path(context.__file__), out/'observation-helper.py')
    shutil.copyfile(work/'legacy-sharpen-cases.h', out/'capture-cases.h')
    started = datetime.now(timezone.utc).isoformat()
    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        executable, commands = build(source,work,out)
        binaries = [work/'.libs/legacy-sharpen',args.legacy_prefix/'bin/gimp-2.8',
                    args.legacy_prefix/'lib/gimp/2.0/plug-ins/sharpen']
        binary_hashes = {str(path):sha(path) for path in binaries}
        code, command = run(executable,args.legacy_prefix,out,args.timeout)
    errors,count = validate(out,tests,code)
    diagnostics = [line for line in (out/'capture.log').read_text(errors='replace').splitlines()
                   if any(word in line.lower() for word in ('critical', 'warning', 'error', 'assertion', 'failed'))]
    post_hashes = {str(path):sha(path) for path in binaries}
    post_sources = {name:sha(source/name) for name in SOURCES}
    if post_hashes != binary_hashes or post_sources != hashes: errors.append('Oracle source or executable changed during capture')
    for filename in ('build.log','capture.log'):
        path=out/filename
        (out/(filename+'.gz')).write_bytes(gzip.compress(path.read_bytes(),mtime=0)); path.unlink()
    report=dict(schema_version=1,status='passed' if not errors else 'failed',source_commit=COMMIT,
        started_utc=started,finished_utc=datetime.now(timezone.utc).isoformat(),source_sha256=hashes,
        executable_sha256=binary_hashes,build_commands=commands,command=command,exit_code=code,
        case_count=count,expected_cases=len(tests),large_cases=args.large,errors=errors,
        capture_script_sha256=sha(Path(__file__)),harness_sha256=sha(ROOT/'migration/tests/legacy-sharpen-capture.c'),
        instrumentation_sha256=sha(out/'observation-hooks.patch'),
        observation_helper_sha256=sha(out/'observation-helper.py'),
        post_capture_source_sha256=post_sources,post_capture_executable_sha256=post_hashes,
        runtime_diagnostics=diagnostics,
        profile_policy='Fresh isolated profile with only the pinned original Sharpen plug-in',
        files_sha256={p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()})
    (out/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],count,errors); return 0 if not errors else 1

if __name__ == '__main__': raise SystemExit(main())

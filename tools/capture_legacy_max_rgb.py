#!/usr/bin/env python3
"""Capture genuine pinned Max RGB PDB, FilterLayer and old-writer XCF evidence.

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
    'plug-ins/common/max-rgb.c', 'libgimp/gimpregioniterator.c', 'libgimp/gimpprogress.c',
    'app/core/gimpchannel.c', 'app/pdb/drawable-cmds.c', 'libgimp/gimpdrawable_pdb.c', 'app/base/tile-manager.c', 'app/plug-in/gimpplugin-cleanup.c', 'app/xcf/xcf-save.c', 'app/xcf/xcf-write.c',
    'app/core/gimpparamspecs.c', 'app/plug-in/gimppluginprocedure.c',
    'app/plug-in/gimppluginmanager-call.c', 'libgimp/gimppixelrgn.c', 'libgimp/gimpdrawable.c')
sha = context.sha
COLUMNS = ('id live source_channels output_channels width height image_width image_height '
           'offset_x offset_y mode start_selection final_selection start_components '
           'final_components start_lock final_lock rerun writer starts ends merges status mutated '
           'phase idle_selection idle_components idle_lock idle_starts idle_ends').split()


def cases(large=False):
    """Finite synthetic oracle matrix. Phase: 0 none, 1 actual merge entry, 2 observed mid-work."""
    result = []
    def add(c, t=1, w=17, h=13, live=0, s=0, fs=-1, bits=None, fb=-1,
            lock=0, fl=-1, zero=0, rerun=0, writer=0, ox=3, oy=2, phase=0,
            idle_s=-1, idle_b=-1, idle_l=-1, suffix=''):
        out_c = 4 if live else c
        if bits is None: bits = (1 << out_c) - 1
        ident = f"{'live' if live else 'pdb'}-c{c}-{w}x{h}-m{t}-s{s}-f{fs}-a{bits}-l{lock}{suffix}"
        result.append(dict(id=ident, width=w, height=h, components=c, image_width=w+8,
            image_height=h+8, offset_x=ox, offset_y=oy, mode=t, live=live,
            selection=s, final_selection=fs, active_components=bits,
            final_components=fb, lock_alpha=lock, final_lock=fl,
            zero_alpha=zero, rerun=rerun, writer=writer, phase=phase,
            idle_selection=idle_s, idle_components=idle_b, idle_lock=idle_l))
    if large:
        for c, fs in ((3, 0), (4, 2)):
            add(c, 1 if c == 3 else -2, w=1031, h=1021, live=1, s=1, fs=fs, phase=2,
                rerun=2, suffix='-spill-unchanged')
        return result
    boundaries = (-2147483648, -2, -1, 0, 1, 3, 2147483647)
    for c in (3, 4):
        for t in boundaries: add(c, t)
        for w, h in ((1, 1), (1, 7), (7, 1), (2, 2), (67, 66)):
            for t in (0, 1): add(c, t, w=w, h=h)
        for s in (1, 2, 3, 4): add(c, s=s)
        for bits in range(1 << c): add(c, s=2, bits=bits, suffix='-components')
        if c == 4:
            for t in (-1, 0, 1, 3): add(c, t, zero=1, suffix='-hidden')
        if c == 4:
            for t in (-1, 0, 1, 3): add(c, t, s=2, lock=1)
        add(c, ox=-2, oy=-1, s=4, suffix='-negative-offset')
        add(c, ox=5, oy=-2, s=4, suffix='-asymmetric-offset')
    for c in (3, 4):
        for t in boundaries:
            add(c, t, live=1, rerun=1, writer=1)
        for w, h in ((1, 1), (1, 7), (7, 1), (2, 2), (67, 66)):
            add(c, w=w, h=h, live=1, suffix='-tiny')
        for s in (1, 2, 3, 4): add(c, live=1, s=s)
        out_c = 4
        for bits in range(1 << out_c):
            add(c, live=1, s=2, bits=bits, suffix='-components')
        add(c, live=1, s=2, lock=1)
        add(c, live=1, ox=-2, oy=-1, s=4, suffix='-negative-offset')
        # Real phase requests, never inferred from image size or a missing hook.
        for phase, w, h, s, label in ((1, 53, 41, 1, 'merge-entry'),
                                      (2, 133, 129, 5, 'mid-work')):
            for fs in (0, 2, 3, 4):
                add(c, w=w, h=h, live=1, s=s, fs=fs, phase=phase,
                    rerun=2, suffix='-'+label+'-unchanged')
            add(c, w=w, h=h, live=1, s=s, fb=1, phase=phase,
                rerun=2, suffix='-'+label+'-late-components')
            add(c, w=w, h=h, live=1, s=s, fl=1, phase=phase,
                rerun=2, suffix='-'+label+'-late-lock')
        add(c, w=17, h=13, live=1, s=7, fs=0, phase=1,
            suffix='-one-pixel-merge-entry')
        add(c, w=67, h=66, live=1, s=6, fs=0, phase=2,
            suffix='-two-pixel-crossing')
        # Context-only edits must leave the completed cache untouched; the
        # following lower notification has byte-identical input and is distinct
        # from the changed-source writer rerun above.
        for field, value in (('idle_s', 2), ('idle_b', 1), ('idle_l', 1)):
            add(c, live=1, s=1, rerun=2, suffix='-'+field+'-unchanged', **{field:value})
    # Signed minimum mode also receives soft-mask/component/alpha-lock and
    # phase-controlled coverage; positive-only context would hide mode mistakes.
    for c in (3, 4):
        for live in (0, 1):
            for s in (1, 2, 3, 4): add(c, 0, live=live, s=s, suffix='-minimum-context')
            for bits in (0, 1, 5, 7): add(c, -2, live=live, s=2, bits=bits, suffix='-minimum-components')
        add(c, -2147483648, live=1, s=2, lock=1, suffix='-minimum-lock')
        add(c, 0, w=133, h=129, live=1, s=5, fs=2, phase=2, rerun=2,
            suffix='-minimum-mid-work')
        add(c, -2, w=53, h=41, live=1, s=1, fs=0, phase=1, rerun=2,
            suffix='-minimum-merge-entry')
    assert all(c['components'] in (3, 4) for c in result)
    assert len({c['id'] for c in result}) == len(result)
    return result


def write_header(work, tests):
    fields = ('width height components image_width image_height offset_x offset_y mode live '
              'selection final_selection active_components final_components lock_alpha final_lock '
              'zero_alpha rerun writer phase idle_selection idle_components idle_lock').split()
    lines = ['typedef struct { const char *id; gint ' + ', '.join(fields) + '; } CaptureCase;',
             'static const CaptureCase capture_cases[] = {']
    for case in tests:
        lines.append('  {' + ', '.join([json.dumps(case['id'])] + [str(case[k]) for k in fields]) + '},')
    lines.append('};')
    (work/'legacy-max-rgb-cases.h').write_text('\n'.join(lines)+'\n')


def build(source, work, out):
    cwd = source/'app/tests'
    dry = subprocess.check_output(['make', '-n', '-W', 'test-xcf.c', 'test-xcf'], cwd=cwd, text=True)
    compile_line = next(line for line in dry.splitlines() if ' -c -o test-xcf.o ' in line)
    flags = shlex.split(compile_line[compile_line.index('/usr/bin/gcc '):].split(' -MT ')[0])[1:]
    includes = [flag for flag in flags if flag.startswith(('-I', '-D')) or flag == '-pthread']
    includes += ['-I'+str(source/'app/core'), '-I'+str(source/'app/pdb'), '-I'+str(work)]
    paths = context.instrument(source, work, out)
    commands, objects = [], []
    for path in [out/'capture-harness.c'] + paths:
        output = work/(path.stem+'.o')
        cmd = (['g++', '-std=c++17', '-include', 'type_traits'] if path.suffix == '.cpp' else ['gcc']) + includes
        cmd += ['-g', '-O1', '-fcommon', '-c', str(path), '-o', str(output)]
        commands.append(cmd); objects.append(str(output))
    link_line = next(line for line in dry.splitlines() if '--mode=link ' in line)
    cmd = shlex.split(link_line[link_line.index('/bin/bash '):])
    index = cmd.index('-o'); executable = work/'legacy-max-rgb'
    cmd[index:index+3] = ['-o', str(executable)] + objects
    cmd.insert(cmd.index('../../app/core/libappcore.a'), '../../app/presets/libapppresets.a')
    cmd += ['-lstdc++']; commands.append(cmd)
    with (out/'build.log').open('w') as log:
        for cmd in commands:
            log.write(shlex.join(cmd)+'\n'); log.flush()
            subprocess.run(cmd, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
    return executable, commands


def run(executable, prefix, out, timeout):
    # The isolated profile is a different directory: symlink targets must be absolute.
    prefix = Path(prefix).resolve()
    with tempfile.TemporaryDirectory(prefix='gimp-old-max-rgb-profile-') as tmp:
        home = Path(tmp)
        for name in ('profile', 'plugins', 'empty', 'temp', 'cache', 'config', 'data'):
            (home/name).mkdir(mode=0o700)
        (home/'plugins/max-rgb').symlink_to(prefix/'lib/gimp/2.0/plug-ins/max-rgb')
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
            try:
                result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
                code = result.returncode
            except subprocess.TimeoutExpired:
                log.write('\nCAPTURE_TIMEOUT_SECONDS='+str(timeout)+'\n'); code = 124
        return code, command


def writer_shape(path):
    """Inspect bytes actually emitted by the writer, never generate a capsule."""
    data = path.read_bytes(); literal = b'plug-in-max-rgb\0'
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
    # Import locally so preparing the matrix never reads archive pins. This
    # validator checks captured observations; it does not produce oracle pixels.
    from check_max_rgb_evidence import validate_directory
    return validate_directory(out, tests, code)



def verify_sources(source):
    """Reject changed oracle inputs before preparing files or building anything."""
    if subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip() != COMMIT:
        raise ValueError('Wrong legacy source commit')
    hashes = {}
    for name in SOURCES:
        pinned = subprocess.check_output(['git','-C',str(source),'show',COMMIT+':'+name])
        hashes[name] = sha(source/name)
        if hashlib.sha256(pinned).hexdigest() != hashes[name]:
            raise ValueError('Changed oracle source '+name)
    return hashes


def validate_destinations(source, out, work):
    if out == work or out in work.parents or work in out.parents:
        raise ValueError('Capture output and work must be independent destinations')
    if source == out or source == work or source in out.parents or source in work.parents:
        raise ValueError('Capture destinations must be outside original source')
    for path in (out,work):
        if path.is_symlink() or (path.exists() and (not path.is_dir() or any(path.iterdir()))):
            raise ValueError('Capture destination must be absent or empty: '+str(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', action='store_true')
    parser.add_argument('--case-pattern', default='')
    parser.add_argument('--large', action='store_true', help='Capture only the two genuine spill-size FilterLayer cases')
    parser.add_argument('--output', type=Path, default=ROOT/'migration/fixtures/legacy-max-rgb-context')
    parser.add_argument('--legacy-source', type=Path, required=True)
    parser.add_argument('--legacy-prefix', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    parser.add_argument('--timeout', type=float, default=300)
    args = parser.parse_args(); out=args.output.resolve(); work=args.work.resolve(); source=args.legacy_source.resolve()
    args.legacy_prefix = args.legacy_prefix.resolve()
    try:
        validate_destinations(source,out,work)
        hashes=verify_sources(source)
    except ValueError as exc: raise SystemExit(str(exc))
    out.mkdir(parents=True,exist_ok=True); work.mkdir(parents=True,exist_ok=True)
    tests = [c for c in cases(args.large) if args.case_pattern in c['id']]
    if not tests: raise SystemExit('No matching cases')
    write_header(work, tests)
    (out/'capture-plan.json').write_text(json.dumps(tests,indent=2)+'\n')
    (out/'.gitattributes').write_text('*.raw binary\n*.xcf binary\n')
    if not args.capture: print('Prepared',len(tests),'genuine old PDB/FilterLayer cases; no execution'); return 0
    shutil.copyfile(Path(__file__), out/'capture-script.py')
    shutil.copyfile(ROOT/'migration/tests/legacy-max-rgb-capture.c', out/'capture-harness.c')
    shutil.copyfile(Path(context.__file__), out/'observation-helper.py')
    shutil.copyfile(ROOT/'tools/check_max_rgb_evidence.py', out/'evidence-validator.py')
    shutil.copyfile(work/'legacy-max-rgb-cases.h', out/'capture-cases.h')
    started = datetime.now(timezone.utc).isoformat()
    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        executable, commands = build(source,work,out)
        binaries = [work/'.libs/legacy-max-rgb',args.legacy_prefix/'bin/gimp-2.8',
                    args.legacy_prefix/'lib/gimp/2.0/plug-ins/max-rgb']
        binary_hashes = {str(path):sha(path) for path in binaries}
        code, command = run(executable,args.legacy_prefix,out,args.timeout)
    errors,count = validate(out,tests,code)
    diagnostics = [line for line in (out/'capture.log').read_text(errors='replace').splitlines()
                   if any(word in line.lower() for word in ('critical', 'warning', 'error', 'assertion', 'failed'))]
    post_hashes = {str(path):sha(path) for path in binaries}
    post_sources = {name:sha(source/name) for name in SOURCES}
    if post_hashes != binary_hashes or post_sources != hashes: errors.append('Oracle source or executable changed during capture')
    for source_path, archived in ((Path(__file__), 'capture-script.py'),
        (ROOT/'migration/tests/legacy-max-rgb-capture.c', 'capture-harness.c'),
        (Path(context.__file__), 'observation-helper.py'),
        (ROOT/'tools/check_max_rgb_evidence.py', 'evidence-validator.py')):
        if sha(source_path) != sha(out/archived): errors.append('Capture source changed during execution: '+archived)
    for filename in ('build.log','capture.log'):
        path=out/filename
        (out/(filename+'.gz')).write_bytes(gzip.compress(path.read_bytes(),mtime=0)); path.unlink()
    report=dict(schema_version=1,status='passed' if not errors else 'failed',source_commit=COMMIT,
        started_utc=started,finished_utc=datetime.now(timezone.utc).isoformat(),source_sha256=hashes,
        executable_sha256=binary_hashes,build_commands=commands,command=command,exit_code=code,
        case_count=count,expected_cases=len(tests),large_cases=args.large,errors=errors,
        capture_script_sha256=sha(out/'capture-script.py'),harness_sha256=sha(out/'capture-harness.c'),
        instrumentation_sha256=sha(out/'observation-hooks.patch'),
        observation_helper_sha256=sha(out/'observation-helper.py'),
        evidence_validator_sha256=sha(out/'evidence-validator.py'),
        post_capture_source_sha256=post_sources,post_capture_executable_sha256=post_hashes,
        runtime_diagnostics=diagnostics,
        profile_policy='Fresh isolated profile with only the pinned original Max RGB plug-in',
        files_sha256={p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()})
    (out/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],count,errors); return 0 if not errors else 1

if __name__ == '__main__': raise SystemExit(main())

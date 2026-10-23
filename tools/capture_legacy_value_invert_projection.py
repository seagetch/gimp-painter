#!/usr/bin/env python3
"""Capture genuine pinned Value Invert projection-copy PDB and FilterLayer history evidence.

Only observation hooks are linked into a private harness. The original plug-in,
merge implementation and projection-copy algorithm stay unchanged. Source the
replay env.sh before --capture. Without --capture this only prepares the finite case header/plan.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import fcntl
import gzip
import difflib
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile

import capture_legacy_filter_context as context

ROOT = Path(__file__).resolve().parents[1]
COMMIT = context.COMMIT
SOURCES = tuple(name for name in context.SOURCES if not name.endswith('/blinds.c')) + (
    'plug-ins/common/value-invert.c', 'libgimp/gimpprogress.c',
    'app/core/gimpgrouplayer.c', 'app/core/gimpchannel.c', 'app/core/gimpprojection.c', 'app/base/tile-manager.c', 'app/plug-in/gimpplugin-cleanup.c', 'app/xcf/xcf-save.c', 'app/xcf/xcf-write.c',
    'app/core/gimpparamspecs.c', 'app/plug-in/gimppluginprocedure.c',
    'app/plug-in/gimppluginmanager-call.c', 'libgimp/gimppixelrgn.c', 'libgimp/gimpdrawable.c')
VALUE_INVERT_SOURCE_SHA256 = 'bf7c46cac7f62c13ec73590d53c12ba3dd85fd28def5f74451aaf43b969fef20'
sha = context.sha
COLUMNS = ('id source_channels grouped seeded initial_x initial_y final_x final_y '
           'starts ends merges copies initial_loaded_starts initial_loaded_ends').split()


def cases(large=False):
    if large: raise ValueError('Projection supplement has one finite small matrix')
    result=[]
    for channels in (3,4):
        for grouped in (0,1):
            for negative in (0,1):
                for seeded in (0,1):
                    x,y=(-4,-3) if negative else (3,2)
                    result.append(dict(id=f"projection-c{channels}-{'group' if grouped else 'top'}-{'negative' if negative else 'positive'}-{'seeded' if seeded else 'fresh'}",
                        width=17,height=13,components=channels,image_width=13,image_height=11,
                        offset_x=x,offset_y=y,live=1,selection=0,
                        final_selection=-1,active_components=15,final_components=-1,
                        lock_alpha=0,final_lock=-1,zero_alpha=0,rerun=0,writer=0,phase=0,
                        idle_selection=-1,idle_components=-1,idle_lock=-1,grouped=grouped,seeded=seeded))
    return result


def write_header(work, tests):
    fields = ('width height components image_width image_height offset_x offset_y live '
              'selection final_selection active_components final_components lock_alpha final_lock '
              'zero_alpha rerun writer phase idle_selection idle_components idle_lock grouped seeded').split()
    lines = ['typedef struct { const char *id; gint ' + ', '.join(fields) + '; } CaptureCase;',
             'static const CaptureCase capture_cases[] = {']
    for case in tests:
        lines.append('  {' + ', '.join([json.dumps(case['id'])] + [str(case[k]) for k in fields]) + '},')
    lines.append('};')
    (work/'legacy-value-invert-projection-cases.h').write_text('\n'.join(lines)+'\n')


def instrument(source,work,out):
    paths=context.instrument(source,work,out)
    path=work/'gimpfilterlayer.cpp'
    before=path.read_text()
    anchor='      copy_region_nocow(&srcPR, &destPR);'
    extra='''      extern void capture_projection_copy_cpp (GimpDrawable *, TileManager *, gint, gint, gint, gint, gint, gint, gint, gint);
      capture_projection_copy_cpp (GIMP_DRAWABLE(g_object), projPR->tiles,
        parent_off_x, parent_off_y, source_area.x, source_area.y,
        dest_area.x, dest_area.y, dest_area.width, dest_area.height);
'''
    edited=context.replace_once(before,anchor,extra+anchor)
    edited+='''
extern "C" void capture_projection_copy (GimpDrawable *, TileManager *, gint, gint, gint, gint, gint, gint, gint, gint);
namespace GLib {
void capture_projection_copy_cpp (GimpDrawable *drawable, TileManager *tiles,
  gint parent_x, gint parent_y, gint source_x, gint source_y,
  gint dest_x, gint dest_y, gint width, gint height)
{ capture_projection_copy (drawable, tiles, parent_x, parent_y, source_x, source_y, dest_x, dest_y, width, height); }
}
'''
    path.write_text(edited)
    patch=(out/'observation-hooks.patch').read_text()
    patch+=''.join(difflib.unified_diff(before.splitlines(True),edited.splitlines(True),
        fromfile='observation-base/app/core/gimpfilterlayer.cpp',tofile='projection-observation/app/core/gimpfilterlayer.cpp'))
    (out/'observation-hooks.patch').write_text(patch)
    return paths


def build(source, work, out):
    cwd = source/'app/tests'
    dry = subprocess.check_output(['make', '-n', '-W', 'test-xcf.c', 'test-xcf'], cwd=cwd, text=True)
    compile_line = next(line for line in dry.splitlines() if ' -c -o test-xcf.o ' in line)
    flags = shlex.split(compile_line[compile_line.index('/usr/bin/gcc '):].split(' -MT ')[0])[1:]
    includes = [flag for flag in flags if flag.startswith(('-I', '-D')) or flag == '-pthread']
    includes += ['-I'+str(source/'app/core'), '-I'+str(source/'app/pdb'), '-I'+str(work)]
    paths = instrument(source, work, out)
    commands, objects = [], []
    for path in [out/'capture-harness.c'] + paths:
        output = work/(path.stem+'.o')
        cmd = (['g++', '-std=c++17', '-include', 'type_traits'] if path.suffix == '.cpp' else ['gcc']) + includes
        cmd += ['-g', '-O1', '-fcommon', '-c', str(path), '-o', str(output)]
        commands.append(cmd); objects.append(str(output))
    link_line = next(line for line in dry.splitlines() if '--mode=link ' in line)
    cmd = shlex.split(link_line[link_line.index('/bin/bash '):])
    index = cmd.index('-o'); executable = work/'legacy-value-invert-projection'
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
    with tempfile.TemporaryDirectory(prefix='gimp-old-value-invert-profile-') as tmp:
        home = Path(tmp)
        for name in ('profile', 'plugins', 'empty', 'temp', 'cache', 'config', 'data'):
            (home/name).mkdir(mode=0o700)
        (home/'plugins/value-invert').symlink_to(prefix/'lib/gimp/2.0/plug-ins/value-invert')
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


def validate(out, tests, code):
    # Import locally so preparing the matrix never reads archive pins. This
    # validator checks captured observations; it does not produce oracle pixels.
    from check_value_invert_projection_evidence import validate_directory
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
    if hashes.get('plug-ins/common/value-invert.c') != VALUE_INVERT_SOURCE_SHA256:
        raise ValueError('Wrong pinned Value Invert callback')
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
    parser.add_argument('--large', action='store_true', help='Unsupported: projection history has one finite small matrix')
    parser.add_argument('--output', type=Path, default=ROOT/'migration/fixtures/legacy-value-invert-projection')
    parser.add_argument('--legacy-source', type=Path, required=True)
    parser.add_argument('--legacy-prefix', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--lock', type=Path, default=Path('/workspace/shared/gimp-painter-build.lock'))
    parser.add_argument('--timeout', type=float, default=300)
    args = parser.parse_args(); out=args.output.resolve(); work=args.work.resolve(); source=args.legacy_source.resolve()
    args.legacy_prefix = args.legacy_prefix.resolve()
    try:
        validate_destinations(source, out, work)
        hashes = verify_sources(source)
    except ValueError as exc:
        raise SystemExit(str(exc))
    out.mkdir(parents=True,exist_ok=True); work.mkdir(parents=True,exist_ok=True)
    tests = [c for c in cases(args.large) if args.case_pattern in c['id']]
    if not tests: raise SystemExit('No matching cases')
    write_header(work, tests)
    (out/'capture-plan.json').write_text(json.dumps(tests,indent=2)+'\n')
    (out/'.gitattributes').write_text('*.raw binary\n*.xcf binary\n')
    if not args.capture: print('Prepared',len(tests),'genuine old PDB/FilterLayer cases; no execution'); return 0
    shutil.copyfile(Path(__file__), out/'capture-script.py')
    shutil.copyfile(ROOT/'migration/tests/legacy-value-invert-projection-capture.c', out/'capture-harness.c')
    shutil.copyfile(Path(context.__file__), out/'observation-helper.py')
    shutil.copyfile(ROOT/'tools/check_value_invert_projection_evidence.py', out/'evidence-validator.py')
    shutil.copyfile(ROOT/'tools/check_value_invert_evidence.py', out/'base-evidence-validator.py')
    shutil.copyfile(work/'legacy-value-invert-projection-cases.h', out/'capture-cases.h')
    started = datetime.now(timezone.utc).isoformat()
    with args.lock.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        executable, commands = build(source,work,out)
        binaries = [work/'.libs/legacy-value-invert-projection',args.legacy_prefix/'bin/gimp-2.8',
                    args.legacy_prefix/'lib/gimp/2.0/plug-ins/value-invert']
        binary_hashes = {str(path):sha(path) for path in binaries}
        code, command = run(executable,args.legacy_prefix,out,args.timeout)
    errors,count = validate(out,tests,code)
    diagnostics = [line for line in (out/'capture.log').read_text(errors='replace').splitlines()
                   if any(word in line.lower() for word in ('critical', 'warning', 'error', 'assertion', 'failed'))]
    post_hashes = {str(path):sha(path) for path in binaries}
    post_sources = {name:sha(source/name) for name in SOURCES}
    if post_hashes != binary_hashes or post_sources != hashes: errors.append('Oracle source or executable changed during capture')
    for source_path, archived in ((Path(__file__), 'capture-script.py'),
        (ROOT/'migration/tests/legacy-value-invert-projection-capture.c', 'capture-harness.c'),
        (Path(context.__file__), 'observation-helper.py'),
        (ROOT/'tools/check_value_invert_projection_evidence.py', 'evidence-validator.py'),
        (ROOT/'tools/check_value_invert_evidence.py', 'base-evidence-validator.py')):
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
        base_evidence_validator_sha256=sha(out/'base-evidence-validator.py'),
        post_capture_source_sha256=post_sources,post_capture_executable_sha256=post_hashes,
        runtime_diagnostics=diagnostics,
        profile_policy='Fresh isolated profile with only the unchanged original Value Invert plug-in; projection-copy observer only',
        files_sha256={p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()})
    (out/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],count,errors); return 0 if not errors else 1

if __name__ == '__main__': raise SystemExit(main())

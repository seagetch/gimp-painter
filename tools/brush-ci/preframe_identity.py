#!/usr/bin/env python3
"""Closed identity handoff for a separate, never-acceptance case14 diagnostic."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys

CASE = '/brush-thumbnail-native-popup/private-target-release'
CONTROLLER_SHA = 'abee6fd22c605cc63e8e8b9b3d594241671929f9ce2c39d754129e1884387a48'
ASAN = 'halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1'
UBSAN = 'halt_on_error=1:print_stacktrace=1'
class Invalid(Exception): pass

def require(ok):
    if not ok: raise Invalid()

def cleanpath(value):
    require(isinstance(value, str) and len(value) < 4096 and not any(ord(c) < 32 for c in value))
    p = Path(value)
    require(p.is_absolute() and str(p) == value and p.resolve(strict=True) == p)
    return p

def regular(p):
    cleanpath(str(p))
    s = p.stat()
    require(stat.S_ISREG(s.st_mode) and s.st_uid == os.getuid())
    return p

def digest(p):
    regular(p)
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1024*1024), b''): h.update(block)
    return h.hexdigest()

def readjson(p, limit=4*1024*1024):
    regular(p)
    require(p.stat().st_size <= limit)
    return json.loads(p.read_text())

def command(args):
    r = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=30, check=True)
    require(len(r.stdout) <= 4*1024*1024)
    return r.stdout.decode('utf-8')

def hosted():
    sha = os.environ.get('GITHUB_SHA', '')
    require(os.environ.get('GITHUB_ACTIONS') == 'true' and re.fullmatch('[0-9a-f]{40}', sha))
    return sha

def roots(run, source, build):
    run, source, build = map(cleanpath, (run, source, build))
    temp = cleanpath(os.environ.get('RUNNER_TEMP', ''))
    require(run.parent == temp and re.fullmatch(r'painter-brush-ci\.[A-Za-z0-9]{8}', run.name))
    s = run.stat()
    require(stat.S_ISDIR(s.st_mode) and s.st_uid == os.getuid() and stat.S_IMODE(s.st_mode) == 0o700)
    require(source == run/'source' and build == run/'build' and source.is_dir() and build.is_dir())
    return run, source, build

def source_identity(source, sha):
    require(command(['git', '-C', str(source), 'rev-parse', 'HEAD']).strip() == sha)
    require(command(['git', '-C', str(source), 'status', '--porcelain=v1', '--untracked-files=all', '--ignore-submodules=none']) == '')

def options(value):
    require(isinstance(value, dict))
    require(value.get('ASAN_OPTIONS') == ASAN and value.get('UBSAN_OPTIONS') == UBSAN and value.get('LSAN_OPTIONS_unset') is True)
    return {'ASAN_OPTIONS': ASAN, 'UBSAN_OPTIONS': UBSAN, 'LSAN_OPTIONS_unset': True}

def build_identity(build):
    opts = readjson(build/'meson-info/intro-buildoptions.json')
    require(isinstance(opts, list) and all(isinstance(x, dict) for x in opts))
    values = [x['value'] for x in opts if x.get('name') == 'b_sanitize']
    require(len(values) == 1)
    require(isinstance(values[0], (list, str)))
    names = values[0] if isinstance(values[0], list) else values[0].split(',')
    require(all(isinstance(x, str) for x in names))
    require(set(names) == {'address', 'undefined'} and len(names) == 2)
    targets = readjson(build/'meson-info/intro-targets.json', limit=32*1024*1024)
    require(isinstance(targets, list) and all(isinstance(x, dict) for x in targets))
    targets = [x for x in targets if x.get('name') == 'painter-compact-options' and x.get('type') == 'executable']
    require(len(targets) == 1 and isinstance(targets[0]['filename'], list) and len(targets[0]['filename']) == 1)
    binary = cleanpath(targets[0]['filename'][0])
    require(binary.is_relative_to(build) and os.access(binary, os.X_OK))
    return binary

def files(run, source, build, binary):
    paths = [binary, build/'compile_commands.json', build/'build.ninja',
             build/'meson-info/intro-buildoptions.json', build/'meson-info/intro-targets.json', run/'deps/env.sh',
             source/'tools/linux-debian13-env.sh', source/'tools/brush-ci/preframe_identity.py',
             source/'tools/brush-ci/sample_case14.py']
    result = {str(p): digest(p) for p in paths}
    require(result[str(source/'tools/brush-ci/sample_case14.py')] == CONTROLLER_SHA)
    require(result[str(source/'tools/brush-ci/preframe_identity.py')] == digest(Path(__file__).resolve()))
    return result

def capture(args):
    sha = hosted()
    run, source, build = roots(args.run, args.source, args.build)
    source_identity(source, sha)
    opt = options(readjson(cleanpath(args.options_json)))
    binary = build_identity(build)
    tree = command(['git', '-C', str(source), 'rev-parse', 'HEAD^{tree}']).strip()
    require(re.fullmatch('[0-9a-f]{40}', tree))
    return {'format': 1, 'acceptance_eligible': False, 'status': 'identity-captured', 'source_tree': tree,
            'github_sha': sha, 'original_exit': args.original_exit, 'run': str(run),
            'source': str(source), 'build': str(build), 'binary': str(binary),
            'sanitizer_options': opt, 'hashes': files(run, source, build, binary)}

def verify(receipt):
    j = readjson(cleanpath(receipt))
    require(isinstance(j, dict))
    require(j['format'] == 1 and j['acceptance_eligible'] is False and type(j['original_exit']) is int and j['original_exit'] in (0, 1))
    require(j['github_sha'] == hosted())
    run, source, build = roots(j['run'], j['source'], j['build'])
    source_identity(source, j['github_sha'])
    require(command(['git', '-C', str(source), 'rev-parse', 'HEAD^{tree}']).strip() == j['source_tree'])
    options(j['sanitizer_options'])
    binary = build_identity(build)
    require(str(binary) == j['binary'])
    require(files(run, source, build, binary) == j['hashes'])
    return j

def write(output, data):
    p = Path(output)
    require(not p.is_symlink())
    p.write_text(json.dumps(data, indent=2)+'\n')

def main(argv=None):
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest='mode', required=True)
    c = sub.add_parser('capture')
    for flag in ('source', 'build', 'run', 'options-json', 'output'): c.add_argument('--'+flag, required=True)
    c.add_argument('--original-exit', type=int, choices=(0,1), required=True)
    for mode in ('verify', 'wrapper'):
        a = sub.add_parser(mode)
        a.add_argument('--receipt', required=True); a.add_argument('--output', required=True)
        if mode == 'wrapper':
            a.add_argument('--controller', required=True); a.add_argument('args', nargs=argparse.REMAINDER)
    a = p.parse_args(argv)
    try:
        if a.mode == 'capture': result = capture(a)
        else:
            j = verify(a.receipt)
            result = {'format': 1, 'acceptance_eligible': False, 'status': 'identity-verified'}
            if a.mode == 'wrapper':
                require(a.args == ['--', j['binary'], '-p', CASE])
                require(a.controller == str(Path(j['source'])/'tools/brush-ci/sample_case14.py'))
                require(digest(cleanpath(a.controller)) == CONTROLLER_SHA)
                require(os.environ.get('ASAN_OPTIONS') == ASAN and os.environ.get('UBSAN_OPTIONS') == UBSAN and 'LSAN_OPTIONS' not in os.environ)
                # exec, rather than a timeout-owning parent: controller owns all child cleanup.
                os.execv(sys.executable, [sys.executable, '-B', a.controller, '--gdb', '/usr/bin/gdb',
                         '--binary', j['binary'], '--sha256', j['hashes'][j['binary']],
                         '--output', a.output, '--', '-p', CASE])
        write(a.output, result)
        return 0
    except (Invalid, OSError, ValueError, TypeError, KeyError, subprocess.SubprocessError):
        try: write(a.output, {'format': 1, 'acceptance_eligible': False, 'status': 'inconclusive-identity-invalid'})
        except (Invalid, OSError): pass
        return 2

if __name__ == '__main__': sys.exit(main())

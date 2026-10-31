#!/usr/bin/env python3
"""Bounded old-frame style RED evidence; never candidate acceptance."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import os
import shutil
import subprocess

OLD_SHA = 'd158f0367d7c83540c49d4b997e1f3a99466766f5d7c0fb913d1989b10d47997'
TEST_SHA = '77399aea24654a7cb127d5231a8672ea56084aefd13fb6801cc7cc8aeccd5f08'
FIXTURE = 'libgimpwidgets/fixtures/gimpframe-c14.c'
TEST = 'libgimpwidgets/test-frame-style-idempotence.c'
TARGET = 'frame-style-old-provider'
CASES = ('/gimp-frame-style/unchanged',)
APIS = ('gimp_frame_c14_get_type', 'gimp_frame_c14_new')
ASSERTION = re.compile(r'^(?:Bail out! )?(?:LibGimpWidgets:)?ERROR:[^\n]*\btest-frame-style-idempotence\.c:\d+:assert_unchanged_update: assertion failed \(count == 0\): \(3 == 0\)$')

def intended_assertion(line, case):
    prefix = re.fullmatch(r'not ok (?:1 )?' + re.escape(case) + r' - ((?:LibGimpWidgets:)?ERROR:.*)', line)
    return bool(ASSERTION.fullmatch(prefix.group(1) if prefix else line))

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def base(kind):
    return {'format': 1, 'kind': kind, 'control_passed': False,
            'negative_control': True, 'acceptance_eligible': False, 'errors': [],
            'raw_environment_exported': False}

def rows_from(path):
    text = Path(path).read_text()
    # Real meson-logs/*.json is JSON Lines, one registration per line.
    return [json.loads(line) for line in text.splitlines() if line.strip()]

def check_case(rows, case):
    r = base('frame-old-provider-case'); e = r['errors']; r['selected_case'] = case
    if case not in CASES:
        e.append('Unknown exact leaf'); return r
    if len(rows) != 1 or not isinstance(rows[0], dict):
        e.append('Exactly one Meson registration required'); return r
    t = rows[0]
    r['test'] = {k: t.get(k) for k in ('name', 'result', 'returncode', 'duration', 'stdout', 'stderr')}
    if str(t.get('name', '')).split(' / ')[-1] != TARGET: e.append('Wrong Meson target')
    if t.get('result') != 'EXPECTEDFAIL' or type(t.get('returncode')) is not int or t['returncode'] != -6:
        e.append('Expected Meson EXPECTEDFAIL with real child SIGABRT (-6), not generic failure/timeout/skip/success')
    for envkey in ('env', 'environment'):
        env = t.get(envkey, {})
        if isinstance(env, dict) and any(env.get(k) for k in ('LD_PRELOAD','LD_AUDIT')):
            e.append('Injected runtime provider environment')
    cmd = t.get('command')
    if not isinstance(cmd, list) or not cmd or not all(isinstance(x, str) for x in cmd):
        cmd = []; e.append('Missing actual Meson argv')
    if cmd:
        if Path(cmd[0]).name != TARGET: e.append('Wrong executed provider')
        # No alternate selection, wrappers, skip flags, or trailing extra selectors.
        if cmd[1:] != ['-p', case]: e.append('Expected only exact -p leaf selection')
    stdout, stderr = t.get('stdout'), t.get('stderr')
    if not isinstance(stdout, str) or not isinstance(stderr, str):
        e.append('Raw stdout and stderr required'); stdout = stderr = ''
    output = stdout + '\n' + stderr
    if not re.search(r'^TAP version \d+\s*$', stdout, re.M): e.append('Missing TAP execution')
    for part in ('gimp-frame-style',):
        if '# Start of ' + part + ' tests' not in stdout: e.append('Missing TAP hierarchy: ' + part)
    for stream in (stdout, stderr):
        lines = [line for line in stream.splitlines() if 'assertion failed' in line]
        if len(lines) != 1 or not intended_assertion(lines[0], case): e.append('Wrong/missing exact lifetime assertion')
    if re.search(r'^\s*ok\b|^\s*1\.\.0\b|#\s*(?:SKIP|TODO)\b', output, re.M | re.I):
        e.append('Completed/zero/skip TAP does not prove intended abort')
    if re.search(r'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:|DEADLYSIGNAL|CRITICAL|WARNING|Traceback|timed? out|timeout', output, re.I):
        e.append('Unrelated sanitizer/runtime/timeout diagnostic')
    not_ok = re.findall(r'^not ok .*$', output, re.M)
    if len(not_ok) > 1 or any(not intended_assertion(line, case) for line in not_ok):
        e.append('Wrong or multiple failing TAP leaf')
    if not re.search(r'^Bail out!(?: ERROR:.*)?$', stdout, re.M):
        e.append('Missing TAP bailout')
    # Reject additional GLib ERROR lines, including unrelated assertions.
    for line in output.splitlines():
        if re.search(r'\bERROR:', line) and not intended_assertion(line, case): e.append('Unrelated ERROR diagnostic')
    r['control_passed'] = not e
    return r

def _resolved(path, directory):
    p = Path(path)
    return (p if p.is_absolute() else directory / p).resolve()

PRODUCTION_SHA = '6207252ed416e8e295e6a60fd5f3986f8fc28f38cd18e3b8810efefe973c9386'
PINNED = {
    'libgimpwidgets/gimpframe.h': '1fffcfb5f5ec45eeffd39a6c426abca390473f25fd9f944c48bfb32cce4de122',
    'libgimpwidgets/gimpwidgetstypes.h': '5063f38f0c8def2c5e433a975fb32dbae44393fbd19d7a707623cbf53fe654fc',
    'libgimpwidgets/gimpwidgetsutils.h': '7336640ff0771f98d344e92c9eb2a5e0a1f0c5ba82b9080e99138c8d8bc7808a',
    'libgimpwidgets/gimpwidgetsutils.c': 'aa84583b0b60d1e53136cfb87d8d2e55b8f3cf0901991f38727db676f76c2a51',
}
MACROS = {'-Dgimp_frame_new=gimp_frame_c14_new', '-Dgimp_frame_get_type=gimp_frame_c14_get_type'}
CANONICAL = {'gimp_frame_new', 'gimp_frame_get_type'}
HELPER = 'gimp_label_set_attributes'
MAX_MAP_BYTES = 256 * 1024 * 1024
MAX_INSPECT_BYTES = 64 * 1024 * 1024

def map_text(path):
    if path.stat().st_size > MAX_MAP_BYTES: raise ValueError('Link map exceeds 256 MiB evidence bound')
    return path.read_text()

def run(argv, loader_env=None, cwd=None):
    env = dict(os.environ, LC_ALL='C')
    if loader_env is not None:
        for key in list(env):
            if key.startswith('LD_') or key == 'GLIBC_TUNABLES': env.pop(key)
        env.update(loader_env)
    completed = subprocess.run(argv, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               timeout=60, env=env, cwd=cwd, check=False)
    if len(completed.stdout.encode()) > MAX_INSPECT_BYTES or len(completed.stderr.encode()) > 1024 * 1024:
        raise ValueError('Inspection output exceeds bounded evidence limit')
    if completed.returncode or completed.stderr.strip():
        raise ValueError('Provider inspection failed: ' + Path(argv[0]).name)
    return completed.stdout

def symbols(text):
    result = []
    for line in text.splitlines():
        match = re.match(r'\s*\d+:\s+[0-9a-fA-F]+\s+\d+\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)', line)
        if match:
            typ, bind, vis, ndx, name = match.groups()
            result.append({'name': name.split('@')[0], 'type': typ, 'bind': bind, 'visibility': vis, 'defined': ndx != 'UND'})
    if not result: raise ValueError('Missing ELF symbol table')
    return result

def has_definition(rows, name):
    return any(x['name'] == name and x['defined'] and x['type'] == 'FUNC' and x['bind'] == 'GLOBAL' for x in rows)

def map_proof(text, build, output, obj, names):
    if 'Cross Reference Table' not in text: raise ValueError('GNU --cref table required')
    paths = re.findall(r'^OUTPUT\((\S+)(?:\s+[^)]*)?\)\s*$', text, re.M)
    if len(paths) != 1 or _resolved(paths[0], build) != output: raise ValueError('Map OUTPUT mismatch')
    loads = [_resolved(x, build) for x in re.findall(r'^LOAD\s+(\S+)\s*$', text, re.M)]
    if obj not in loads: raise ValueError('Map missing actual object LOAD')
    table = text.split('Cross Reference Table', 1)[1]
    for name in names:
        defs = re.findall(r'^' + re.escape(name) + r'\s+(\S+)\s*$', table, re.M)
        if len(defs) != 1 or _resolved(defs[0], build) != obj: raise ValueError('Map defining provider mismatch: ' + name)

def label(path, source, build):
    if path.is_relative_to(source): return 'source/' + str(path.relative_to(source))
    if path.is_relative_to(build): return 'build/' + str(path.relative_to(build))
    return 'external/' + str(path).lstrip('/')

def parse_loaded(listing):
    if 'not found' in listing: raise ValueError('Unresolved runtime library')
    out={}
    for name,path in re.findall(r'^\s*(\S+)\s+=>\s+(\S+)\s+\(0x[0-9a-fA-F]+\)\s*$',listing,re.M):
        if name in out: raise ValueError('Duplicate loaded soname')
        out[name]=Path(path).resolve()
    return out


def check_provider(source, build, map_path):
    if os.environ.get('LD_PRELOAD') or os.environ.get('LD_AUDIT'):
        raise ValueError('Preload/audit provider injection forbidden')
    source, build = Path(source).resolve(), Path(build).resolve()
    map_path = Path(map_path).absolute()
    expected_map = build / 'libgimpwidgets/frame-style-old-provider.map'
    if map_path != expected_map or map_path.resolve() != map_path: raise ValueError('Wrong/symlinked target map')
    r = base('frame-old-provider-provenance'); evidence = set()
    fixture, test, utils = source / FIXTURE, source / TEST, source / 'libgimpwidgets/gimpwidgetsutils.c'
    for rel, digest in dict(PINNED, **{FIXTURE: OLD_SHA, TEST: TEST_SHA, 'libgimpwidgets/gimpframe.c': PRODUCTION_SHA}).items():
        path = source / rel
        if path.resolve() != path or sha(path) != digest: raise ValueError('Source/header differs from reviewed exact input: ' + rel)
        evidence.add(path)
    cc_path = build / 'compile_commands.json'; options_path = build / 'meson-info/intro-buildoptions.json'
    cc, options = json.loads(cc_path.read_text()), json.loads(options_path.read_text())
    values = [x.get('value') for x in options if x.get('name') == 'b_sanitize']
    if len(values) != 1 or values[0] not in ('none', [], ['none']): raise ValueError('Old frame provider must be non-sanitized')
    evidence.update((cc_path, options_path, map_path))
    objects = {}; compilers = set()
    specs = [('old', fixture, TARGET + '.p'), ('old_test', test, TARGET + '.p'),
             ('candidate_test', test, 'test-frame-style-idempotence.p'), ('utils', utils, None),
             ('production', source/'libgimpwidgets/gimpframe.c', None)]
    for key, src, parent in specs:
        matched = []
        for entry in cc:
            directory = Path(entry['directory']).resolve()
            if _resolved(entry['file'], directory) != src: continue
            args = entry.get('arguments', None)
            if args is None: args = shlex.split(entry.get('command', ''))
            if not isinstance(args, list) or not args or not all(isinstance(x, str) for x in args): raise ValueError('Invalid compiler argv')
            outputs = [args[i+1] for i, x in enumerate(args[:-1]) if x == '-o']
            if len(outputs) != 1: raise ValueError('Ambiguous compiler output')
            raw_obj = Path(outputs[0]); raw_obj = raw_obj if raw_obj.is_absolute() else directory / raw_obj
            obj = raw_obj.resolve()
            if parent and parent not in obj.parts: continue
            if key in ('utils','production') and not any(x.startswith('libgimpwidgets-3.0.so') and x.endswith('.p') for x in obj.parts): continue
            if raw_obj.absolute() != obj or not obj.is_relative_to(build): raise ValueError('Compiler output outside build/symlinked')
            if '-c' not in args or sum(_resolved(x, directory) == src for x in args if not x.startswith('-')) != 1: raise ValueError('Input not directly compiled once')
            if any(x.startswith(('-fsanitize', '@')) for x in args): raise ValueError('Sanitizer/response compile override forbidden')
            frame_flags = [x for x in args if re.match(r'-[DU]gimp_frame_', x)]
            expected = MACROS if key in ('old','old_test') else set()
            if set(frame_flags) != expected or len(frame_flags) != len(expected): raise ValueError('Exact target-local API namespaces required')
            if any(x.startswith(('-include','-imacros')) for x in args): raise ValueError('Forced alternate header/provider forbidden')
            compiler = shutil.which(args[0])
            if not compiler: raise ValueError('Compiler executable provenance missing')
            compilers.add(Path(compiler).resolve()); matched.append(obj)
        if len(matched) != 1: raise ValueError('Expected one exact compile entry: ' + key)
        objects[key] = matched[0]; evidence.add(matched[0])
    # Ninja records TU header dependencies. VALID only matches object mtime;
    # fresh sealed compilation plus source/header hashes supply content context.
    # Missing/stale dependency records are never admitted.
    dep_hashes = {}
    for key, obj in objects.items():
        dep_text = run(['ninja', '-C', str(build), '-t', 'deps', str(obj.relative_to(build))])
        heads = [line for line in dep_text.splitlines() if ': #deps ' in line]
        if len(heads) != 1 or not heads[0].endswith('(VALID)'): raise ValueError('Missing/stale Ninja dependencies: ' + key)
        if _resolved(heads[0].split(': #deps ',1)[0], build) != obj: raise ValueError('Dependencies for different object')
        deps = {_resolved(x.strip(), build) for x in dep_text.splitlines() if x.startswith('    ') and x.strip()}
        required = {build / 'config.h'}
        if key in ('old','old_test','candidate_test','production'): required.update(source / p for p in ('libgimpwidgets/gimpframe.h','libgimpwidgets/gimpwidgetstypes.h'))
        if key in ('old','utils','production'): required.add(source / 'libgimpwidgets/gimpwidgetsutils.h')
        if not required <= deps: raise ValueError('Exact compiled headers missing: ' + key)
        for dep in deps:
            if dep.suffix == '.h':
                if not dep.is_file(): raise ValueError('Compiled header unavailable: ' + str(dep))
                evidence.add(dep)
        dep_hashes[key] = hashlib.sha256(dep_text.encode()).hexdigest()
    binary = build / 'libgimpwidgets' / TARGET
    candidate = build / 'libgimpwidgets/test-frame-style-idempotence'
    elf = {key: symbols(run(['readelf','-Ws',str(path)])) for key,path in {**objects,'binary':binary,'candidate':candidate}.items()}
    for key in ('candidate_test','candidate','production'):
        if any(x['name'] in APIS for x in elf[key]): raise ValueError('Unique old namespace leaked into candidate')
    for key in ('candidate_test','candidate'):
        if not any(x['name']=='gimp_frame_new' and not x['defined'] for x in elf[key]): raise ValueError('Candidate does not call real canonical constructor')
        if any(x['name'] in CANONICAL and x['defined'] for x in elf[key]): raise ValueError('Candidate interposes canonical provider')
    if not all(has_definition(elf['production'],api) for api in CANONICAL): raise ValueError('Production frame definitions missing')
    for key in ('old','old_test','binary'):
        if any(x['name'] in CANONICAL for x in elf[key]): raise ValueError('Canonical frame symbol leaked into old target: ' + key)
    for key in ('old','binary'):
        if not all(has_definition(elf[key], api) for api in APIS): raise ValueError('Unique API definitions absent: ' + key)
    if not any(x['name']=='gimp_frame_c14_new' and not x['defined'] for x in elf['old_test']): raise ValueError('Test does not call unique old constructor')
    if any(x['name']=='gimp_frame_c14_new' and x['defined'] for x in elf['old_test']): raise ValueError('Test replaced constructor')
    if not has_definition(elf['utils'], HELPER): raise ValueError('Unchanged utils object does not define helper')
    if not any(x['name']==HELPER and not x['defined'] for x in elf['old']): raise ValueError('Old frame does not call real label helper')
    if any(has_definition(elf[key],HELPER) for key in ('binary','candidate')): raise ValueError('Executable interposes label helper')
    oldmap = map_text(map_path)
    map_proof(oldmap, build, binary, objects['old'], APIS)
    old_loads = {_resolved(x,build) for x in re.findall(r'^LOAD\s+(\S+)\s*$',oldmap,re.M)}
    if objects['old_test'] not in old_loads: raise ValueError('Map missing exact test object LOAD')
    if re.search(r'[^\s()]+\.a\([^\n)]*gimpframe\.c\.o\)', oldmap): raise ValueError('Canonical frame archive member extracted')
    dynamic = run(['readelf','-dW',str(binary)])
    if re.search(r'\((?:AUDIT|DEPAUDIT)\)',dynamic): raise ValueError('ELF audit provider injection forbidden')
    interp = re.findall(r'Requesting program interpreter:\s*(\S+)\]',run(['readelf','-lW',str(binary)]))
    if len(interp)!=1: raise ValueError('Exact ELF loader required')
    loader = Path(interp[0]).resolve()
    if loader.name!='ld-linux-x86-64.so.2' or loader.parent not in (Path('/usr/lib/x86_64-linux-gnu'),Path('/lib/x86_64-linux-gnu'),Path('/usr/lib64'),Path('/lib64')): raise ValueError('Unexpected ELF loader')
    listing=run([str(loader),'--list',str(binary)],cwd=str(build))
    if 'not found' in listing: raise ValueError('Unresolved runtime library')
    resolved = {}
    for name,path in re.findall(r'^\s*(\S+)\s+=>\s+(\S+)\s+\(0x[0-9a-fA-F]+\)\s*$',listing,re.M):
        if name in resolved: raise ValueError('Duplicate loaded soname')
        resolved[name] = Path(path).resolve()
    widgets=[p for n,p in resolved.items() if re.fullmatch(r'libgimpwidgets-3\.0\.so(?:\..*)?',n)]
    bases=[p for n,p in resolved.items() if re.fullmatch(r'libgimpbase-3\.0\.so(?:\..*)?',n)]
    if len(widgets)!=1 or not widgets[0].is_relative_to(build/'libgimpwidgets'): raise ValueError('Loaded widgets DSO not exact build provider')
    if len(bases)!=1 or not bases[0].is_relative_to(build/'libgimpbase'): raise ValueError('Loaded base DSO not exact build provider')
    widgets_dso=widgets[0]; widget_symbols=symbols(run(['readelf','-Ws',str(widgets_dso)]))
    if not all(has_definition(widget_symbols,api) for api in CANONICAL): raise ValueError('Production widgets canonical API exports missing')
    if not has_definition(widget_symbols,HELPER): raise ValueError('Real widgets DSO helper export missing')
    if any(x['name'] in APIS for x in widget_symbols): raise ValueError('Unique old APIs leaked into production DSO')
    helpermap=build/'libgimpwidgets/frame-helper-provider.map'
    map_proof(map_text(helpermap),build,widgets_dso,objects['utils'],(HELPER,))
    map_proof(map_text(helpermap),build,widgets_dso,objects['production'],CANONICAL)
    candidate_map=build/'libgimpwidgets/frame-candidate-provider.map'
    candidate_text=map_text(candidate_map)
    # main is external in the shared test TU and proves exact test-object linkage.
    map_proof(candidate_text,build,candidate,objects['candidate_test'],('main',))
    if re.search(r'\((?:AUDIT|DEPAUDIT)\)',run(['readelf','-dW',str(candidate)])): raise ValueError('Candidate ELF audit injection')
    candidate_interp=re.findall(r'Requesting program interpreter:\s*(\S+)\]',run(['readelf','-lW',str(candidate)]))
    if len(candidate_interp)!=1 or Path(candidate_interp[0]).resolve()!=loader: raise ValueError('Candidate loader mismatch')
    candidate_listing=run([str(loader),'--list',str(candidate)],cwd=str(build))
    candidate_resolved=parse_loaded(candidate_listing)
    if candidate_resolved.get(next(n for n,p in resolved.items() if p==widgets_dso))!=widgets_dso: raise ValueError('Candidate uses different widgets DSO')
    if candidate_resolved.get(next(n for n,p in resolved.items() if p==bases[0]))!=bases[0]: raise ValueError('Candidate uses different base DSO')
    canonical_binding=re.findall(r'^gimp_frame_new\s+(\S+)\s*$',candidate_text.split('Cross Reference Table',1)[1],re.M)
    if len(canonical_binding)!=1 or _resolved(canonical_binding[0],build)!=widgets_dso: raise ValueError('Candidate map constructor not real widgets provider')
    defs=re.findall(r'^'+HELPER+r'\s+(\S+)\s*$',oldmap.split('Cross Reference Table',1)[1],re.M)
    if len(defs)!=1 or _resolved(defs[0],build)!=widgets_dso: raise ValueError('Old map helper binding not loaded widgets DSO')
    evidence.update((binary,candidate,candidate_map,helpermap,widgets_dso,bases[0],loader,*compilers))
    metadata=build/'meson-info/intro-tests.json'
    tests=json.loads(metadata.read_text()); evidence.add(metadata)
    registrations={}
    for name,exe in ((TARGET,binary),('gimp-frame-style-idempotence',candidate)):
        entries=[t for t in tests if t.get('name')==name]
        if len(entries)!=1 or entries[0].get('cmd')!=[str(exe)]: raise ValueError('Exact Meson registration metadata missing')
        if entries[0].get('workdir') not in (None,str(build)): raise ValueError('Unexpected Meson runtime workdir')
        if any(entries[0].get('env',{}).get(k) for k in ('LD_PRELOAD','LD_AUDIT')): raise ValueError('Metadata injects loader provider')
        registrations[name]=entries[0]
    tools={}
    for name in ('readelf','ninja'):
        tool=shutil.which(name)
        if not tool: raise ValueError('Inspection tool missing')
        tool=Path(tool).resolve();evidence.add(tool);tools[name]=sha(tool)
    for path in evidence:
        if not path.is_file(): raise ValueError('Evidence missing: '+str(path))
        if path.is_relative_to(source) or path.is_relative_to(build):
            if path.resolve()!=path: raise ValueError('Evidence symlink not resolved')
    r.update(inputs={label(p,source,build):sha(p) for p in sorted(evidence)},
             objects={k:str(v.relative_to(build)) for k,v in objects.items()},
             api_definitions=list(APIS), binary_path=str(binary), binary_sha256=sha(binary),
             dependency_record_sha256=dep_hashes, inspection_tools=tools,
             loaded_libraries={n:{'path':str(p),'sha256':sha(p)} for n,p in resolved.items()},
             candidate_loaded_libraries={n:{'path':str(p),'sha256':sha(p)} for n,p in candidate_resolved.items()},
             source_root=str(source),build_root=str(build),provider_map=str(map_path),
             candidate_binary_path=str(candidate),candidate_binary_sha256=sha(candidate),
             loader_path=str(loader),runtime_workdir=str(build),runtime_binding_complete=False,
             old_internal_get_type_reference='May be optimized/inlined; exact source, macros and unique definitions verified',
             control_passed=True)
    return r

POSITIVE_CASES = ('unchanged','different-weight','same-weight-extra-attribute','partial-range-bold',
                  'equal-list-different-instance','replacement-label','style-change')

def check_candidate(rows,binary):
    if len(rows)!=1 or not isinstance(rows[0],dict): raise ValueError('Exactly one candidate registration required')
    row=rows[0]
    if str(row.get('name','')).split(' / ')[-1]!='gimp-frame-style-idempotence' or row.get('result')!='OK' or type(row.get('returncode')) is not int or row['returncode']!=0: raise ValueError('Candidate whole seven-case registration not passing')
    if row.get('command')!=[str(binary)]: raise ValueError('Candidate whole binary exact command required')
    stdout,stderr=row.get('stdout'),row.get('stderr','')
    if not isinstance(stdout,str) or not isinstance(stderr,str): raise ValueError('Raw candidate output missing')
    output=stdout+'\n'+stderr
    if re.search(r'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:|CRITICAL|WARNING|ERROR:|Traceback|Bail out!|^not ok|#\s*(SKIP|TODO)\b',output,re.I|re.M): raise ValueError('Candidate diagnostic/skip/failure')
    passed=re.findall(r'^ok (\d+) (/gimp-frame-style/[^\s]+)\s*$',stdout,re.M)
    expected=[(str(i+1),'/gimp-frame-style/'+leaf) for i,leaf in enumerate(POSITIVE_CASES)]
    if passed!=expected or len(re.findall(r'^ok\b',stdout,re.M))!=7 or re.findall(r'^1\.\.(\d+)\s*$',stdout,re.M)!=['7']: raise ValueError('Exact seven candidate TAP leaves and plan required')
    return row

def runtime_replay(provider,row,binary,expected_libraries):
    raw=row.get('env')
    if not isinstance(raw,dict) or not all(isinstance(k,str) and isinstance(v,str) for k,v in raw.items()): raise ValueError('Actual Meson full env metadata required; no guessed fallback')
    allow=('LD_LIBRARY_PATH','LD_BIND_NOW','GLIBC_TUNABLES')
    if any(v for k,v in raw.items() if k.startswith('LD_') and k not in allow): raise ValueError('Unexpected loader variable/injected provider')
    values={k:raw.get(k) for k in allow}
    effective={k:v for k,v in values.items() if v is not None}
    loaded=parse_loaded(run([provider['loader_path'],'--list',str(binary)],loader_env=effective,cwd=provider['runtime_workdir']))
    if not loaded or set(loaded)!=set(expected_libraries): raise ValueError('Runtime resolution replay library set differs')
    for name,path in loaded.items():
        expected=expected_libraries[name]
        if str(path)!=expected['path'] or sha(path)!=expected['sha256']: raise ValueError('Runtime resolution replay changed provider: '+name)
    return {'kind':'ELF loader resolution replay under actual Meson env, not live-module observation',
            'loader_env_sha256':hashlib.sha256(json.dumps(values,sort_keys=True).encode()).hexdigest(),
            'resolved_library_count':len(loaded),'raw_environment_exported':False}


def validate_provider_receipt(provider):
    if (provider.get('kind')!='frame-old-provider-provenance' or provider.get('control_passed') is not True or
        provider.get('errors')!=[] or provider.get('negative_control') is not True or provider.get('acceptance_eligible') is not False): raise ValueError('Invalid provider provenance receipt')
    for root in ('source_root','build_root'):
        if not isinstance(provider.get(root),str) or not Path(provider[root]).is_absolute(): raise ValueError('Missing exact provenance roots')
    inputs=provider.get('inputs');objects=provider.get('objects')
    if not isinstance(inputs,dict) or not inputs or not isinstance(objects,dict) or set(objects)!= {'old','old_test','candidate_test','utils','production'}: raise ValueError('Incomplete source/object evidence schema')
    pinned=dict(PINNED,**{FIXTURE:OLD_SHA,TEST:TEST_SHA,'libgimpwidgets/gimpframe.c':PRODUCTION_SHA})
    if any(inputs.get('source/'+name)!=digest for name,digest in pinned.items()): raise ValueError('Missing pinned source/header evidence')
    required={'build/compile_commands.json','build/meson-info/intro-buildoptions.json','build/meson-info/intro-tests.json',
              'build/libgimpwidgets/frame-style-old-provider.map','build/libgimpwidgets/frame-helper-provider.map',
              'build/libgimpwidgets/frame-candidate-provider.map','build/libgimpwidgets/frame-style-old-provider',
              'build/libgimpwidgets/test-frame-style-idempotence'}
    required.update('build/'+v for v in objects.values())
    if not required<=inputs.keys(): raise ValueError('Missing core binary/map/compiler evidence')
    if provider.get('api_definitions')!=list(APIS) or set(provider.get('dependency_record_sha256',{}))!=set(objects) or not provider.get('inspection_tools'): raise ValueError('Missing API/header/tool evidence')
    build=Path(provider['build_root'])
    if provider.get('runtime_workdir')!=str(build): raise ValueError('Missing exact Meson working directory')
    if provider.get('binary_path')!=str(build/'libgimpwidgets'/TARGET) or provider.get('candidate_binary_path')!=str(build/'libgimpwidgets/test-frame-style-idempotence'): raise ValueError('Receipt binary path mismatch')
    for key in ('loaded_libraries','candidate_loaded_libraries'):
        libs=provider.get(key)
        if not isinstance(libs,dict) or not libs: raise ValueError('Missing actual loaded-library evidence')
        for prefix,subdir in (('libgimpwidgets-3.0.so','libgimpwidgets'),('libgimpbase-3.0.so','libgimpbase')):
            hits=[item for name,item in libs.items() if name.startswith(prefix)]
            if len(hits)!=1 or not Path(hits[0].get('path','')).is_relative_to(build/subdir): raise ValueError('Missing exact widgets/base runtime identity')
    loader=Path(provider.get('loader_path',''))
    if (not loader.is_absolute() or loader.resolve()!=loader or loader.name!='ld-linux-x86-64.so.2' or
        loader.parent not in (Path('/usr/lib/x86_64-linux-gnu'),Path('/lib/x86_64-linux-gnu'),Path('/usr/lib64'),Path('/lib64'))):
        raise ValueError('Invalid receipt ELF loader path')
    if not inputs.get('external/'+str(loader).lstrip('/')): raise ValueError('Missing ELF loader identity')

def verify_receipt_files(provider):
    if os.environ.get('LD_PRELOAD') or os.environ.get('LD_AUDIT'): raise ValueError('Runtime provider injection forbidden')
    roots={'source':Path(provider['source_root']), 'build':Path(provider['build_root']), 'external':Path('/')}
    for name,digest in provider['inputs'].items():
        prefix,rel=name.split('/',1)
        if prefix not in roots or sha(roots[prefix]/rel)!=digest: raise ValueError('Provider evidence changed: '+name)
    for key in ('loaded_libraries','candidate_loaded_libraries'):
        for item in provider[key].values():
            if sha(item['path'])!=item['sha256']: raise ValueError('Loaded provider library changed')
    for pathkey,hashkey in (('binary_path','binary_sha256'),('candidate_binary_path','candidate_binary_sha256')):
        binary=Path(provider[pathkey])
        if binary.resolve()!=binary or sha(binary)!=provider[hashkey]: raise ValueError('Proven executable changed/symlinked')


def aggregate(paths, provider_path, candidate_log):
    r=base('frame-old-provider-aggregate')
    provider=json.loads(Path(provider_path).read_text())
    validate_provider_receipt(provider)
    verify_receipt_files(provider)  # fail before any loader execution
    binary=Path(provider['binary_path']);candidate=Path(provider['candidate_binary_path'])
    candidate_row=check_candidate(rows_from(candidate_log),candidate)
    if len(paths)!=1: raise ValueError('Exactly one historical unchanged-style receipt required')
    receipt=json.loads(Path(paths[0]).read_text())
    if (receipt.get('kind')!='frame-old-provider-case' or receipt.get('control_passed') is not True or
        receipt.get('errors')!=[] or receipt.get('acceptance_eligible') is not False or receipt.get('negative_control') is not True or
        receipt.get('selected_case')!=CASES[0]): raise ValueError('Invalid exact case receipt')
    log=Path(receipt.get('log_path',''))
    if not receipt.get('log_sha256') or sha(log)!=receipt['log_sha256']: raise ValueError('Raw log missing/changed')
    old_rows=rows_from(log)
    if not check_case(old_rows,CASES[0])['control_passed']: raise ValueError('Raw log revalidation failed')
    executed=Path(old_rows[0]['command'][0])
    if not executed.is_absolute() or executed.resolve()!=binary or executed.absolute()!=binary: raise ValueError('Case did not execute proven old binary')
    r['candidate_runtime_replay']=runtime_replay(provider,candidate_row,candidate,provider['candidate_loaded_libraries'])
    r['old_runtime_replay']=runtime_replay(provider,old_rows[0],binary,provider['loaded_libraries'])
    verify_receipt_files(provider)  # ensure identities remain stable after replay
    r.update(inputs={str(Path(p).resolve()):sha(p) for p in [*paths,provider_path,candidate_log]},
             binary_sha256=provider['binary_sha256'],selected_cases=list(CASES),candidate_passed_case_count=7,
             runtime_binding_complete=True,control_passed=True)
    return r


def main():
    p = argparse.ArgumentParser(description=__doc__)
    mode = p.add_mutually_exclusive_group(required=True)
    mode.add_argument('--log', type=Path); mode.add_argument('--provider', type=Path)
    mode.add_argument('--aggregate', type=Path, nargs=1)
    p.add_argument('--candidate-log', type=Path); p.add_argument('--provider-receipt', type=Path); p.add_argument('--case-id'); p.add_argument('--source', type=Path); p.add_argument('--build', type=Path)
    p.add_argument('--output', type=Path, required=True); a = p.parse_args()
    try:
        if a.log:
            if not a.case_id or a.source or a.build or a.provider_receipt or a.candidate_log: raise ValueError('Case mode accepts log/case-id/output only')
            r = check_case(rows_from(a.log), a.case_id)
            r.update(log_path=str(a.log.resolve()), log_sha256=sha(a.log))
        elif a.provider:
            if not a.source or not a.build or a.case_id or a.provider_receipt or a.candidate_log: raise ValueError('Provider mode needs source/build only')
            r = check_provider(a.source, a.build, a.provider)
        else:
            if a.source or a.build or a.case_id or not a.provider_receipt or not a.candidate_log:
                raise ValueError('Aggregate mode requires one report and provider-receipt/output only')
            r = aggregate(a.aggregate, a.provider_receipt, a.candidate_log)
    except (OSError, ValueError, TypeError, KeyError, AttributeError, subprocess.SubprocessError) as exc:
        r = base('frame-old-provider-error'); r['errors'].append(str(exc))
    a.output.write_text(json.dumps(r, indent=2) + '\n')
    return 0 if r['control_passed'] else 1

if __name__ == '__main__':
    raise SystemExit(main())

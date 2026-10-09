#!/usr/bin/env python3
"""Reject pre-main C++ initialization roots in native ELF object inputs.

This is a native, non-LTO object check, not a source-text heuristic or a claim
about initialization performed inside separately supplied shared libraries.
Function-local static guards are intentionally permitted.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


def inspect_object(path, readelf='readelf'):
    path = Path(path)
    result = subprocess.run([readelf, '--wide', '--file-header', '--section-headers',
                             '--symbols', str(path)], capture_output=True, text=True)
    if result.returncode:
        raise ValueError('Cannot inspect object: '+str(path)+'\n'+result.stderr)
    text = result.stdout
    if not re.search(r'Type:\s+REL\b', text):
        raise ValueError('Expected a relocatable ELF object: '+str(path))
    findings, candidates, sections = [], [], []
    pattern = re.compile(r'^\s*\[\s*\d+\]\s+(\S+)\s+(\S+)\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+([0-9a-fA-F]+)\b')
    for line in text.splitlines():
        match = pattern.match(line)
        if not match:
            continue
        name, kind, size_hex = match.groups()
        size = int(size_hex, 16)
        if name.startswith('.gnu.lto_'):
            raise ValueError('LTO object requires a separate post-LTO check: '+str(path))
        sections.append({'name':name, 'type':kind, 'size':size})
        startup = (kind in ('INIT_ARRAY', 'PREINIT_ARRAY') or
                   name in ('.init', '.init_array', '.preinit_array', '.ctors') or
                   name.startswith(('.init_array.', '.preinit_array.', '.ctors.')))
        if size and startup:
            findings.append({'kind':'startup-section', 'name':name, 'bytes':size})
        if size and name.startswith('.text.startup'):
            candidates.append(name)
    if not sections:
        raise ValueError('No parsed ELF sections: '+str(path))
    symbols = []
    for line in text.splitlines():
        match = re.match(r'^\s*\d+:\s+[0-9a-fA-F]+\s+\d+\s+(\S+)\s+\S+\s+\S+\s+(\S+)\s+(.+)$',line)
        if not match:
            continue
        kind, index, name = match.groups()
        if index == 'UND':
            continue
        if kind == 'IFUNC' or name.startswith(('_GLOBAL__sub_I_', '_GLOBAL__I_', '_ZTH')) or \
           '__static_initialization_and_destruction_0' in name or name == '__tls_init':
            findings.append({'kind':'initializer-symbol' if kind!='IFUNC' else 'ifunc-resolver',
                             'name':name, 'symbol_type':kind})
        if name.startswith('_ZGV'):
            symbols.append(name)
    return {'object':str(path), 'bytes':path.stat().st_size,
            'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'findings':findings, 'startup_code_candidates':candidates,
            'lazy_guard_symbols':symbols}



def production_inputs(root, build):
    root, build = Path(root).resolve(), Path(build).resolve()
    cpp = {'.cpp', '.cc', '.cxx'}
    population = {p.resolve() for p in (root/'app').rglob('*')
                  if p.suffix in cpp and 'tests' not in p.relative_to(root).parts}
    http = bool(re.search(r'^#define\s+HAVE_PAINTER_HTTP(?:\s|$)',
                         (build/'config.h').read_text(),re.M))
    expected = {p for p in population if http or not p.is_relative_to(root/'app/httpd')}
    targets = json.loads((build/'meson-info/intro-targets.json').read_text())
    database = json.loads((build/'compile_commands.json').read_text())
    owners = {}
    for target in targets:
        production = ((target['type']=='static library' and target['build_by_default']) or
                      (target['type']=='executable' and target['installed']))
        if not production:
            continue
        for group in target['target_sources']:
            if group.get('language')!='cpp':
                continue
            for source in group.get('sources',[])+group.get('generated_sources',[]):
                source = Path(source).resolve()
                if source.is_relative_to(build/'app') and source.suffix in cpp:
                    raise ValueError('Generated production C++ source needs a reviewed scope')
                if source in population and source not in expected:
                    raise ValueError('Configured HTTP macro disagrees with production targets')
                if source not in expected:
                    continue
                owners.setdefault(source,[]).append(target)
    if set(owners) != expected:
        raise ValueError('Missing production C++ registration: '+str(sorted(map(str,expected-set(owners)))))
    records = []
    for row in database:
        source = (Path(row['directory'])/row['file']).resolve()
        if source not in owners:
            continue
        argv = row.get('arguments') or shlex.split(row['command'])
        output = (Path(row['directory'])/argv[argv.index('-o')+1]).resolve()
        matched = [t for t in owners[source] if any(output.is_relative_to(Path(f+'.p')) for f in t['filename'])]
        if not matched:
            continue  # Same production source compiled separately into a test.
        if len(matched)!=1 or any(a=='-flto' or a.startswith('-flto=') for a in argv):
            raise ValueError('Ambiguous owner or LTO command: '+str(output))
        records.append({'source':str(source.relative_to(root)), 'object':str(output),
                        'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
                        'target':matched[0]['name'], 'target_type':matched[0]['type'],
                        'defined_in':str(Path(matched[0]['defined_in']).relative_to(root)),
                        'command':argv})
    if {root/r['source'] for r in records} != expected or len(records)!=len(expected):
        raise ValueError('Production object coverage is missing or duplicated')
    return records, http


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('objects', nargs='*', type=Path)
    parser.add_argument('--build', type=Path)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--readelf', default='readelf')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    scope = None
    if args.build:
        if args.objects: parser.error('use either --build or explicit object paths')
        scope, http = production_inputs(args.root,args.build)
        args.objects = [Path(r['object']) for r in scope]
    if not args.objects: parser.error('no objects selected')
    records = [inspect_object(path,args.readelf) for path in args.objects]
    passed = not any(row['findings'] for row in records)
    report = {'status':'PASS' if passed else 'FAIL', 'scope':'Native ELF non-LTO C++ object startup roots',
              'objects':records, 'object_count':len(records),
              'findings':sum(len(row['findings']) for row in records)}
    if scope is not None:
        report.update(production_inputs=scope,http_enabled=http,
                      compile_database_sha256=hashlib.sha256((args.build/'compile_commands.json').read_bytes()).hexdigest(),
                      target_metadata_sha256=hashlib.sha256((args.build/'meson-info/intro-targets.json').read_bytes()).hexdigest())
    if args.report:
        args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(report['status']+': '+str(len(records))+' objects, '+str(report['findings'])+' initialization roots')
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()

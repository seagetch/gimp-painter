#!/usr/bin/env python3
"""Generate build-only C/C++ header probes from the reviewed source boundaries."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
C_SCOPE = 'migration/inventory/c-header-linkage.json'
CPP_SCOPE = 'migration/inventory/cpp-header-boundary.json'
PRELUDE = 'migration/tests/painter-headers/prologue.h'
MACROS = 'migration/tests/painter-headers/config-macros.c'
VISIBILITY = 'app/painter/gimp-painter-visibility.h'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def include(path):
    return '#include "'+path+'"\n'


def specification(root, http, scope=None, private_scope=None):
    # Explicit File arguments let Meson track the metadata used to enumerate
    # generated output names and prerequisite files at configure time.
    for given, expected in [(scope, C_SCOPE), (private_scope, CPP_SCOPE)]:
        if given is not None and given.resolve() != (root/expected).resolve():
            raise ValueError('unexpected reviewed scope path: '+str(given))
    c_scope = json.loads((root/C_SCOPE).read_text())
    cpp_scope = json.loads((root/CPP_SCOPE).read_text())
    if len(c_scope['routes']) != 103:
        raise ValueError('missing or additional reviewed C-header route')
    if len({row['route'] for row in c_scope['routes']}) != len(c_scope['routes']):
        raise ValueError('duplicate C-header route')
    legacy = {}
    for row in c_scope['legacy_work_rows']:
        if row['path'].endswith('.h'):
            legacy.setdefault(row['path'], set()).add(row['work_id'])
    assigned = {row['route']:set(row['work_ids']) for row in c_scope['routes']
                if row['work_ids']}
    if assigned != legacy:
        raise ValueError('reviewed legacy header source duties are missing or reassigned')
    if len(cpp_scope['header_mappings']) != 11:
        raise ValueError('missing or additional private-header mapping')
    if len({row['legacy_header'] for row in cpp_scope['header_mappings']}) != len(cpp_scope['header_mappings']):
        raise ValueError('duplicate private-header mapping')
    targets = sorted({row['target'] for row in c_scope['routes']} | {VISIBILITY})
    if len(targets) != 102 or any(not p.endswith('.h') for p in targets):
        raise ValueError('review the C-header boundary before changing its target set')
    private = sorted({p for row in cpp_scope['header_mappings']
                      for p in row['current_counterparts'] if p.endswith('.hpp')})
    if len(private) != 21:
        raise ValueError('private-header registration mapping changed')
    active_private = [p for p in private if http or not p.startswith('app/httpd/')]
    if len(active_private) != (21 if http else 19):
        raise ValueError('unexpected optional HTTP header set')
    prelude = (root/PRELUDE).read_text()
    adapter = '#ifdef __cplusplus\nextern "C" {\n#endif\n'
    if prelude.count(adapter) != 1:
        raise ValueError('historical prelude boundary changed')
    prelude = prelude.replace(adapter, '')
    if 'extern "C"' in prelude or 'G_BEGIN_DECLS' in prelude:
        raise ValueError('probe must not supply an outer C-linkage wrapper')
    probes = []
    def add(label, header, body, languages=('c', 'cpp')):
        for language in languages:
            guard = ('#ifndef __cplusplus\n#error C++ compiler required\n#endif\n'
                     if language == 'cpp' else
                     '#ifdef __cplusplus\n#error C compiler required\n#endif\n')
            probes.append({'name':'probe-%03d.%s' % (len(probes), language),
                           'label':label, 'header':header, 'language':language,
                           'body':guard+body})
    for target in targets:
        add('public-header', target, prelude+include(target)*2)
    for target in ['app/xcf/painter-xcf-load.h', 'app/xcf/painter-xcf-preserve.h']:
        add('xcf-private-context', target,
            prelude+include('app/xcf/xcf-private.h')+include(target)*2)
    add('configuration-macros', None, prelude+(root/MACROS).read_text())
    add('visibility-without-prelude', VISIBILITY, include(VISIBILITY)*2+
        '#ifdef __cplusplus\nnamespace Probe GIMP_PAINTER_PRIVATE {}\n'
        'extern "C" GIMP_PAINTER_C_ENTRY void probe_entry (void);\n#endif\n')
    for target in active_private:
        add('private-cpp-header', target, prelude+include(target)*2, ('cpp',))
    baseline = {row['path']:row for row in c_scope['all_baseline_app_headers']}
    closure = {row['path'] for row in c_scope['headers']} | set(targets)
    for header in sorted((root/'app').rglob('*.h')):
        if 'tests' in header.relative_to(root).parts:
            continue
        path = str(header.relative_to(root))
        if path not in baseline and path not in targets:
            raise ValueError('new production C header needs a reviewed compile route: '+path)
        data = header.read_bytes()
        blob = hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        if path in baseline and blob != baseline[path]['baseline_blob'] and path not in closure:
            raise ValueError('changed C header is outside the reviewed closure: '+path)
    inputs = {C_SCOPE, CPP_SCOPE, PRELUDE, MACROS, VISIBILITY,
              str(Path(__file__).resolve().relative_to(ROOT))}
    inputs.update(closure); inputs.update(active_private)
    records = [{'name':p['name'], 'label':p['label'], 'header':p['header'],
                'language':p['language'], 'source_sha256':sha(p['body'].encode())}
               for p in probes]
    manifest = {'format':1, 'scope':'C11/C++14 header compilation; no runtime or link ABI claim',
                'http_enabled':http, 'public_targets':targets,
                'private_cpp_targets':active_private, 'probes':records,
                'outer_linkage_adapter':False,
                'source_sha256':{p:sha((root/p).read_bytes()) for p in sorted(inputs)}}
    return probes, manifest


def write_if_changed(path, content):
    if not path.is_file() or path.read_bytes() != content:
        path.write_bytes(content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--http', choices=('0','1'), default='0')
    parser.add_argument('--scope', type=Path)
    parser.add_argument('--private-scope', type=Path)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--list-inputs', action='store_true')
    parser.add_argument('--output-dir', type=Path)
    parser.add_argument('--verify-build', type=Path)
    parser.add_argument('--library', type=Path, action='append', default=[])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    probes, manifest = specification(args.root.resolve(), args.http == '1',
                                     args.scope, args.private_scope)
    if args.list_inputs:
        print('\n'.join(manifest['source_sha256']))
        return
    if args.list:
        print('\n'.join([p['name'] for p in probes]+['header-probes.json']))
        return
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        for probe in probes:
            write_if_changed(args.output_dir/probe['name'], probe['body'].encode())
        write_if_changed(args.output_dir/'header-probes.json',
                         (json.dumps(manifest, indent=2)+'\n').encode())
    if args.verify_build:
        saved = json.loads((args.verify_build/'header-probes.json').read_text())
        if saved != manifest:
            raise ValueError('header probe inputs changed; rebuild the registered compile targets')
        for probe in probes:
            if (args.verify_build/probe['name']).read_text() != probe['body']:
                raise ValueError('generated probe differs: '+probe['name'])
        if len(args.library) != 2 or any(not p.is_file() or p.stat().st_size == 0 for p in args.library):
            raise ValueError('both C and C++ compile libraries must be built')
        result = {'status':'PASS', **manifest,
                  'compiled_probes':len(probes),
                  'c_probes':sum(p['language']=='c' for p in probes),
                  'cpp_probes':sum(p['language']=='cpp' for p in probes),
                  'libraries':{str(p):{'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())}
                               for p in args.library}}
        if args.report:
            args.report.write_text(json.dumps(result, indent=2)+'\n')
        print('PASS: %d C and %d C++ header compile probes' % (result['c_probes'],result['cpp_probes']))


if __name__ == '__main__':
    main()

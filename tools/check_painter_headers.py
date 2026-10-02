#!/usr/bin/env python3
"""Compile every inventoried existing C-header route under C and C++14."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
PROBES = ROOT / 'migration/tests/painter-headers'
ENDING = '\n#ifdef __cplusplus\n}\n#endif\n'


def routes():
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as source:
        paths = sorted({r['path'] for r in csv.DictReader(source, delimiter='\t')
                        if r['wbs_task'] == '04.003' and r['path'].endswith('.h')})
    result = []
    for path in paths:
        target = path
        reason = 'existing app header; required app type prelude plus C linkage'
        if path == 'app/config/gimpbaseconfig.h':
            target = 'app/config/gimpgeglconfig.h'
            reason = 'legacy base-config type removed; modern core config inherits GimpGeglConfig; field/settings migration remains separate'
        elif path == 'libgimpcolor/gimpcairocolor.h':
            target = 'libgimpcolor/gimpcolor.h'
            reason = 'old Cairo RGB header removed; current Cairo buffer/color APIs through color/widgets umbrellas; numeric feature equivalence not claimed'
        elif path.startswith('libgimpconfig/'):
            target = 'libgimpconfig/gimpconfig.h'
            reason = 'public direct-inclusion guard requires umbrella; do not bypass guard'
        elif path.startswith('libgimpwidgets/'):
            target = 'libgimpwidgets/gimpwidgets.h'
            reason = 'public direct-inclusion guard requires umbrella; do not bypass guard'
        result.append((path, target, reason))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build-debian13')
    parser.add_argument('--report', type=Path, default=ROOT / 'migration/tests/painter-headers.json')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    cflags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', 'gegl-0.4', 'gtk+-3.0', 'gexiv2'], text=True))
    flags = ['-Wall', '-Wextra', '-Werror', '-Wno-error=ignored-qualifiers',
             '-I'+str(ROOT), '-I'+str(ROOT/'app'), '-I'+str(build)] + cflags
    prelude = (PROBES / 'prologue.h').read_text()
    results = []

    def compile_probe(label, body, language, directory, execute=False):
        cpp = language == 'c++'
        compiler = shlex.split(os.environ.get('CXX' if cpp else 'CC', 'c++' if cpp else 'cc'))
        command = compiler + ['-std=c++14' if cpp else '-std=c11'] + flags
        executable = directory / ('probe-cpp' if cpp else 'probe-c')
        command += ([] if execute else ['-fsyntax-only']) + ['-x', language, '-']
        if execute:
            command += ['-o', str(executable)]
        completed = subprocess.run(command, input=body, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        record = dict(probe=label, language=language, command=command,
                      source_sha256=hashlib.sha256(body.encode()).hexdigest(),
                      exit_code=completed.returncode, output=completed.stdout)
        results.append(record)
        if completed.returncode:
            raise RuntimeError(label+' '+language+'\n'+completed.stdout)
        if execute:
            record['runtime_output'] = subprocess.check_output([str(executable)], text=True)
        return record

    with tempfile.TemporaryDirectory(prefix='painter-headers-') as tmp:
        directory = Path(tmp)
        for path, target, reason in routes():
            include = target.removeprefix('app/')
            body = prelude + f'\n#include "{include}"\n#include "{include}"\n' + ENDING
            for language in ('c', 'c++'):
                record = compile_probe(path, body, language, directory)
                record.update(target_header=target, reason=reason)
            print('header route OK:', path, flush=True)
        macros = prelude + (PROBES / 'config-macros.c').read_text() + ENDING
        for language in ('c', 'c++'):
            compile_probe('all18 config-property macros', macros, language, directory)
        layout = prelude + '''
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "display/gimpcanvasitem.h"
''' + ENDING + '''
#include <stddef.h>
#include <stdio.h>
int main(void) {
#ifdef __cplusplus
#define TEMPLATE_MEMBER template_object
#define PRIVATE_MEMBER priv
#else
#define TEMPLATE_MEMBER template
#define PRIVATE_MEMBER private
#endif
printf("%zu %zu %zu %zu %zu %zu %zu %zu\\n",
       sizeof(GimpContext), offsetof(GimpContext, TEMPLATE_MEMBER),
       sizeof(GimpDrawable), offsetof(GimpDrawable, PRIVATE_MEMBER),
       sizeof(GimpCanvasItem), offsetof(GimpCanvasItem, PRIVATE_MEMBER),
       sizeof(GimpContextClass), offsetof(GimpContextClass, template_changed));
return 0;
}
'''
        c = compile_probe('C/C++ field layout', layout, 'c', directory, True)
        cpp = compile_probe('C/C++ field layout', layout, 'c++', directory, True)
        if c['runtime_output'] != cpp['runtime_output']:
            raise RuntimeError('C/C++ struct layout differs')
    report = dict(status='PASS', scope='header syntax, macro expansion and selected native-ABI layouts only',
                  routes=len(routes()), probes=len(results),
                  compile_results=results,
                  limitations=['No feature behavior, Windows/macOS ABI or all added module completion claimed',
                               'Internal headers require the documented real app prelude and extern C adapter boundary',
                               'Existing upstream ignored scalar-return qualifier warning retained, not treated as a new error'])
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2)+'\n')
    print(f"PASS: {len(routes())} routes, {len(results)} C/C++ probes")


if __name__ == '__main__':
    main()

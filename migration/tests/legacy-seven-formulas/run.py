#!/usr/bin/env python3
"""Compare the unchanged old RGBA functions with the production byte kernel.
No old application, Edge operation, GUI or shared build directory is executed.
"""
import argparse, hashlib, json, pathlib, shlex, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[3]
OLD = 'afa43fae3e920210146abed514f136fd49f671b5'
p = argparse.ArgumentParser()
p.add_argument('--report', type=pathlib.Path, required=True)
p.add_argument('--sanitize', action='store_true')
a = p.parse_args()
def old(path):
    return subprocess.check_output(['git','show',f'{OLD}:{path}'],cwd=ROOT,text=True)
def function(source, name):
    start = source.index(name + ' (')
    opening = source.index('{',start)
    level = 1
    end = opening + 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
paint = old('app/paint-funcs/paint-funcs.c')
composite = old('app/composite/gimp-composite-generic.c')
utils = old('app/paint-funcs/paint-funcs-utils.h')
production = (ROOT/'app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c').read_text()
parts = {name: function(paint,name) for name in ['replace_inten_pixels','erase_inten_pixels','anti_erase_inten_pixels']}
parts.update({name: function(composite,name) for name in ['gimp_composite_src_in_any_any_any_generic','gimp_composite_src_out_any_any_any_generic']})
# Original helpers and kernel, not a second reimplementation of the new math.
kernel = production[production.index('static gint mult ('):production.index('\ngboolean\ngimp_painter_layer_mode_from_legacy')]
shim = '''
#include <glib.h>
#include <string.h>
#include <stdio.h>
static const guchar no_mask = 255;
#define REPLACE_INTEN 1
typedef struct { guchar *A,*B,*D; gulong n_pixels; guint pixelformat_A,pixelformat_B; int combine; } GimpCompositeContext;
static const guint gimp_composite_pixel_bpp[] = {4};
'''
divmacro = '#define INT_DIV(a,b) ((a)/(b)+(((a)%(b))>((b)/2)))\n'
code = shim + utils + divmacro + '\n'.join('static void\n'+body for body in parts.values()) + '\n' + kernel + (pathlib.Path(__file__).with_name('compare.inc')).read_text()
with tempfile.TemporaryDirectory(prefix='painter-seven-formulas-') as directory:
    directory = pathlib.Path(directory)
    source = directory/'compare.c'; exe=directory/'compare'
    source.write_text(code)
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
    command=['cc','-std=c11','-O2','-g0',str(source),'-o',str(exe)]+flags
    if a.sanitize: command += ['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run(command,check=True)
    import os
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
    result=subprocess.run([str(exe)],text=True,capture_output=True,env=env)
    report={'old_commit':OLD,'scope':'source-extracted unchanged old RGBA functions versus source-extracted production kernel; not GUI/full-app proof','sanitizers':a.sanitize,'function_sha256':{name:hashlib.sha256(body.encode()).hexdigest() for name,body in parts.items()},'legacy_source_sha256':{name:hashlib.sha256(text.encode()).hexdigest() for name,text in [('app/paint-funcs/paint-funcs.c',paint),('app/composite/gimp-composite-generic.c',composite),('app/paint-funcs/paint-funcs-utils.h',utils)]},'production_sha256':hashlib.sha256(production.encode()).hexdigest(),'test_sha256':hashlib.sha256(pathlib.Path(__file__).with_name('compare.inc').read_bytes()).hexdigest(),'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
    a.report.parent.mkdir(parents=True,exist_ok=True)
    a.report.write_text(json.dumps(report,indent=2)+'\n')
    print(result.stdout,end='');print(result.stderr,end='')
    raise SystemExit(result.returncode)

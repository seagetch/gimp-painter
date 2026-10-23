#!/usr/bin/env python3
"""Execute production-extracted enum/constant binding code in actual TinyScheme.

This deliberately does not launch a plug-in/PDB session: no GUI or whole Script-Fu
startup claim. All registration bodies/table and interpreter C sources are exact
current repository inputs; the supplied built typelib is separately inventoried.
"""
import argparse, hashlib, json, os, pathlib, re, shlex, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',required=True);p.add_argument('--output',required=True);p.add_argument('--sanitize',action='store_true');args=p.parse_args()
build=pathlib.Path(args.build).resolve();out=pathlib.Path(args.output);out.mkdir(parents=True,exist_ok=True)
wrapper=ROOT/'plug-ins/script-fu/libscriptfu/scheme-wrapper.c';s=wrapper.read_text()
table=s[s.index('static const NamedConstant script_constants[] ='):];table=table[:table.index('\n};')+3]
start=s.index('ts_init_enum (scheme');start=s.rfind('static void',0,start)
end=s.index('\n/* Define a symbol into interpreter state',start);enums=s[start:end]
start=s.index('  for (int i = 0; script_constants[i].name');end=s.index('\n  /* use to build paths',start);loop=s[start:end]
names=['ERASE','REPLACE','ANTI-ERASE','SRC-IN','DST-IN','SRC-OUT','DST-OUT']
# C string escaping generated centrally instead of interpolating Scheme quoting.
def cstr(x):return json.dumps(x)
pre=''.join('check(sc,'+cstr(f"(if (defined? '{n}) (error \"alias collision\") #t)")+');\n' for n in names)
post=''.join('check(sc,'+cstr(f'(if (= {n} {64+i}) #t (error "wrong legacy alias"))')+');\n' for i,n in enumerate(names))
post+=''.join('check(sc,'+cstr(f'(if (= LAYER-MODE-PAINTER-{n} {64+i}) #t (error "wrong GI identity"))')+');\n' for i,n in enumerate(names))
post+=''.join('check(sc,'+cstr(f'(if (= LAYER-MODE-{n} {v}) #t (error \"modern mode changed\"))')+');\n' for n,v in [('ERASE',58),('REPLACE',62)])
source='''#include "config.h"
#include <glib.h>
#include <girepository.h>
#include "libgimp/gimp.h"
#include "plug-ins/script-fu/libscriptfu/script-fu-enums.h"
#include "plug-ins/script-fu/libscriptfu/tinyscheme/scheme-private.h"
typedef struct { const gchar *name; gint value; } NamedConstant;
'''+table+'\n'+enums+'''
static int failures;
static void check(scheme *sc,const char *expr) {
 sc->retcode=0; scheme_load_string(sc,expr);
 if(sc->retcode){fprintf(stderr,"FAILED %s\\n",expr); failures++;}
}
int main(void){
 scheme *sc=scheme_init_new();g_assert_nonnull(sc);
 scheme_set_output_port_file(sc,stdout);
 GIRepository *repo=g_irepository_get_default();GError *error=NULL;
 g_assert_nonnull(g_irepository_require(repo,"Gimp","3.0",0,&error));
 ts_init_enums(sc,repo,"Gimp");
'''+pre+loop+'\n'+post+'''
 printf("aliases=7 enum-identities=7 no-GI-alias-collisions=7 modern-identities=2 failures=%d\\n",failures);
 scheme_deinit(sc);g_free(sc);return failures?1:0;
}
'''
files=[wrapper,ROOT/'libgimp/gimpenums.h']+[ROOT/'plug-ins/script-fu/libscriptfu/tinyscheme'/n for n in ['scheme.c','string-port.c','error-port.c','scheme.h','scheme-private.h','opdefines.h']]
typelib=build/'libgimp/Gimp-3.0.typelib';files.append(typelib)
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gimp-3.0','gobject-introspection-1.0'],text=True))
with tempfile.TemporaryDirectory(prefix='painter-scheme-') as tmp:
 c=pathlib.Path(tmp)/'probe.c';exe=pathlib.Path(tmp)/'probe';c.write_text(source)
 cmd=['cc','-std=c11','-O1','-g','-I'+str(build),'-I'+str(ROOT),'-DSTANDALONE=0','-DUSE_ASCII_NAMES=0','-DUSE_INTERFACE=1','-DUSE_MATH=1','-DUSE_STRLWR=0',str(c),*[str(f) for f in files[2:5]],*flags,'-lm','-o',str(exe)]
 if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 compile=subprocess.run(cmd,text=True,capture_output=True)
 env=dict(os.environ);env['GI_TYPELIB_PATH']=str(build/'libgimp')+':'+env.get('GI_TYPELIB_PATH','');env['LD_LIBRARY_PATH']=':'.join(str(d) for d in build.glob('libgimp*') if d.is_dir())+':'+env.get('LD_LIBRARY_PATH','');env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 run=subprocess.run([str(exe)],text=True,capture_output=True,env=env) if not compile.returncode else None
 (out/'probe.c').write_text(source);(out/'compile.log').write_text(compile.stdout+compile.stderr)
 (out/'run.log').write_text(run.stdout+run.stderr if run else 'NOT RUN\n')
 report={'scope':__doc__,'sanitize':args.sanitize,'compile_exit':compile.returncode,'run_exit':run.returncode if run else None,'source_sha256':{str(f.relative_to(ROOT)) if f.is_relative_to(ROOT) else str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'cases':{'aliases':7,'GI_identity':7,'collision_checks':7,'modern_identities':2},'instrumented':'probe and all three TinyScheme C units; GLib/GI/dependencies uninstrumented; LSan disabled' if args.sanitize else 'normal'}
 (out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));raise SystemExit(compile.returncode or (run.returncode if run else 1))

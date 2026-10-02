#!/usr/bin/env python3
"""Check public/app enum ABI, including holes for hidden upstream modes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
def modes(text):
    end=text.index('} GimpLayerMode;');start=text.rfind('typedef enum',0,end)
    body=re.sub(r'/\*.*?\*/','',text[text.index('{',start)+1:end],flags=re.S)
    result={};value=-1
    for field in body.split(','):
        field=field.strip()
        if not field:continue
        if '=' in field:name,raw=field.split('=',1);value=int(raw.strip(),0)
        else:name=field;value+=1
        result[name.strip()]=value
    return result

def pdb_modes(text):
    section=text.split('    GimpLayerMode =>\n',1)[1].split('\n    GimpConvertDitherType =>',1)[0]
    values={name:int(value) for name,value in re.findall(r"(GIMP_LAYER_MODE_\w+)\s*=>\s*'(-?\d+)'",section)}
    contiguous=int(re.search(r'contig\s*=>\s*(\d+)',section).group(1))
    return values,contiguous

def check_pdb(pdb,public):
    values,contiguous=pdb
    errors=[]
    if values!=public:errors.append('Generated PDB mode table differs from public enum')
    ordered=sorted(public.values())
    expected=int(ordered==list(range(ordered[0],ordered[-1]+1)))
    if contiguous!=expected:errors.append('Generated PDB enum contiguity flag ignores hidden slots')
    return errors

def check(compile_probes=False, runtime_build=None):
    paths=['app/operations/operations-enums.h','libgimp/gimpenums.h']
    app,public=[modes((ROOT/p).read_text()) for p in paths]
    baseline=json.loads((ROOT/'migration/baseline/layer-mode-values.json').read_text())['values']
    pdb=pdb_modes((ROOT/'pdb/enums.pl').read_text())
    errors=check_pdb(pdb,public)
    for name,value in public.items():
        if app.get(name)!=value:errors.append(f'{name}: public={value}, app={app.get(name)}')
    for name,value in baseline.items():
        if app.get(name)!=value:errors.append(f'Upstream ABI changed: {name} {value}->{app.get(name)}')
    if len(set(public.values()))!=len(public):errors.append('Duplicate public mode identity')
    if 63 in public.values():errors.append('Hidden upstream slot63 was exported')
    expected={name:value for name,value in app.items() if '_PAINTER_' in name}
    if len(expected)!=9 or not set(expected)<=public.keys():errors.append('Missing Painter public modes')
    if 'GIMP_LAYER_MODE_ANTI_ERASE' in public:errors.append('Hidden upstream mode leaked into public API')
    probes=[]
    if compile_probes and not errors:
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gobject-2.0'],text=True))
        for header,values in [(paths[0],app),(paths[1],public)]:
            for compiler,language,standard,assertion in [('cc','c','c11','_Static_assert'),('c++','c++','c++14','static_assert')]:
                source='#include <glib-object.h>\n#include "'+header+'"\n'+'\n'.join(f'{assertion}({name}=={value},"{name}");' for name,value in values.items())+'\n'
                command=[compiler,'-std='+standard,'-Wall','-Wextra','-Werror','-I'+str(ROOT),*flags,'-x',language,'-fsyntax-only','-']
                result=subprocess.run(command,input=source,text=True,capture_output=True)
                probes.append(dict(header=header,language=language,command=command,exit_code=result.returncode,stderr=result.stderr))
                if result.returncode:errors.append('Compilation failed: '+header+' '+language)
    runtime=[]
    if runtime_build and not errors:
        build=Path(runtime_build).resolve()
        library=build/'libgimp/libgimp-3.0.so'
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gobject-2.0'],text=True))
        with tempfile.TemporaryDirectory(prefix='painter-public-modes-') as tmp:
            file=Path(tmp)/'probe.c';exe=Path(tmp)/'probe'
            source='#include <glib-object.h>\n#include "libgimp/gimpenums.h"\nint main(void){GEnumClass*c=g_type_class_ref(gimp_layer_mode_get_type());\n'
            for name,value in public.items():source+=f'if(!g_enum_get_value_by_name(c,"{name}") || g_enum_get_value_by_name(c,"{name}")->value!={value})return 1;\n'
            source+='if(g_enum_get_value(c,63))return 2;g_type_class_unref(c);return 0;}\n';file.write_text(source)
            command=['cc','-std=c11','-I'+str(ROOT),str(file),'-L'+str(build/'libgimp'),'-Wl,-rpath,'+str(build/'libgimp'),'-lgimp-3.0',*flags,'-o',str(exe)]
            linked=subprocess.run(command,text=True,capture_output=True)
            env=dict(os.environ);env['LD_LIBRARY_PATH']=str(build/'libgimp')+':'+env.get('LD_LIBRARY_PATH','')
            run=subprocess.run([str(exe)],env=env,text=True,capture_output=True) if linked.returncode==0 else linked
            runtime=dict(command=command,runtime_library_path=env['LD_LIBRARY_PATH'],library=str(library.resolve()),library_sha256=hashlib.sha256(library.read_bytes()).hexdigest(),link_exit=linked.returncode,exit_code=run.returncode,stdout=run.stdout,stderr=linked.stderr+run.stderr)
            if linked.returncode or run.returncode:errors.append('Native public GType enum values differ')
    return dict(scope='Public C/C++ Painter mode identity against native app and unchanged upstream values; generated PDB table verified, not PDB transport/pixel acceptance',source_sha256={p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths+['migration/baseline/layer-mode-values.json','tools/check_painter_public_modes.py','pdb/enumgen.pl','pdb/enumcode.pl','pdb/enums.pl']},public_modes=len(public),generated_pdb_modes=len(pdb[0]),generated_pdb_contiguous=pdb[1],upstream_modes=len(baseline),painter_modes=expected,errors=errors,probes=probes,runtime=runtime,passed=not errors)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--compile',action='store_true');p.add_argument('--report',type=Path);p.add_argument('--runtime-build',type=Path);a=p.parse_args();r=check(a.compile,a.runtime_build)
    if a.report:a.report.write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps(r,indent=2));raise SystemExit(not r['passed'])

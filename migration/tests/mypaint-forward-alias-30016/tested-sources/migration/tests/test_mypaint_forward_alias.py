#!/usr/bin/env python3
"""Compile original opaque options name in C11/C++14 and both include orders.

Uses native compile database flags, isolated header overlays and no linking.
Caller owns build lock. Baseline header and wrong-tag mutant must fail.
"""
import argparse,pathlib,json,shlex,subprocess,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--root',type=pathlib.Path,required=True);p.add_argument('--baseline-header',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();R=a.root.resolve();O=a.output.resolve();O.mkdir(exist_ok=True,parents=True)
entries=json.loads((B/'compile_commands.json').read_text());rows=[]
current=(R/'app/paint/paint-types.h').read_text();old=a.baseline_header.read_text();assert 'GimpMypaintOptions;' not in old
prelude='#include "config.h"\n#include <gegl.h>\n#include <gtk/gtk.h>\n'
for variant,text in [('baseline',old),('current',current),('wrong-tag',current.replace('struct _GimpPainterMybrushOptions   GimpMypaintOptions','struct _WrongMypaintOptions         GimpMypaintOptions'))]:
 D=O/variant;(D/'paint').mkdir(parents=True,exist_ok=True);(D/'paint/paint-types.h').write_text(text)
 # Exact existing committed shim; native dependencies stay in the clean source.
 (D/'paint/gimpmypaintoptions.h').write_bytes((R/'app/paint/gimpmypaintoptions.h').read_bytes())
 for language in ['c','cpp']:
  entry=next(e for e in entries if e['file'].endswith('/test-paint-registration.'+language))
  for order in ['opaque-only','forward-first','shim-first']:
   body=prelude
   if order=='opaque-only':body+='#include "paint/paint-types.h"\n#include "paint/paint-types.h"\nGimpMypaintOptions *opaque_options;\n'
   else:
    headers=['paint/paint-types.h','paint/gimpmypaintoptions.h']
    if order=='shim-first':headers.reverse()
    body+=''.join('#include "'+h+'"\n' for h in headers*2)
    if language=='cpp':body+='#include <type_traits>\nstatic_assert(std::is_same<GimpMypaintOptions,GimpPainterMybrushOptions>::value,"one native options type");\n'
    else:body+='_Static_assert(__builtin_types_compatible_p(GimpMypaintOptions,GimpPainterMybrushOptions),"one native options type");\n'
   source=D/(order+'.'+language);source.write_text(body);raw=shlex.split(entry['command']);cmd=[];skip=False
   for item in raw:
    if skip:skip=False;continue
    if item in ['-o','-MF','-MT','-MQ']:skip=True;continue
    if item in ['-MD','-MMD','-c']:continue
    if item==entry['file']:continue
    cmd.append(item)
   native_root=(pathlib.Path(entry['directory'])/entry['file']).resolve().parents[2]
   cmd[1:1]=['-I'+str(D),'-I'+str(native_root/'app/paint')];cmd+=['-fsyntax-only',str(source)]
   q=subprocess.run(cmd,cwd=B,capture_output=True,text=True,timeout=60)
   expected=variant=='current' or (variant=='baseline' and order!='opaque-only') or (variant=='wrong-tag' and order=='opaque-only')
   rows.append(dict(variant=variant,language=language,order=order,returncode=q.returncode,expected_success=expected,stdout=q.stdout,stderr=q.stderr,command=cmd));(O/'results.json').write_text(json.dumps(rows,indent=2)+'\n');assert (q.returncode==0)==expected,(variant,language,order,q.stderr)
print('PASS: 18 C11/C++14 opaque/include-order probes; baseline missing alias and wrong-tag negative controls')

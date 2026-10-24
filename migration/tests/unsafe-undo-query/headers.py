from pathlib import Path
import json,subprocess,shlex
W=Path('/workspace/shared/unsafe-undo-query');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';C=json.loads((B/'compile_commands.json').read_text());out=[]
for language,suffix,source,std in [('c','c','../source/app/core/gimpimage.c','c11'),('c++','cpp','../source/app/paint/painter-mypaint-surface/paint-core.cpp','c++14')]:
 for twice in [False,True]:
  p=W/('header-'+language.replace('+','x')+str(int(twice))+'.'+suffix);p.write_text('#include "config.h"\n#include <gio/gio.h>\n#include <gdk-pixbuf/gdk-pixbuf.h>\n#include <gegl.h>\n'+('extern "C" {\n' if language=='c++'else '')+'#include "core/core-types.h"\n#include "core/gimpimage.h"\n'+('#include "core/gimpimage.h"\n'if twice else '')+('}\n'if language=='c++'else '')+'gboolean (*unsafe_query)(GimpImage *) = gimp_image_has_unsafe_undo;\n')
  c=shlex.split(next(x['command']for x in C if x['file']==source));
  for flag in ['-o','-MF','-MQ']:
   i=c.index(flag);del c[i:i+2]
  c=[x for x in c if x not in ['-MD','-c'] and not x.startswith('-std=')];c[-1]=str(p);c[1:1]=['-I'+str(W/'source/app'),'-iquote',str(S/'app/core'),'-std='+std,'-fsyntax-only'];r=subprocess.run(c,cwd=B,capture_output=True,text=True);out.append({'language':language,'double_include':twice,'exit':r.returncode,'command':c,'stdout':r.stdout,'stderr':r.stderr});assert r.returncode==0,r.stderr
(W/'headers.json').write_text(json.dumps(out,indent=2)+'\n');print('4 C11/C++14 first/double probes PASS')

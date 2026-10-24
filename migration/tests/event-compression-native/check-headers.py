from pathlib import Path
import subprocess,json,shlex,hashlib
O=Path(__file__).resolve().parent;S=Path('/workspace/shared/gimp-clean-aggregate/source');B=S.parent/'build'
pre=(S/'migration/tests/painter-headers/prologue.h').read_text()+'\n#ifdef __cplusplus\n}\n#endif\n'
body=pre+'#include "display/gimpdisplayshell.h"\n#include "display/gimpdisplayshell-tool-events.h"\nvoid probe(GimpDisplayShell *shell) { gimp_display_shell_restore_motion_compression(shell); }\n'
flags=['-I'+str(O/'overlay/app'),'-I'+str(B),'-I'+str(S),'-I'+str(S/'app')]+shlex.split(subprocess.check_output(['pkg-config','--cflags','gegl-0.4','gtk+-3.0','gexiv2'],text=True));results=[]
for lang,cc,std in [('c','cc','c11'),('cpp','c++','c++14')]:
 p=O/('headers.'+lang);p.write_text(body);out=O/('headers-'+lang+'.o');cmd=[cc,'-std='+std,'-Wall','-Wextra','-Werror','-Wno-error=ignored-qualifiers','-c',*flags,str(p),'-o',str(out)];r=subprocess.run(cmd,capture_output=True,text=True);assert r.returncode==0,r.stderr
 nm=subprocess.check_output(['nm','-u',str(out)],text=True);assert ' U gimp_display_shell_restore_motion_compression\n' in nm,nm;results.append({'language':lang,'command':cmd,'unresolved':nm,'compiler_stderr':r.stderr})
(O/'headers.json').write_text(json.dumps({'results':results,'scope':'two owned headers native C11/C++14 no outer linkage adapter; symbol unmangled, runtime link pending'},indent=2)+'\n');print('PASS owned headers C11/C++14; unmangled new restore symbol')

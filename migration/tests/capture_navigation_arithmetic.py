#!/usr/bin/env python3
"""Reproduce source-arithmetic oracle; deliberately not a GUI event capture."""
from pathlib import Path
import subprocess,hashlib,json,argparse,tempfile
parser=argparse.ArgumentParser();parser.add_argument('--legacy',type=Path,required=True);args=parser.parse_args()
root=Path(__file__).resolve().parents[2];legacy=args.legacy;out=root/'migration/fixtures/legacy-navigation'
source=legacy/'app/display/gimpdisplayshell-tool-events.c';text=source.read_text();blob=subprocess.check_output(['git','-C',str(legacy),'show','afa43fae3e920210146abed514f136fd49f671b5:app/display/gimpdisplayshell-tool-events.c']);assert blob==source.read_bytes()
def extract(name):
 start=text.index('static void\n'+name+' ');brace=text.index('{',start);level=1;end=brace+1
 while level:
  if text[end]=='{':level+=1
  elif text[end]=='}':level-=1
  end+=1
 return text[start:end]
functions='\n\n'.join(extract(n) for n in ['gimp_display_shell_start_rotating','gimp_display_shell_rotate'])
pre='''/* Extracted unchanged functions; GTK grab/cursor/expose are no-op stubs.
 * This is executable source-arithmetic evidence, not an actual GUI event run. */
#include <math.h>
#include <stdio.h>
#include <assert.h>
typedef double gdouble; typedef int gint;
typedef struct { unsigned state; } GdkEvent, GdkEventMotion;
typedef struct { int rotating, disp_width, disp_height, mirrored; double rotate_angle, rotate_start_angle; } GimpDisplayShell;
#define TRUE 1
#define GDK_CONTROL_MASK 4
#define GDK_POINTER_MOTION_MASK 0
#define GDK_EXCHANGE 0
#define ROTATE_SNAP_UNIT 15.0
#define g_return_if_fail(x) assert(x)
#define gimp_display_shell_pointer_grab(...) ((void)0)
#define gimp_display_shell_set_override_cursor(...) ((void)0)
#define gimp_display_shell_expose_full(...) ((void)0)
'''
main='''
int main(void) {
 const double angles[]={0,7.5,22.5,337.5,352.5,359.999};
 const int points[][2]={{0,0},{40,0},{0,40},{-40,0},{0,-40},{23,37},{-23,37},{-23,-37},{23,-37}};
 const unsigned states[]={0,1,4,5,13,8};
 for(int w=100;w<=101;w++) for(int mirror=0;mirror<2;mirror++)
 for(unsigned a=0;a<6;a++) for(unsigned s=0;s<4;s++) {
  GimpDisplayShell shell={0,w,w+12,mirror,angles[a],0};
  int sx=w/2+points[s][0],sy=(w+12)/2+points[s][1];
  gimp_display_shell_start_rotating(&shell,NULL,sx,sy);
  for(unsigned p=0;p<9;p++) for(unsigned m=0;m<6;m++) {
   GdkEvent event={states[m]}; int x=w/2+points[p][0],y=(w+12)/2+points[p][1];
   gimp_display_shell_rotate(&shell,&event,x,y);
   printf("%d\\t%d\\t%d\\t%.17g\\t%d\\t%d\\t%d\\t%d\\t%u\\t%.17g\\n",w,w+12,mirror,angles[a],sx,sy,x,y,states[m],shell.rotate_angle);
  }
 }
 return 0;
}
'''
harness=out/'capture.c';harness.write_text(pre+functions+main)
with tempfile.TemporaryDirectory(prefix='painter-navigation-') as tmp:
 executable=str(Path(tmp)/'capture')
 subprocess.run(['cc','-std=gnu11',str(harness),'-lm','-o',executable],check=True)
 result=subprocess.check_output([executable])
(out/'rotation.tsv').write_bytes(result)
manifest={'source_commit':'afa43fae3e920210146abed514f136fd49f671b5','source_path':'app/display/gimpdisplayshell-tool-events.c','source_sha256':hashlib.sha256(blob).hexdigest(),'scope':'unchanged extracted rotation functions with GTK side-effect stubs; not GUI/device runtime','records':len(result.splitlines()),'command':['cc','-std=gnu11','capture.c','-lm','-o','capture'],'files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [harness,out/'rotation.tsv']}}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(manifest['records'])

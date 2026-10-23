#!/usr/bin/env python3
import hashlib,json,pathlib,re,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
def outside_extents(s):
 a=s.index('static cairo_region_t *\ngimp_canvas_handle_get_extents (GimpCanvasItem *item)\n{');b=s.index('\nstatic gboolean\ngimp_canvas_handle_hit',a)
 return s[:a],s[b:]
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text())
 require(m['task']=='25.009/handle-damage' and m['parent_closed'] is False and m['original_work_rows_closed']==0,'scope')
 require(m['old_runtime']=='USER-SKIPPED' and m['oncanvas'],'runtime/oncanvas')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build policy')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 old=(e/'handle-red-source.c.txt').read_bytes();require(old==subprocess.check_output(['git','show',m['baseline']+':app/display/gimpcanvashandle.c'],cwd=R),'immutable old handle')
 current=(root/'app/display/gimpcanvashandle.c').read_text();require(outside_extents(current)==outside_extents(old.decode()),'draw/hit unchanged')
 require('RINT (scaled_x) - shell->offset_x > G_MAXINT' in current and 'x2 - x1 > G_MAXINT - 4.0' in current,'bounded admission')
 for kind in ['normal','sanitizer']:
  for group,count in [('geometry',1),('admission',1),('bounds',13),('window-0',1),('window-1',1),('window-2',1)]:
   r=json.loads((e/(kind+'-'+group+'.json')).read_text())
   require(r['result']=='OK' and r['returncode']==0 and r['duration']<60 and r['test_budget_seconds']==60,'result/budget')
   require(r['source_sha256']==m['source_sha256'] and r['xvfb']==m['xvfb'],'tested source/display')
   lines=[x for x in r['stdout'].splitlines() if x.startswith('ok ') and '# SKIP' not in x]
   require(r['cases']==count and len(lines)==count and all('/handle-damage/'+group in x for x in lines),'TAP')
   if group=='geometry':require(r['stdout'].count('# item geometry family=handle ')==156,'raster scenes')
   if group=='bounds':
    require(all(f'# handle type={t} local-bounds scenes=648' in r['stdout'] for t in range(13)),'8424 bounds scenes')
   if group.startswith('window'):
    cases=[x for x in r['stdout'].splitlines() if x.startswith('# real item damage family=handle ')]
    require(len(cases)==52 and all(re.search(r'visible=[1-9][0-9]* residual=0$',x) for x in cases),'real window residuals')
 red=json.loads((e/'real-handle-trails-red.json').read_text());require(red['result']=='FAIL' and red['returncode']==-6 and 'variant=3 visible=443 residual=129' in red['stdout'],'circle red')
 require(sha((e/'handle-real-red-test.inc.txt').read_bytes())==red['test_sha256'],'circle red test')
 red=json.loads((e/'old-handle-local-bounds-red.json').read_text());require(red['result']=='FAIL' and 'local-angle=0.63' in red['stdout'] and 'ink=386,186 47x45 damage=382,187 54x43' in red['stdout'],'square angle red')
 for p,h in red['sources'].items():require(sha((e/'old-handle-local-bounds-red-source'/p).read_bytes())==h,'local red source')
 require((e/'old-handle-local-bounds-red-source/app/display/gimpcanvashandle.c').read_bytes()==old,'old native bounds')
 audit=json.loads((e/'old-handle-angle-audit.json').read_text())
 for a in audit['sources']:
  raw=subprocess.check_output(['git','show',a['revision']+':'+a['path']],cwd=R);b=b''.join(raw.splitlines(True)[a['start']-1:a['end']]);require(b==(e/a['snapshot']).read_bytes() and sha(b)==a['sha256'],'old angle excerpt')
 for name in ['item-geometry-handle-shown.png','item-geometry-handle-hidden.png']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'screenshot')
 return dict(status='PASS',normal_cases=18,sanitizer_cases=18,raster_scenes=156,bounds_scenes=8424,window_scenes=156,parent_closed=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))

#!/usr/bin/env python3
"""Validate real portrait output and exact test-only checkpoint receipts."""
from pathlib import Path
import argparse,hashlib,json,re,tarfile,subprocess,tempfile,copy
P=Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
def check(b,m):
 if not b:raise ValueError(m)
def result(j,name):
 check(j['exit_code']==0,'runtime failure')
 check(re.findall(r'^ok \d+ (\S+)',j['stdout'],re.M)==['/painter-mode-ui/filter-'+name+'-portrait'],'wrong selector')
 for text in ['1080x1920x24, authenticated accepted, empty authority rejected','Actual window 1040x1800, root1080x1920,oncanvas visible','full120000 projection/source/native pixels match independent byte oracle']:
  check(text in j['stdout'],text)
 check(j['stdout'].count('Actual window 1040x1800')==2,'two states')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check-source',action='store_true');a=ap.parse_args()
 artifacts=json.loads((P/'artifact-sha256.json').read_text());check(set(artifacts)=={str(p.relative_to(P))for p in P.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts},'artifact set')
 for n,h in artifacts.items():check(sha((P/n).read_bytes())==h,n)
 seal=json.loads((P/'seal.json').read_text());owned=json.loads((P/'owned.json').read_text());sources=json.loads((P/'source-sha256.json').read_text());baseline=json.loads((P/'baseline-source-sha256.json').read_text());results=json.loads((P/'results.json').read_text());images=json.loads((P/'png-oracle.json').read_text())
 check(seal['base_commit']=='0a775df4135052a2c851c9a5dfa37cea25887268' and not seal['changed']and seal['only_test_overlay']==owned,'exact source/overlay')
 check(set(owned)=={'app/tests/test-painter-mode-ui.c','app/tests/test-filter-compat-portrait.inc','app/tests/test-filter-normalize-gray-portrait.inc'},'fixture-only scope')
 check(len(baseline)==seal['baseline_sources']and sha((P/'baseline-source-sha256.json').read_bytes())==seal['manifest_sha256'],'baseline manifest')
 check(len(json.loads((P/'linked-before.json').read_text()))==seal['linked_inputs'] and len(json.loads((P/'runtime-assets.json').read_text()))==seal['runtime_assets'],'input counts')
 check(set(results)=={'normalize-gray'},'runtime cases')
 for name,j in results.items():result(j,name)
 archived={}
 with tarfile.open(P/'sources.tar.gz')as t:
  for f in t:
   check(f.isfile()and not Path(f.name).is_absolute()and '..'not in Path(f.name).parts and f.name not in archived,'archive path');archived[f.name]=sha(t.extractfile(f).read())
 check(archived==sources,'source archive')
 for n,h in owned.items():
  check(sources[n]==h,'owned archive')
  if a.check_source:check(sha((R/n).read_bytes())==h,n)
 provider=json.loads((P/'committed-gray-provider.json').read_text());core='app/core/gimpfilterlayer.cpp'
 commit=subprocess.check_output(['git','rev-parse',provider['commit']],cwd=R,text=True).strip();check(commit=='7e975aa124753ae8f4e96634543465bf996d3e04','committed Gray provider')
 check(sha(subprocess.check_output(['git','show',commit+':'+core],cwd=R))==provider['sha256']==sources[core],'provider bytes')
 check(provider['native_seal']['owned'][core]==sources[core]and not provider['native_seal']['changed'],'native predecessor seal')
 for n,h in sources.items():
  if n not in owned and n!=core:check(baseline[n]==h,'committed baseline source')
 from PIL import Image
 check(len(images)==2 and images[0]['sha256']!=images[1]['sha256'],'two distinct PNGs')
 for state,row in zip(['before','after'],images):
  check(row['path']=='normalize-gray-'+state+'-oncanvas-portrait.png','PNG name');check(sha((P/row['path']).read_bytes())==row['sha256'],'PNG hash');im=Image.open(P/row['path']).convert('RGB');check(im.size==(1080,1920),'portrait PNG')
  for probe,level in zip(row['probes'],[120,40,80]if state=='before'else[255,0,127]):
   check(probe['expected_rgb']==[level]*3 and probe['pixels']==400,'independent pixel oracle');crop=im.crop(tuple(probe['rect']));check(crop.size==(20,20),'patch size');pixels=[crop.getpixel((x,y))for y in range(20)for x in range(20)];deviation=max(abs(c-level)for pixel in pixels for c in pixel);check(deviation==probe['max_channel_error']and deviation<=3,'actual PNG patch')
 review=json.loads((P/'visual-review.json').read_text());check(review['reviewed_both']and review['actual_application']and review['oncanvas_visible']and not review['fixed_canvas_origin_claim'],'visual review')
 controls=0;good=results['normalize-gray']
 for bad in [dict(good,exit_code=1),dict(good,stdout=good['stdout'].replace('/filter-normalize-gray-portrait','/wrong')),dict(good,stdout=good['stdout'].replace('oncanvas visible','oncanvas hidden')),dict(good,stdout=good['stdout'].replace('1080x1920x24','1280x800x24')),dict(good,stdout=good['stdout'].replace('full120000 projection/source/native pixels','model-only'))]:
  try:result(bad,'normalize-gray')
  except ValueError:controls+=1
  else:raise ValueError('invalid UI result admitted')
 for name,h in json.loads((P/'predecessors.json').read_text()).items():
  check(sha((R/'migration/tests'/name/'artifact-sha256.json').read_bytes())==h,'predecessor pin');subprocess.run(['python3','-B',str(R/'migration/tests'/name/'validate.py')],check=True,stdout=subprocess.DEVNULL)
 print(f'PASS actual Gray portrait1 case/2states,6 PNG patch oracles,{controls} result controls,{len(sources)} archived sources; direct API,not editor gesture')
if __name__=='__main__':main()

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
 seal=json.loads((P/'seal.json').read_text());owned=json.loads((P/'owned.json').read_text());sources=json.loads((P/'source-sha256.json').read_text());baseline=json.loads((P/'baseline-source-sha256.json').read_text());results=json.loads((P/'results.json').read_text());images=json.loads((P/'images.json').read_text())
 check(seal['base_commit']=='0a775df4135052a2c851c9a5dfa37cea25887268' and not seal['changed']and seal['only_test_overlay']==owned,'exact source/overlay')
 check(set(owned)=={'app/tests/test-painter-mode-ui.c','app/tests/test-filter-compat-portrait.inc'},'fixture-only scope')
 check(len(baseline)==seal['baseline_sources']and sha((P/'baseline-source-sha256.json').read_bytes())==seal['manifest_sha256'],'baseline manifest')
 check(len(json.loads((P/'linked-before.json').read_text()))==seal['linked_inputs'] and len(json.loads((P/'runtime-assets.json').read_text()))==seal['runtime_assets'],'input counts')
 check(set(results)=={'normalize','exchange'},'runtime cases')
 for name,j in results.items():result(j,name)
 archived={}
 with tarfile.open(P/'sources.tar.gz')as t:
  for f in t:
   check(f.isfile()and not Path(f.name).is_absolute()and '..'not in Path(f.name).parts and f.name not in archived,'archive path');archived[f.name]=sha(t.extractfile(f).read())
 check(archived==sources,'source archive')
 for n,h in owned.items():
  check(sources[n]==h,'owned archive')
  if a.check_source:check(sha((R/n).read_bytes())==h,n)
 for n,h in sources.items():
  if n not in owned:check(baseline[n]==h,'committed provider source')
 with tempfile.TemporaryDirectory(prefix='portrait-image-check-')as d:
  report=Path(d)/'images.json';subprocess.run(['python3','-B',str(P/'check-filter-compat-portrait-images.py'),str(P),'--report',str(report)],check=True);check(json.loads(report.read_text())==images,'actual PNG pixels')
 review=json.loads((P/'visual-review.json').read_text());check(review['reviewed_all_four']and review['rendered_actual_application']and review['oncanvas_controls_visible']and not review['fixed_canvas_origin_claim'],'visual review scope')
 controls=0;good=results['normalize']
 for bad in [dict(good,exit_code=1),dict(good,stdout=good['stdout'].replace('/filter-normalize-portrait','/wrong')),dict(good,stdout=good['stdout'].replace('oncanvas visible','oncanvas hidden')),dict(good,stdout=good['stdout'].replace('1080x1920x24','1280x800x24')),dict(good,stdout=good['stdout'].replace('full120000 projection/source/native pixels','model-only'))]:
  try:result(bad,'normalize')
  except ValueError:controls+=1
  else:raise ValueError('invalid UI result admitted')
 for name,h in json.loads((P/'predecessors.json').read_text()).items():
  check(sha((R/'migration/tests'/name/'artifact-sha256.json').read_bytes())==h,'predecessor pin');subprocess.run(['python3','-B',str(R/'migration/tests'/name/'validate.py')],check=True,stdout=subprocess.DEVNULL)
 print(f'PASS actual portrait2 cases/4states,8 PNG patch oracles,{controls} result controls,{len(sources)} archived sources; direct API,not editor gesture')
if __name__=='__main__':main()

#!/usr/bin/env python3
import hashlib,json,re,tarfile
from pathlib import Path
from PIL import Image
P=Path(__file__).resolve().parent
def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,msg):
 if not ok:raise ValueError(msg)
def main():
 a=json.loads((P/'artifact-sha256.json').read_text());check(set(a)=={str(p.relative_to(P)) for p in P.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts},'artifact set')
 for p,h in a.items():check(sha((P/p).read_bytes())==h,p)
 r=json.loads((P/'native-gui.json').read_text());check(r['exit_code']==0,'GUI failed');check(re.findall(r'^ok \d+ (\S+)',r['stdout'],re.M)==['/painter-mode-ui/filter-red-eye-portrait'],'case count');check('1080x1920x24, authenticated accepted, empty authority rejected' in r['stdout'],'Xvfb');check(r['stdout'].count('metadata exact;oncanvas true1080x1920')==2,'model/metadata')
 b=json.loads((P/'source-before.json').read_text());e=json.loads((P/'source-after.json').read_text());check(b['sha256']==e['sha256'] and not e['changed_after_run'],'source drift');actual={}
 with tarfile.open(P/'sources.tar.gz') as t:
  for f in t:
   check(f.isfile() and not Path(f.name).is_absolute() and '..' not in Path(f.name).parts and f.name not in actual,'archive member');actual[f.name]=sha(t.extractfile(f).read())
 check(actual==b['sha256'],'source archive')
 for p,h in json.loads((P/'owned-source-sha256.json').read_text()).items():check(actual[p]==h,'owned')
 for i,h in enumerate(b['predecessors'].values()):check(sha((P/('predecessor-'+str(i)+'.json')).read_bytes())==h,'predecessor')
 for phase in ['before','after']:
  im=Image.open(P/f'red-eye-{phase}-oncanvas-portrait.png');check(im.size==(1080,1920),'portrait size');check(im.getpixel((320,900))[:3]==((200,20,10) if phase=='before' else (20,20,10)),'red pixel');check(im.getpixel((720,900))[:3]==(10,30,200),'blue control')
 print(f'PASS actual GUI1/two1080x1920 oncanvas states;model+rendered channels;{len(actual)} source seals;direct API not gesture')
if __name__=='__main__':main()

#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess,tarfile,re,argparse,struct
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
def verify(commit=None):
 for p,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==h,p
 m=json.loads((P/'owned-source.json').read_text())
 with tarfile.open(P/'owned-sources.tar.gz')as t:
  assert set(t.getnames())==set(m['owned_sha256'])
  for p,h in m['owned_sha256'].items():
   b=subprocess.check_output(['git','show',(commit or m['commit'])+':'+p],cwd=R);assert sha(b)==h and t.extractfile(p).read()==b,p
 old=json.loads((P/'old-oracle.json').read_text());b=subprocess.check_output(['git','show',old['commit']+':'+old['source']],cwd=R);assert b==(P/'old-settings.c').read_bytes()and sha(b)==old['sha256']
 rows=json.loads((P/'input-oracle.json').read_text());assert len(rows)==9
 actual=re.findall(r'^\s*\{"([^"]+)",\s*(INPUT_\w+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),',b.decode(),re.M)
 assert [[v.strip()for v in x]for x in actual]==old['rows']==[[x[k]for k in old['keys']]for x in rows]
 source=(P/'fixture.cpp').read_text();array='static const InputExpected inputs[]={\n'+'\n'.join(' {"%s",%d,double(float(%s)),double(float(%s))},'%(x['name'],x['index'],x['soft_minimum'],x['soft_maximum'])for x in rows)+'\n};';assert array in source
 result=json.loads((P/'result.json').read_text());assert result['exit']==0 and sha((P/'fixture.cpp').read_bytes())==result['fixture_sha256']
 log=(P/'run.log').read_text();assert len(re.findall(r'^ok \d+',log,re.M))==1 and '# SKIP'not in log
 assert re.findall(r'^# (\S+): add2\.\.8,.* PASS$',log,re.M)==[x['name']for x in rows]
 assert 'g_assert_cmpuint(success,==,144)'in source and 'g_assert_cmpuint(rejected,==,18)'in source and 'g_assert_cmpfloat_with_epsilon'not in source
 assert 'gimp_painter_canvas_ui_set_visible(shell,TRUE)'in source and 'gimp_painter_canvas_ui_get_visible(shell)'in source
 image=(P/'matrix-portrait.png').read_bytes();assert image[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack('>II',image[16:24])==(1080,1920)
 before=json.loads((P/'linked-before.json').read_text());assert before==json.loads((P/'linked-after.json').read_text())==json.loads((P/'runtime-inputs-after.json').read_text())
 duties=json.loads((P/'duty-owners.json').read_text());assert len(duties)==18 and all('24.004'in x['implementation_tasks'].split(',')for x in duties)
 print('PASS nine inputs /144 accepted and18 rejected ordinary curve operations; exact model/two-view state and oncanvas portrait')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--commit');args=a.parse_args();verify(args.commit)

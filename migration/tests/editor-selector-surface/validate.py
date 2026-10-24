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
  mask=t.extractfile('app/paint/painter-mypaint-surface/legacy-mask-transform.cpp').read().decode();assert 'if (gimp_matrix3_is_identity (&matrix))\n    return *source;'in mask
 result=json.loads((P/'result.json').read_text());source=(P/'fixture.cpp').read_text();assert result['exit']==0 and sha(source.encode())==result['fixture_sha256']
 log=(P/'run.log').read_text();assert len(re.findall(r'^ok \d+',log,re.M))==1 and '# SKIP'not in log
 assert len(re.findall(r'^# selector .* exact1024 stamp bytes and4 sample components PASS$',log,re.M))==6
 assert 'g_assert_true(actual==expected)'in source and 'g_assert_cmpfloat_with_epsilon'not in source
 assert 'GimpResources::Purpose::Stroke'in source and 'Purpose::Preview'not in source
 assert 'gimp_painter_canvas_ui_set_visible(shell,TRUE)'in source and 'gimp_painter_canvas_ui_get_visible(shell)'in source
 image=(P/'matrix-portrait.png').read_bytes();assert image[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack('>II',image[16:24])==(1080,1920)
 before=json.loads((P/'linked-before.json').read_text());assert before==json.loads((P/'linked-after.json').read_text())==json.loads((P/'runtime-inputs-after.json').read_text())
 rows=json.loads((P/'independent-oracle.json').read_text());assert len(rows)==6
 assert [(x['mask_point'],x['paper_byte'],x['grain'],x['contrast'])for x in rows]==[(3,255,0,1),(5,255,0,1),(5,0,0,1),(5,0,1,1),(5,0,1,0),(5,255,0,1)]
 for x in rows:
  weight=min(1,max(0,(x['paper_byte']/255+x['grain'])*x['contrast']));assert weight==x['weight']==int(x['covered'])
  assert x['pixel_x']==2+x['mask_point']%3 and x['pixel_y']==2+x['mask_point']//3
  b=bytearray(1024);i=(x['pixel_y']*16+x['pixel_x'])*4
  if x['covered']:b[i]=b[i+3]=255
  assert sha(b)==x['stamp_sha256'];assert x['sample']==[int(x['covered']and x['mask_point']==3),int(not x['covered']),int(x['covered']and x['mask_point']==5),int(x['covered'])]
 duties=json.loads((P/'duty-owners.json').read_text());assert len(duties)==18 and all('24.005'in x['implementation_tasks'].split(',')and '24.006'in x['implementation_tasks'].split(',')for x in duties)
 print('PASS six native selector/model/Stroke-surface scenes; exact1024-byte stamps and4-component samples, explicit oncanvas portrait')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--commit');args=a.parse_args();verify(args.commit)

#!/usr/bin/env python3
"""Independent byte specification checked against pinned existing old-runtime data."""
import csv,gzip,hashlib,json,pathlib,re,unittest
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def divide(n,d,floor=False):return n//d if floor or n>=0 else -((-n)//d)
def mix(a,b,q,alpha,shade=False,plus=1,floor=False):
 if alpha:
  u=(255-q)*a[-1];v=(q+plus)*b[-1];s=u+v
  return [a[c]+divide(a[c]*u+b[c]*v-s*a[c],s,floor)for c in range(len(a)-1)]+[s>>8] if s else [0]*len(a)
 return [divide(x*(255-q)+y*q,255,floor)if shade else x+divide(x*(255-q)+y*q-x*255,255,floor)for x,y in zip(a,b)]
def fixture_rows():
 for line in gzip.decompress((R/'migration/fixtures/legacy-smudge-pixels/pixels.tsv.gz').read_bytes()).decode().splitlines():
  if line.startswith('PIXELS '):
   _,c,q,a,b,_=line.split();yield int(c),int(q),bytes.fromhex(a),bytes.fromhex(b)
def evaluate(c,q,plus=1,floor=False):
 sampled=[(i*37+q*13)%256 for i in range(57*c)];accum=[(i*59+q*7+103)%256 for i in range(57*c)];color=[203,39,91,255][:c]
 if c%2==0:
  color[-1]=255
  for i in range(57):
   if i%7==0:sampled[i*c+c-1]=0
   if i%9==0:accum[i*c+c-1]=0
 output=[];paint=[]
 for i in range(0,57*c,c):
  a=mix(sampled[i:i+c],accum[i:i+c],q,c%2==0,plus=plus,floor=floor);output+=a;paint+=mix(a,color,q,c%2==0,True,plus,floor)
 return bytes(output),bytes(paint)
class Spec(unittest.TestCase):
 def test_source_identity_and_channels(self):
  pins=json.loads((P/'fixture-pins.json').read_text())
  for n,h in pins.items():self.assertEqual(hashlib.sha256((R/n).read_bytes()).hexdigest(),h)
  m=json.loads((P/'old-source-manifest.json').read_text());self.assertEqual(m['commit'],'afa43fae3e920210146abed514f136fd49f671b5')
  for n,v in m['files'].items():
   b=(P/'old-source'/n).read_bytes();self.assertEqual(hashlib.sha256(b).hexdigest(),v['sha256']);self.assertEqual(hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest(),v['git_blob'])
  s=(P/'old-source/app/paint/gimpsmudge.c').read_text();self.assertNotIn('GIMP_DYNAMICS_OUTPUT_FLOW',s)
  order=['rate_output = gimp_dynamics_get_output','rate = (options->rate / 100.0) * dynamic_rate','blend_region (&srcPR, &tempPR, &tempPR, ROUND (rate * 255.0))','if (options->use_color_blending)','GIMP_DYNAMICS_OUTPUT_BLENDING','shade_region (&tempPR, &blendPR, col, ROUND (blending_rate * 255.0))','gimp_brush_core_replace_canvas']
  start=s.index('static void\ngimp_smudge_motion');s=s[start:];positions=[s.index(t)for t in order];self.assertEqual(positions,sorted(positions))
  e=(P/'old-source/app/core/core-enums.h').read_text();names=re.findall(r'GIMP_DYNAMICS_OUTPUT_([A-Z_]+),',e);self.assertEqual(names,['OPACITY','SIZE','ANGLE','COLOR','HARDNESS','FORCE','ASPECT_RATIO','SPACING','RATE','FLOW','JITTER','BLENDING'])
  modern=(P/'modern-smudge.c').read_text();self.assertIn('(options->flow / 100.0)',modern);self.assertIn('GIMP_DYNAMICS_OUTPUT_FLOW',modern);self.assertNotIn('GIMP_DYNAMICS_OUTPUT_BLENDING',modern)
 def test_all_1024_old_vectors_two_stages(self):
  rows=list(fixture_rows());self.assertEqual(len(rows),1024)
  for c,q,a,b in rows:self.assertEqual(evaluate(c,q),(a,b),(c,q))
 def test_alpha_plus_one_mutant_rejected(self):
  self.assertTrue(any(evaluate(c,q,plus=0)!=(a,b)for c,q,a,b in fixture_rows()if c%2==0))
 def test_signed_floor_mutant_rejected(self):
  self.assertTrue(any(evaluate(c,q,floor=True)!=(a,b)for c,q,a,b in fixture_rows()))
 def test_shading_is_not_accumulator_feedback(self):
  old=[20,80,140,150];sample=[120,50,30,200];fg=[240,10,70,255]
  first=mix(sample,old,128,True);shaded=mix(first,fg,200,True,True)
  proper=mix(sample,first,180,True);feedback=mix(sample,shaded,180,True)
  self.assertNotEqual(proper,feedback);self.assertNotEqual(first,shaded)
 def test_rate_blending_orthogonal(self):
  sample=[10,20,30];old=[200,180,160];fg=[250,50,90]
  state=mix(sample,old,100,False)
  self.assertNotEqual(mix(state,fg,0,False,True),mix(state,fg,255,False,True));self.assertEqual(state,mix(sample,old,100,False));self.assertNotEqual(state,mix(sample,old,200,False))
 def test_all_52_original_duties_retained(self):
  rows=json.loads((P/'source-duty-review.json').read_text());self.assertEqual(len(rows),52);self.assertEqual(len({r['work_id']for r in rows}),52)
  expected={r['work_id']:r for r in csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').read_text().splitlines(),delimiter='\t')if r['wbs_task']=='23.001'};self.assertEqual(set(expected),{r['work_id']for r in rows})
  readme=(P/'README.md').read_text()
  for r in rows:
   for k in ['source_id','path','source_blob','source_range','source_sha256','phase','wbs_task','action','acceptance']:self.assertEqual(r[k],expected[r['work_id']][k])
   self.assertEqual(hashlib.sha256(r['payload'].encode()).hexdigest(),r['source_sha256']);self.assertIn('## '+r['section'],readme);self.assertTrue(r['specification_only']);self.assertEqual(r['acceptance'],'Flow/Rate と区別した仕様を記録')
if __name__=='__main__':unittest.main(verbosity=2)

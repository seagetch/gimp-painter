#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess,tarfile,re,argparse
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
def verify(commit=None):
 for p,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==h,p
 m=json.loads((P/'owned-source.json').read_text())
 with tarfile.open(P/'owned-sources.tar.gz')as t:
  assert set(t.getnames())==set(m['owned_sha256'])
  for p,h in m['owned_sha256'].items():
   b=subprocess.check_output(['git','show',(commit or m['commit'])+':'+p],cwd=R);assert sha(b)==h and t.extractfile(p).read()==b,p
 old=json.loads((P/'oracle-verification.json').read_text());data=subprocess.check_output(['git','show',old['legacy_commit']+':'+old['legacy_source']],cwd=R);assert data==(P/'legacy-settings.c').read_bytes()and sha(data)==old['legacy_source_sha256']
 rows=json.loads((P/'oracle.json').read_text())['settings'];assert len(rows)==old['count']==45
 for i,(a,b)in enumerate(zip(old['exact_rows'],rows)):assert a==[b['name'],b['symbol'],b['minimum'],b['default_value'],b['maximum']]and b['index']==i
 result=json.loads((P/'result.json').read_text());assert result['exit']==0 and sha((P/'fixture.cpp').read_bytes())==result['fixture_sha256']
 log=(P/'run.log').read_text();assert len(re.findall(r'^ok \d+',log,re.M))==1 and '# SKIP'not in log
 checked=re.findall(r'^# (\S+) independent old float metadata .* PASS$',log,re.M);assert checked==[r['name'].replace('_','-')for r in rows]
 expected_array='struct NumericExpected {const char*name;int index;double minimum,missing_default,maximum;};\nstatic const NumericExpected numeric_expected[]={\n'
 for row in rows:expected_array+='  {\"'+row['name'].replace('_','-')+'\",'+str(row['index'])+','+','.join('double(float('+row[k]+'))'for k in ['minimum','default_value','maximum'])+'},\n'
 expected_array+='};\n'
 assert expected_array in (P/'fixture.cpp').read_text()
 source=(P/'fixture.cpp').read_text();assert 'g_assert_cmpuint(assignments,==,405)'in source and 'g_assert_cmpfloat_with_epsilon'not in source
 assert 'gimp_painter_canvas_ui_set_visible(shell,TRUE)'in source and 'gimp_painter_canvas_ui_get_visible(shell)'in source
 before=json.loads((P/'linked-before.json').read_text());assert before==json.loads((P/'linked-after.json').read_text())==json.loads((P/'runtime-inputs-after.json').read_text())
 print('PASS45 numeric settings /405 exact edits, current owned providers; two views and explicit oncanvas portrait')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--commit');args=a.parse_args();verify(args.commit)

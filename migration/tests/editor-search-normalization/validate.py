#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess,tarfile,re,argparse,struct
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();EDITOR='app/widgets/gimppaintermybrusheditor.cpp'
HELPER='''String search_key(const char *text) {
  const char *value=text?text:"";
  if(!g_utf8_validate(value,-1,nullptr))
    throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Brush search needs valid UTF-8");
  String normalized(g_utf8_normalize(value,-1,G_NORMALIZE_ALL));
  if(!normalized)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Brush search normalization failed");
  String folded(g_utf8_casefold(normalized.get(),-1));
  if(!folded)throw Error(GIMP_PAINTER_ERROR_INVALID_STATE,"Brush search case folding failed");
  return folded;
}
'''
def transition(before,after):
 assert before.count('void rebuild_list(EditorImpl&i) {')==1
 expected=before.replace('void rebuild_list(EditorImpl&i) {',HELPER+'void rebuild_list(EditorImpl&i) {').replace('String needle(g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(i.search)),-1));','String needle=search_key(gtk_entry_get_text(GTK_ENTRY(i.search)));').replace('String haystack(g_utf8_casefold((std::string(name?name:"")+" "+tags).c_str(),-1));','String haystack=search_key((std::string(name?name:"")+" "+tags).c_str());')
 assert after==expected,'unexpected source delta'
def verify(commit=None):
 for p,h in json.loads((P/'artifact-sha256.json').read_text()).items():assert sha((P/p).read_bytes())==h,p
 before=(P/'before-editor.cpp').read_text();after=(P/'after-editor.cpp').read_text();transition(before,after)
 prior=json.loads((P/'predecessors.json').read_text());assert subprocess.check_output(['git','show',prior['predecessor_commit']+':'+EDITOR],cwd=R).decode()==before
 for p,h in prior['artifact_sha256'].items():assert sha((R/p).read_bytes())==h,p
 m=json.loads((P/'owned-source.json').read_text())
 with tarfile.open(P/'owned-sources.tar.gz')as t:
  assert set(t.getnames())==set(m['owned_sha256'])
  for p,h in m['owned_sha256'].items():
   b=subprocess.check_output(['git','show',commit+':'+p],cwd=R)if commit else (R/p).read_bytes();assert sha(b)==h and t.extractfile(p).read()==b,p
 assert sha(before.encode())==m['editor_predecessor_sha256']
 result=json.loads((P/'result.json').read_text());red=json.loads((P/'final-red/result.json').read_text());assert result['exit']==0 and red['exit']!=0
 assert (P/'fixture.cpp').read_bytes()==(P/'final-red/fixture.cpp').read_bytes();assert sha((P/'fixture.cpp').read_bytes())==result['fixture_sha256']==red['fixture_sha256']
 assert 'search [日本タグ]: rows=0 expected=1'in (P/'final-red/run.log').read_text()
 log=(P/'run.log').read_text();assert len(re.findall(r'^ok \d+',log,re.M))==1 and '# SKIP'not in log
 for query in ['日本タグ','日本タグ','café','café','ＦＲＥＳＨＴＡＧ','STRASSE']:assert 'search ['+query+']: rows=1 expected=1'in log
 assert 'full45 scalar/5switch/2reference/9input curves'in log
 for name,count in [('numeric',3),('curve',1),('selector',1)]:
  d=P/('replay-'+name);r=json.loads((d/'result.json').read_text());assert r['exit']==0 and sha((d/'fixture.cpp').read_bytes())==r['fixture_sha256']
  assert (d/'fixture.cpp').read_bytes()==(R/prior['replay_fixtures'][name]).read_bytes();assert len(re.findall(r'^ok \d+',(d/'run.log').read_text(),re.M))==count
 for d in [P,P/'final-red',P/'replay-numeric',P/'replay-curve',P/'replay-selector']:
  b=json.loads((d/'linked-before.json').read_text());assert b==json.loads((d/'linked-after.json').read_text())==json.loads((d/'runtime-inputs-after.json').read_text())
 image=(P/'matrix-portrait.png').read_bytes();assert image[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack('>II',image[16:24])==(1080,1920)
 source=(P/'fixture.cpp').read_text();assert 'gimp_painter_canvas_ui_set_visible(shell,TRUE)'in source and 'gimp_painter_canvas_ui_get_visible(shell)'in source
 duties=json.loads((P/'duty-owners.json').read_text());assert {k:len(v)for k,v in duties.items()}=={'24.007':27,'24.010':2,'24.011':2}
 print('PASS exact search-only successor; same-fixture red/green +numeric3/curve1/selector1; ordinary full draft history and portrait')
def controls():
 b=(P/'before-editor.cpp').read_text();a=(P/'after-editor.cpp').read_text()
 for changed in [a.replace('G_NORMALIZE_ALL','G_NORMALIZE_DEFAULT'),a.replace('if(!g_utf8_validate(value,-1,nullptr))','if(false)'),a+'\n/* unknown drift */\n']:
  try:transition(b,changed)
  except AssertionError:continue
  raise AssertionError('negative source control accepted')
 print('PASS3 source-transition rejection controls')
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('--commit');a.add_argument('--negative-controls',action='store_true');args=a.parse_args();verify(args.commit)
 if args.negative_controls:controls()

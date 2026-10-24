import pathlib,json,tempfile,shutil,subprocess,hashlib
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
controls=[('native-count','normal-results.json',lambda x:x[:-1]),('san-return','asan-results.json',lambda x:dict(x,returncode=1)),('san-unit','asan-build.json',lambda x:dict(x,instrumented_sources=x['instrumented_sources'][:-1])),('mutant-return','mutations.json',lambda x:[dict(r,returncode=0)for r in x]),('source-omission','source-manifest.json',lambda x:x[1:]),('held-omission','excluded-provider-seal.json',lambda x:x[:-1]),('old-id','undo-enum-baseline-values.json',lambda x:dict(x,GIMP_UNDO_PAINT=97)),('new-id','generation.json',lambda x:dict(x,appended={'GIMP_UNDO_GROUP_MYPAINT':109,'GIMP_UNDO_MYPAINT':111})),('generation','generation.json',lambda x:dict(x,generated_sha256=['0'*64]*2))]
for name,file,change in controls:
 with tempfile.TemporaryDirectory(prefix='undo-enum-proof-')as d:
  d=pathlib.Path(d);shutil.copytree(E,d/'proof');q=d/'proof';p=q/file;p.write_text(json.dumps(change(json.loads(p.read_text()))));pins=json.loads((q/'artifact-sha256.json').read_text());pins[file]=hashlib.sha256(p.read_bytes()).hexdigest();(q/'artifact-sha256.json').write_text(json.dumps(pins));r=subprocess.run(['python3',str(q/'validate.py'),'--root',str(R)],capture_output=True,text=True);assert r.returncode!=0,name
print('PASS',len(controls),'resealed negative controls')

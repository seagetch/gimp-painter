from pathlib import Path
import tempfile,shutil,subprocess,json,hashlib
E=Path(__file__).resolve().parent
for case in ['fragment-drift','failed-runtime-resealed','missing-sanitizer-resealed','missing-mutant-resealed']:
 with tempfile.TemporaryDirectory() as d:
  d=Path(d)/'package';shutil.copytree(E,d)
  if case=='fragment-drift':
   p=d/'old-compressor.inc';p.write_text(p.read_text()+'/* drift */\n')
  else:
   p=d/'results.json';r=json.loads(p.read_text())
   if case=='failed-runtime-resealed':r['runs']['normal']['runtime']['exit_code']=1
   if case=='missing-sanitizer-resealed':r['runs']['sanitizer']['compile']['command'].remove('-fsanitize=address,undefined')
   if case=='missing-mutant-resealed':r['mutations'].pop()
   p.write_text(json.dumps(r));seal=json.loads((d/'artifact-sha256.json').read_text());seal[p.name]=hashlib.sha256(p.read_bytes()).hexdigest();(d/'artifact-sha256.json').write_text(json.dumps(seal))
  r=subprocess.run(['python3',str(d/'validate.py')],capture_output=True,text=True);assert r.returncode!=0,case
print('PASS four event-foundation validator rejection controls')

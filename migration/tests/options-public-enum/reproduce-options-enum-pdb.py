from pathlib import Path
import shutil,os,subprocess,json,hashlib
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');O=Path('/workspace/shared/options-enum-generator/pdb-private');S=O/'source';B=O/'build'
headers=['libgimpbase/gimpbaseenums.h','libgimpconfig/gimpconfigenums.h','app/operations/operations-enums.h','app/core/core-enums.h','app/paint/paint-enums.h'];files=headers+['pdb/meson-enumgen.py','pdb/enumgen.pl','pdb/enums-external.pl','pdb/util.pl','pdb/meson-enumcode.py','pdb/enumcode.pl','libgimp/gimpenums.h','libgimp/gimpenums.c.tail','pdb/enums.pl']
for p in files:(S/p).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(R/p,S/p)
for p in ['pdb','libgimp']:(B/p).mkdir(parents=True,exist_ok=True)
env=dict(os.environ,srcdir=str(S/'pdb'),topsrcdir=str(S),rootme=str(B/'pdb'),PDBGEN_BACKUP='0',PDBGEN_GROUPS='0');commands=[];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();runs=[]
for i in range(2):
 for script,args in [('meson-enumgen.py',[str(S/p) for p in headers]),('meson-enumcode.py',[])]:
  cmd=['python3',str(S/'pdb'/script),'/usr/bin/perl',str(S),str(B),*args];commands.append(cmd);subprocess.run(cmd,env=env,check=True)
 result={p:sha(S/p) for p in ['pdb/enums.pl','libgimp/gimpenums.c.tail','libgimp/gimpenums.h']};assert all((S/p).read_bytes()==(R/p).read_bytes() for p in result),result;runs.append(result)
assert runs[0]==runs[1]
(O/'reproducibility.json').write_text(json.dumps({'commands':commands,'runs':runs,'input_sha256':{p:sha(R/p) for p in files},'unrelated_header_unchanged':True},indent=2)+'\n');print('PASS two explicit private PDB enum generator runs')

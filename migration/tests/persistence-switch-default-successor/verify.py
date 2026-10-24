#!/usr/bin/env python3
import argparse,pathlib,sys,json,hashlib,tempfile,subprocess,importlib.util
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];sys.path.insert(0,str(R/'migration/tests'));import persistence_switch_default_successor as m
def run(root,negative):
 r=m.reviewed(root)
 assert (root/m.OPTIONS).read_bytes().count(b'g_param_spec_boolean(s.internal_name,s.displayed_name,nullptr,s.default_value,flags)')==1
 if negative:
  checks=[('source',m.OPTIONS,lambda b:b+b'\n// source drift'),('default',m.FAMILY,lambda b:b.replace(b'Native GParamSpec default: TRUE;',b'Native GParamSpec default: FALSE;')),('neighbor',m.FAMILY,lambda b:b.replace(b'"id": "mypaint.option.use_gimp_texture"',b'"id": "different"')),('evidence',m.PACKAGE+'/transition.json',lambda b:b+b'changed')]
  for name,path,change in checks:
   original=(root/path).read_bytes()
   # An isolated in-memory read overlay avoids touching tested source or held paths.
   original_read=pathlib.Path.read_bytes
   def read(p,*a,**k):return change(original)if p==root/path else original_read(p,*a,**k)
   from unittest.mock import patch
   with patch.object(pathlib.Path,'read_bytes',read):
    try:m.reviewed(root)
    except (AssertionError,FileNotFoundError):pass
    else:raise AssertionError(('control accepted',name))
  print('PASS 4 portable source/family/evidence controls; 6 recorded focused controls also archived')
 print('PASS exact default successor, historical runtime preserved; one field, actual native JSON compatibility')
def main():
 a=argparse.ArgumentParser();a.add_argument('--root',type=pathlib.Path,default=R);a.add_argument('--commit');a.add_argument('--negative-controls',action='store_true');x=a.parse_args()
 hashes=json.loads((P/'artifact-sha256.json').read_text())
 for n,h in hashes.items():assert hashlib.sha256((P/n).read_bytes()).hexdigest()==h,n
 if not x.commit:run(x.root,x.negative_controls);return
 with tempfile.TemporaryDirectory()as d:
  root=pathlib.Path(d);get=lambda p:subprocess.check_output(['git','show',x.commit+':'+p],cwd=R)
  def put(p,b):f=root/p;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b)
  for p in [m.OPTIONS,m.FAMILY,*[m.PACKAGE+'/'+n for n in m.PINS]]:put(p,get(p))
  record=json.loads(get(m.PACKAGE+'/transition.json'));runtime=record['runtime_package'];manifest=get(runtime+'/artifact-sha256.json');put(runtime+'/artifact-sha256.json',manifest)
  for p in json.loads(manifest):put(runtime+'/'+p,get(runtime+'/'+p))
  run(root,x.negative_controls)
if __name__=='__main__':main()

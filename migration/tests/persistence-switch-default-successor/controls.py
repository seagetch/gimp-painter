from pathlib import Path
import sys,json,hashlib,subprocess,unittest.mock as mock
W=Path('/workspace/shared/persistence-switch-default');S=W/'source';sys.path.insert(0,str(S/'migration/tests'));import persistence_switch_default_successor as p
p.reviewed(S);controls=[]
def reject(name,path,change):
 f=S/path;before=f.read_bytes();was=f.is_symlink();f.unlink();f.write_bytes(change(before))
 try:
  try:p.reviewed(S)
  except (AssertionError,FileNotFoundError):controls.append(name)
  else:raise AssertionError(('control accepted',name))
 finally:f.unlink();f.write_bytes(before)
reject('source old default',p.OPTIONS,lambda b:b.replace(b'nullcpy',b'nullcpy').replace(b'nullptr,s.default_value,flags)',b'nullptr,FALSE,flags)'))
reject('unrelated provider drift',p.OPTIONS,lambda b:b+b'\n// unreviewed source change\n')
def field_change(b,field,key,value):
 x=json.loads(b);next(f for f in x['fields']if f['id']==field)[key]=value;return (json.dumps(x,indent=2)+'\n').encode()
reject('neighbor default',p.FAMILY,lambda b:field_change(b,'mypaint.option.use_gimp_texture','default','TRUE'))
reject('field ID mutation',p.FAMILY,lambda b:field_change(b,'mypaint.option.use_gimp_brushmark','id','wrong-id'))
reject('runtime false capture changed','migration/tests/options-switch-default/prior-serializer/false-changed.rc',lambda b:b+b'changed')
reject('transition evidence changed',p.PACKAGE+'/transition.json',lambda b:b+b'changed')
p.reviewed(S);(W/'controls.json').write_text(json.dumps({'result':'PASS','rejected':controls,'scope':'static declared-default/provider/evidence only; no preview negative controls or runtime acquisition'},indent=2)+'\n');print('PASS',len(controls),'focused negative controls')

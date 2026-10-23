import pathlib,sys,subprocess,json,hashlib,os
s,b,e=map(pathlib.Path,sys.argv[1:]);p=s/'app/xcf/painter-xcf-preserve.cpp';original=p.read_text();results=[]
cases=[('native-mode-on-wire','gimp_painter_layer_mode_is_compatibility (mode) || float_mode_name (mode) ? GIMP_LAYER_MODE_NORMAL_LEGACY : mode','gimp_painter_layer_mode_is_compatibility (mode) ? GIMP_LAYER_MODE_NORMAL_LEGACY : mode','float_mode_transport','xcf_painter_standard_mode (modes[k]) == GIMP_LAYER_MODE_NORMAL_LEGACY'),('accept-unknown-policy','policy != 1 || std::strcmp (space, "nonlinear-rgb")','policy > 2 || std::strcmp (space, "nonlinear-rgb")','float_mode_schema','assertion failed')]
def build(name):
 r=subprocess.run(['meson','compile','-C',str(b),'-j2','painter-xcf-roundtrip'],capture_output=True,text=True,timeout=180);(e/(name+'-build.log')).write_text(r.stdout+r.stderr);assert r.returncode==0

def test(name,path):
 cmd=['meson','test','-C',str(b),'--no-rebuild','--num-processes','1','--logbase',name,'--test-args=-p /painter-xcf-fields/'+path,'painter-xcf-roundtrip'];r=subprocess.run(cmd,capture_output=True,text=True,timeout=90);(e/(name+'.log')).write_text(r.stdout+r.stderr);d=json.loads((b/('meson-logs/'+name+'.json')).read_text());out={k:d.get(k)for k in ['name','result','returncode','duration','stdout','stderr','command']};(e/(name+'.json')).write_text(json.dumps(out,indent=2)+'\n');return d
try:
 for name,old,new,path,marker in cases:
  assert original.count(old)==1;changed=original.replace(old,new);p.write_text(changed);build(name);d=test(name,path)
  assert d['result']!='OK' and marker in d['stdout'],(name,d['result'],d['stdout'][-500:]);results.append({'mutation':name,'sha256':hashlib.sha256(changed.encode()).hexdigest(),'expected_path':path,'rejected':True})
finally:
 p.write_text(original);build('restored')
for path in ['float_mode_transport','float_mode_schema']:
 d=test('restored-'+path,path);assert d['result']=='OK' and 'ok 1 /painter-xcf-fields/'+path in d['stdout']
(e/'mutations.json').write_text(json.dumps({'cases':results,'restored_sha256':hashlib.sha256(p.read_bytes()).hexdigest()},indent=2)+'\n')

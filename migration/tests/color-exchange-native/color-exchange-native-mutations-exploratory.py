from pathlib import Path
import json,subprocess,os,resource,time,hashlib
s=Path('/workspace/shared/gimp-clean-aggregate/source');b=Path('/workspace/shared/gimp-clean-aggregate/build');o=Path('/workspace/shared/color-exchange-native-complete');p=s/'app/core/gimpfilterlayer.cpp';original=p.read_text();a=original.index('    else if (procedure == "plug-in-exchange")');z=original.index('    else if (procedure == "plug-in-red-eye-removal")',a);branch=original[a:z]
env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(s),GIMP_TESTING_ABS_TOP_BUILDDIR=str(b),GIMP_TESTING_PLUGINDIRS=str(b/'plug-ins/common'),UI_TEST='yes')
rows=[]
try:
 for name,old,new,case in [
  ('omit-uint8-domain',' || g_value_get_uint (args->at (i)) > 255u','','exchange_invalid_arguments'),
  ('wrong-to-slots','args->at (6 + i)','args->at (3 + i)','filter_color_exchange_native_basic'),
  ('omit-owner-projection','prepared.projection_input = true;','prepared.projection_input = false;','filter_color_exchange_native_basic')]:
  assert branch.count(old)==1,(name,branch.count(old));mutated=original[:a]+branch.replace(old,new)+original[z:];p.write_text(mutated)
  build=subprocess.run(['ninja','-C',str(b),'app/tests/gimp-filter-layer'],capture_output=True,text=True);(o/(name+'-build.log')).write_text(build.stdout+build.stderr);assert build.returncode==0,name
  cmd=[str(b/'app/tests/gimp-filter-layer'),'-p','/gimp-filter-layer/'+case];t=time.monotonic();r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=120,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
  row=dict(name=name,command=cmd,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,seconds=time.monotonic()-t,source_sha256=hashlib.sha256(original.encode()).hexdigest(),mutated_sha256=hashlib.sha256(mutated.encode()).hexdigest());rows.append(row);(o/'mutations.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,r.returncode,flush=True);assert r.returncode!=0,name
finally:
 p.write_text(original)
 restored=subprocess.run(['ninja','-C',str(b),'app/tests/gimp-filter-layer'],capture_output=True,text=True);(o/'restored-build.log').write_text(restored.stdout+restored.stderr);assert restored.returncode==0
 assert hashlib.sha256(p.read_bytes()).hexdigest()==hashlib.sha256(original.encode()).hexdigest()
cmd=[str(b/'app/tests/gimp-filter-layer'),'-p','/gimp-filter-layer/filter_color_exchange_native_basic','-p','/gimp-filter-layer/exchange_invalid_arguments'];r=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=120);(o/'restored.json').write_text(json.dumps(dict(command=cmd,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr),indent=2)+'\n');assert r.returncode==0

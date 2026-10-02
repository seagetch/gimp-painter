"""Bounded unsupported old mode observations; never a positive parity fixture."""
from pathlib import Path
import gzip, hashlib, json, os, shlex, subprocess
root=Path('/workspace/scratch/5b5281e79681/gimp-painter');old=root.parent/'gimp-painter-legacy';base=Path('/workspace/shared/painter-mypaint-gray-oracle');out=Path(__file__).parent
reference=json.loads((base/'report.json').read_text())
for name,h in reference['source_feature_sha256'].items():assert hashlib.sha256((old/name).read_bytes()).hexdigest()==h,name
report={'scope':'Bounded actual old unsupported Gray-alpha and Gray nonincremental diagnostic experiments. These are not positive equivalence fixtures.','source_commit':reference['source_commit'],'source_feature_sha256':reference['source_feature_sha256'],'archives_sha256':reference['archives_sha256'],'cases':{}}
for variant in ['gray-alpha','gray-floating']:
 work=out/variant;work.mkdir(exist_ok=True)
 s=(base/'capture-gray.cpp').read_text().replace('i<48;++i','i<1;++i')
 if variant=='gray-alpha':
  s=s.replace('GIMP_GRAY_IMAGE','GIMP_GRAYA_IMAGE').replace('width*height','width*height*2').replace('(y*width+x)','(y*width+x)*2').replace('initial[i]=(x*19+y*7)%256;','initial[i]=(x*19+y*7)%256;initial[i+1]=180;').replace('p.data(),width);','p.data(),width*2);').replace('initial.data(),width);','initial.data(),width*2);')
 else:s=s.replace('const bool floating=false,','const bool floating=true,')
 source=work/'probe.cpp';source.write_text(s);obj=work/'probe.o';binary=work/'probe'
 flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True));cmd=[os.environ.get('CXX','c++'),'-std=c++14','-g','-O2','-include','type_traits','-I'+str(old),'-I'+str(old/'app'),*flags,'-c',str(source),'-o',str(obj)]
 link=(base/'link.sh').read_text().replace(str(base/'capture-gray.o'),str(obj)).replace(str(base/'capture-gray'),str(binary));(work/'link.sh').write_text(link)
 with (work/'build.log').open('wb') as log:subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,check=True);subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=log,stderr=subprocess.STDOUT,check=True)
 profile=work/'profile';profile.mkdir(exist_ok=True);env=dict(os.environ);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(old),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(old),'GIMP2_DIRECTORY':str(profile)})
 case={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256((work/'.libs/probe').read_bytes()).hexdigest(),'compile_command':cmd,'runs':[]}
 for repeat in range(2):
  with (work/f'run{repeat}.stdout').open('wb') as stdout,(work/f'run{repeat}.stderr').open('wb') as stderr:r=subprocess.run([str(binary)],stdout=stdout,stderr=stderr,env=env,timeout=40)
  raw=(work/f'run{repeat}.stdout').read_bytes();values=b'\n'.join(x for x in raw.splitlines() if x.startswith(b'GRAY_SESSION_'))+b'\n'
  case['runs'].append({'exit_code':r.returncode,'unsupported_layer_diagnostics':raw.count(b'Unsupported layer type: 2'),'records':len(values.splitlines()),'records_sha256':hashlib.sha256(values).hexdigest(),'stdout_sha256':hashlib.sha256(raw).hexdigest()})
 report['cases'][variant]=case
report['changed_archives']=[n for n,h in reference['archives_sha256'].items() if hashlib.sha256((old/n).read_bytes()).hexdigest()!=h]
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({n:c['runs'] for n,c in report['cases'].items()},indent=2))

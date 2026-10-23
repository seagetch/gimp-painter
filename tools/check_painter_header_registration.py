#!/usr/bin/env python3
"""Verify immutable04.014 proof separately from current duty/build admission.

Default: exact original checker + live92-duty coverage, not current compilation.
--build: additionally require the configured registered compile result to match
current inputs and archives. No historical source digest is refreshed in place.
"""
import argparse,csv,hashlib,importlib.util,json,shlex,subprocess,tarfile,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DIRECTORY=ROOT/'migration/tests/header-registration'
RECOVERY=DIRECTORY/'recovery'
ORIGINAL='27f34ac3282c058caf309147dc7460a34d4b87de'
def sha(data):return hashlib.sha256(data).hexdigest()
def rows(path):
 with path.open()as stream:return list(csv.DictReader(stream,delimiter='\t'))
def check_historical():
 pin=json.loads((RECOVERY/'historical-pin.json').read_text())
 assert pin['original_commit']==ORIGINAL
 archive=RECOVERY/'historical-checkpoint.tar.gz';assert sha(archive.read_bytes())==pin['archive_sha256']
 with tempfile.TemporaryDirectory(prefix='painter-header-history-')as directory:
  root=Path(directory)
  with tarfile.open(archive)as t:
   members=t.getmembers();assert len(members)==len(pin['files'])==172
   assert {m.name for m in members}==set(pin['files'])
   for m in members:
    assert m.isfile() and not Path(m.name).is_absolute() and '..' not in Path(m.name).parts
    data=t.extractfile(m).read();assert sha(data)==pin['files'][m.name],m.name
    path=root/m.name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data);path.chmod(0o444)
  result=subprocess.run(['python3','-B',str(root/'tools/check_painter_header_registration.py')],cwd=root,capture_output=True,text=True)
  assert result.returncode==0,result.stdout+result.stderr
  assert 'all92 original verification duties' in result.stdout
  for name in ['report.json','evidence-manifest.json','evidence.tar.gz','source-duty-review.json']:
   assert (DIRECTORY/name).read_bytes()==(root/'migration/tests/header-registration'/name).read_bytes(),name
  return rows(root/'migration/inventory/legacy-port-work-items.tsv')
def check_current_duties(original,current):
 old={r['work_id']:r for r in original if r['wbs_task']=='04.014'}
 live={r['work_id']:r for r in current if r['wbs_task']=='04.014'}
 assert len(old)==len(live)==92 and set(old)==set(live),'original header duty lost/retargeted/duplicated'
 assert len([r for r in current if r['wbs_task']=='04.014'])==92
 for key,row in old.items():
  now=live[key]
  for field in ['source_kind','source_id','path','source_blob','source_range','source_sha256','phase','wbs_task','action','acceptance']:
   assert now[field]==row[field],(key,field)
  assert now['status']=='DONE' and now['phase']=='verification',key
  assert all(now[f] not in ('','-','pending') for f in ['owner','artifact','test','result']),key
 return {'original_duties':92,'current_duties':92,'identity_and_acceptance_preserved':True,'unrelated_statuses':'checked in immutable original ledger, not frozen across legitimate successor work'}
def current_specification(http):
 module=importlib.util.spec_from_file_location('painter_current_headers',ROOT/'app/painter/tests/headers/generate-probes.py')
 generator=importlib.util.module_from_spec(module);module.loader.exec_module(generator)
 return generator.specification(ROOT,http)
def check_build(build):
 build=Path(build).resolve();directory=build/'app/painter/tests/headers'
 result=json.loads((directory/'header-compile-result.json').read_text())
 assert result['status']=='PASS' and not result['outer_linkage_adapter']
 probes,manifest=current_specification(result['http_enabled'])
 assert json.loads((directory/'header-probes.json').read_text())==manifest,'stale generated header inputs'
 for key,value in manifest.items():assert result[key]==value,('stale compile result',key)
 assert result['compiled_probes']==len(probes)
 assert result['c_probes']==sum(p['language']=='c' for p in probes)
 assert result['cpp_probes']==sum(p['language']=='cpp' for p in probes)
 for p in probes:assert (directory/p['name']).read_text()==p['body'],p['name']
 assert len(result['libraries'])==2
 for path,record in result['libraries'].items():
  library=Path(path);assert library.resolve().parent==directory
  assert library.stat().st_size==record['bytes'] and sha(library.read_bytes())==record['sha256'],'stale/missing compile archive'
 commands=json.loads((build/'compile_commands.json').read_text()); selected=[]; vectors={}
 for entry in commands:
  if Path(entry['file']).name.startswith('probe-'):
   source=(Path(entry['directory'])/entry['file']).resolve()
   vectors.setdefault(source,[]).append(entry)
 for p in probes:
  entries=vectors.get((directory/p['name']).resolve(),[])
  assert len(entries)==1,p['name']
  entry=entries[0];command=entry.get('command','');standard='-std=c11' if p['language']=='c' else '-std=c++14'
  assert standard in shlex.split(command) and '-Werror' in shlex.split(command),(p['name'],'language/warning contract')
  assert (build/entry['output']).is_file(),p['name']
  selected.append({'name':p['name'],'language':p['language'],'command':command,'object_sha256':sha((build/entry['output']).read_bytes())})
 targets=json.loads((build/'meson-info/intro-targets.json').read_text())
 for name in ['painter-public-headers-c','painter-public-headers-cpp']:
  target=next(t for t in targets if t['name']==name)
  assert target['type']=='static library' and not target['installed'] and target['build_by_default']
 return {'status':'PASS','http_enabled':result['http_enabled'],'compiled_probes':len(probes),'c_probes':result['c_probes'],'cpp_probes':result['cpp_probes'],'source_sha256':manifest['source_sha256'],'libraries':result['libraries'],'compile_vectors':selected,'scope':'Current configured native registered compile only; other configuration/platform/runtime/linkABI not inferred'}
def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--build',type=Path);parser.add_argument('--report',type=Path);args=parser.parse_args()
 original=check_historical();result={'historical_original_checker':'PASS','current_duties':check_current_duties(original,rows(ROOT/'migration/inventory/legacy-port-work-items.tsv'))}
 if args.build:result['current_build']=check_build(args.build)
 if args.report:args.report.write_text(json.dumps(result,indent=2)+'\n')
 print('PASS: immutable original default231/HTTP233+18 controls; all92 current duty identities/acceptance retained'+('; current configured compilation verified' if args.build else '; current compilation NOT checked (use --build)'))
if __name__=='__main__':main()

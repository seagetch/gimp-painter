#!/usr/bin/env python3
"""Check bounded Surface ownership evidence, not whole-application leak freedom."""
import argparse,hashlib,json,pathlib,re,tarfile
HERE=pathlib.Path(__file__).resolve().parent
ROOT=HERE.parents[2]
def require(ok,msg):
 if not ok:raise ValueError(msg)
def tap(text,n):
 require(f'1..{n}' in text.splitlines(),'TAP plan')
 require(len(re.findall(r'^ok \d+ ',text,re.M))==n,'TAP pass count')
 require(not re.search(r'(?im)^not ok |#\s*(skip|todo)\b',text),'no skip or failure')
def validate(data,native,mapping):
 require(data['LSan'] is False,'bounded sanitizer scope')
 require(len(data['results'])==2 and [x['sanitized'] for x in data['results']]==[False,True],'normal and sanitizer independent')
 for row in data['results']:
  require(row['returncode']==0 and row['passed']==14 and row['timeout']==60 and row['duration']<60,'standalone14/original budget')
  tap(row['stdout'],14);require(not re.search('AddressSanitizer:|runtime error:',row['stderr']),'no sanitizer diagnostic')
 require(len(native)==2,'two registered targets')
 for row in native:
  n=14 if row['name'].endswith('region-internals') else 11
  require(row['name'].endswith(('region-internals','region-lifetime')),'registered target identity')
  require(row['result']=='OK' and row['returncode']==0 and row['duration']<60,'native target pass');tap(row['stdout'],n)
 require(mapping['criterion_status']=='DONE' and mapping['whole_hunk_status']=='TODO' and not mapping['factory_semantic_parity'] and not mapping['production_change'],'honest bounded scope')
 rows=mapping['legacy_sites'];expected=[327,331,335,345,346,347,348,349,400,401,402,403,614,618]
 require([int(r['site'].rsplit(':',1)[1]) for r in rows]==expected,'all14original ownership sites')
 require(sum(r['kind']=='G_FREE' for r in rows)==12,'12free sites')
 for row in rows:
  require(row['current_owner'] and row['evidence'],'ownership evidence')
  if row['kind']=='NEW':require(row['remaining_tasks']==['08.018','31.017'] and row['status']=='factory-owner-traced-not-semantic-parity','factory gaps retained')
 require(len(mapping['ancestor_duties'])==3,'all ancestor manifests')
 for entry in mapping['ancestor_duties'].values():
  require(entry['duties'],'inherited duties retained')
  for duty in entry['duties']:
   require(duty.get('whole_hunk_status',duty.get('whole_hunk_checkpoint_status'))=='TODO','whole-hunk not closed')
   require('09.003/region-lifetime' in duty['successor_obligations'],'original lifetime obligation')
   require({'09.008','09.009','09.011','09.012'}<=set(duty['successor_obligations']),'remaining semantics retained')
def main():
 p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
 load=lambda n:json.loads((HERE/n).read_text())
 data=load('results.json');mapping=load('ownership-map.json');validate(data,load('native.json'),mapping)
 for name,digest in load('artifact-sha256.json').items():require(hashlib.sha256((HERE/name).read_bytes()).hexdigest()==digest,'artifact changed: '+name)
 blob=(HERE/'legacy-surface.cpp.txt').read_bytes();require(hashlib.sha256(blob).hexdigest()==mapping['legacy_sha256'],'pinned original source')
 for row in mapping['legacy_sites']:
  n=int(row['site'].rsplit(':',1)[1]);require(row['original_statement'].strip() in blob.decode().splitlines()[n-1].strip(),'original statement exact')
 with tarfile.open(HERE/'tested-sources.tar.gz')as archive:
  for name,digest in data['source_sha256'].items():require(hashlib.sha256(archive.extractfile(name).read()).hexdigest()==digest,'tested source snapshot: '+name)
 if a.check_source:
  for name,digest in data['source_sha256'].items():require(hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,'current source changed: '+name)
 for name,entry in mapping['ancestor_duties'].items():
  current=json.loads((ROOT/name).read_text())
  for duty in entry['duties']:require(duty in current['duties'],'ancestor obligation altered: '+name)
 print('PASS Surface ownership14normal+14ASan, registered14+11;14legacy sites; all ancestor duties retained; no whole-process/preview parity claim')
if __name__=='__main__':main()

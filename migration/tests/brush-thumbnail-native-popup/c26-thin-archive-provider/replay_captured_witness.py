#!/usr/bin/env python3
"""Replay hash-reconstructed defining lines, NOT absent full maps/ELF/archive."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

WITNESS_SHA='1a445608e46d7044ee5e71ccadfb2b5787c9e7b981164788ce294b35753a4853'
OLD_HELPER_SHA='79006a5d08962548a69999e8083a9c22885b4183f6ada7fc7015fcce1256f064'
def sha(data):return hashlib.sha256(data).hexdigest()
def module(path,name):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m

def replay(witness_path,old_path,new_path):
 raw=Path(witness_path).read_bytes();assert sha(raw)==WITNESS_SHA
 assert sha(Path(old_path).read_bytes())==OLD_HELPER_SHA
 w=json.loads(raw)['provider_witness'];assert w['archive_kind']=='thin'
 old=module(old_path,'old_mask');new=module(new_path,'new_mask')
 assert old.APIS==new.APIS
 build=Path('/replay/build');archive=build/'app/widgets/libappwidgets.a';results={}
 for target in (new.FIXED,new.OLD):
  item=w['targets'][target];assert item['status']=='parsed-single-cref' and item['cross_reference_section_count']==1 and item['output_records']==1
  assert [x['api'] for x in item['apis']]==list(new.APIS)
  lines=[];provider=build/item['expected_object'].removeprefix('build/')
  for api in item['apis']:
   assert api['defining_entry_count']==1 and len(api['entries'])==1
   entry=api['entries'][0];token=entry['provider'];assert entry['parse_state']=='symbol-and-provider-token'
   assert token['format']=='direct-expected-object' and token['direct_matches_expected_object'] is True
   assert token['normalized_object']==item['expected_object']
   text=token['normalized_object'].removeprefix('build/')
   assert len(text.encode())==token['token_bytes'] and sha(text.encode())==token['token_sha256']
   padding=entry['line_bytes']-len(api['api'].encode())-len(text.encode());assert 1<=padding<=256
   line=api['api']+' '*padding+text
   assert sha(line.encode())==entry['line_sha256'];lines.append(line)
  binary=build/new.SUBDIR/target
  # OUTPUT/LOAD are a synthetic minimal context. Their original raw bytes and
  # map files were not retained. Only each defining line above is hash-exact.
  text='OUTPUT('+str(binary)+')\nLOAD '+str(archive if target==new.FIXED else provider)+'\nCross Reference Table\n'+'\n'.join(lines)+'\n'
  args=(text,build,binary,provider,new.APIS)
  if target==new.FIXED:
   try:old.map_defs(*args,archive,provider.name)
   except ValueError as exc:assert str(exc)=='wrong-archive-api-provider';old_result='rejected-wrong-archive-api-provider'
   else:raise AssertionError('old parser unexpectedly admitted direct thin representation')
   new.map_defs(*args,archive,provider.name,thin_archive=True)
   try:new.map_defs(*args,archive,provider.name,thin_archive=False)
   except ValueError as exc:assert str(exc)=='wrong-archive-api-provider';regular='rejected'
   else:raise AssertionError('regular archive incorrectly admitted')
  else:
   old.map_defs(*args);new.map_defs(*args);old_result='accepted-direct-fixture';regular='not-applicable'
  results[target]={'defining_lines_hash_exact':len(lines),'old_parser':old_result,'new_parser':'accepted','regular_archive_direct_form':regular,'captured_map_sha256':item['map_sha256']}
 return {'schema':'canvas-mask-captured-defining-lines-replay-v1','acceptance_eligible':False,'native_executed':False,
         'full_raw_map_available':False,'archive_or_elf_replayed':False,
         'scope':'20 defining lines reconstructed exactly against captured token and line hashes; synthetic OUTPUT/LOAD envelope only, not full map/native provider proof',
         'input_sha256':{'captured-provider.json':WITNESS_SHA,'old-helper.py':OLD_HELPER_SHA,'new-helper.py':sha(Path(new_path).read_bytes()),'replay-script.py':sha(Path(__file__).read_bytes())},
         'captured_archive_kind':'thin','captured_archive_member_name_sha256':w['ar_selected_member_sha256'],
         'captured_compiled_object_sha256':w['compiled_fixed_object_sha256'],
         'hash_scope_note':'ar_selected_member_sha256 is the UTF-8 member-name hash, not object bytes; C24 emitted witness only after unique member and ar-p bytes equality guards passed',
         'results':results,'representation_replay_passed':True}

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--witness',required=True);p.add_argument('--old-helper',required=True);p.add_argument('--new-helper',required=True);p.add_argument('--output',required=True);a=p.parse_args()
 Path(a.output).write_text(json.dumps(replay(a.witness,a.old_helper,a.new_helper),indent=2,sort_keys=True)+'\n')

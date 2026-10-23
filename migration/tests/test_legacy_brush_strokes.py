"""Verify bounded measured old-engine evidence, not tablet/UI or port replay."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import unittest
ROOT = Path(os.environ.get('GIMP_MIGRATION_ROOT', Path(__file__).resolve().parents[2]))
FIXTURES = Path(os.environ.get('GIMP_STROKE_FIXTURES', ROOT/'migration/fixtures/legacy-brush-strokes'))
spec = importlib.util.spec_from_file_location('stroke_validator', ROOT/'tools/validate_stroke_record.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)
NAMES = ('pen','textured_ink','watercolor_expressive')
def read(p): return json.loads(p.read_text())
def lines(p): return [json.loads(line) for line in p.read_text().splitlines()]
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
class OriginalStrokeEvidenceTests(unittest.TestCase):
 def test_all_raw_evidence_hashes(self):
  index=read(FIXTURES/'runtime-evidence-index.json')
  self.assertGreaterEqual(len(index),60)
  for name,expected in index.items():self.assertEqual(sha(FIXTURES/name),expected,name)
 def test_records_and_sidecar_hashes(self):
  for name in NAMES:
   record=read(FIXTURES/(name+'.json'))
   self.assertEqual(validator.validate(record,FIXTURES),[],name)
   self.assertEqual(record['evidence_kind'],'legacy-runtime-capture')
 def test_exact_boundary_events(self):
  for name in NAMES:
   record=read(FIXTURES/(name+'.json'));raw=lines(FIXTURES/name/'boundary.jsonl')
   self.assertEqual(len(raw),18)
   for r,e in zip(raw,record['events']):
    self.assertEqual({k:v for k,v in r.items() if k not in ('snapshot','surface','monotonic_ns')},{k:v for k,v in e.items() if k!='monotonic_ns'})
    self.assertEqual(r['monotonic_ns']-raw[0]['monotonic_ns'],e['monotonic_ns'])
   self.assertEqual(raw[6]['coords']['x'],raw[7]['coords']['x'])
   self.assertEqual(raw[6]['coords']['y'],raw[7]['coords']['y'])
   self.assertNotEqual(raw[6]['coords']['pressure'],raw[7]['coords']['pressure'])
   self.assertEqual(raw[7]['dtime_s'],0)
   self.assertEqual(raw[-1]['coords']['pressure'],0)
   self.assertEqual([r['sequence'] for r in raw if r['engine_split_after']],[0])
 def test_state_names_bits_and_actual_seeds(self):
  enum=(FIXTURES/'pinned-enum-settings.h').read_text()
  names=[name for name,index in re.findall(r'#define (STATE_\w+) (\d+)',enum) if name!='STATE_COUNT']
  self.assertEqual(len(names),30)
  for name in NAMES:
   record=read(FIXTURES/(name+'.json'));raw=lines(FIXTURES/name/'boundary.jsonl');seeds=lines(FIXTURES/name/'seeds.jsonl')
   self.assertEqual(record['engine_state']['state_names'],names)
   for key in ('state_f32_bits','reset_requested','stroke_total_painting_time','stroke_current_idling_time'):
    self.assertEqual(record['engine_state'][key],raw[0]['snapshot'][key])
   self.assertEqual(record['engine_state']['rng_seed_uint32'],seeds[0]['actual_seed_uint32'])
   self.assertEqual(len(seeds),len(raw))
   for e,s in zip(raw,seeds):
    self.assertEqual(e['sequence'],s['sequence']);self.assertEqual(len(e['snapshot']['state_f32_bits']),30)
    floating=struct.unpack('!f',bytes.fromhex(e['snapshot']['state_f32_bits'][names.index('STATE_RNG_SEED')]))[0]
    self.assertEqual(int(floating),s['actual_seed_uint32'])
   self.assertTrue(any(s['actual_seed_uint32']!=0 for s in seeds))
 def test_resources_and_nonuniform_initial_pixels(self):
  for name in NAMES:
   p=FIXTURES/name;resource=read(p/'resources.json');settings=read(p/'effective-settings.json');data=(p/'initial.rgba').read_bytes()
   self.assertEqual(len(data),96*80*4)
   self.assertEqual(len(set(data[i:i+4] for i in range(0,len(data),4))),2)
   self.assertEqual((p/'selection.u8').read_bytes(),b'\0'*(96*80))
   self.assertEqual(settings['switches'],[False]*5);self.assertEqual(settings['text'],[None,None])
   self.assertEqual(len(settings['engine_settings']),52)
   for field in ('brushmark','texture','layer_mask'): self.assertIsNone(resource[field])
   self.assertEqual(resource['layer_offset'],[0,0]);self.assertFalse(resource['non_incremental'])
 def test_source_brush_hashes(self):
  for name in NAMES:
   record=read(FIXTURES/(name+'.json'));resource=read(FIXTURES/name/'resources.json')
   self.assertEqual(sha(FIXTURES/name/'brush.myb'),record['brush']['sha256'])
   self.assertEqual(resource['brush_resource']['sha256'],record['brush']['sha256'])
   self.assertEqual(resource['brush_resource']['path'],record['brush']['source_path'])
 def test_outputs_reopen_and_repeat(self):
  for name in NAMES:
   p=FIXTURES/name;final=(p/'final.rgba').read_bytes()
   self.assertNotEqual(final,(p/'initial.rgba').read_bytes())
   self.assertEqual(final,(p/'reopened.rgba').read_bytes())
   self.assertEqual(final,(p/'repeat-final.rgba').read_bytes())
   self.assertTrue((p/'final.xcf').read_bytes().startswith(b'gimp xcf '))
   self.assertIn('STROKE_REOPEN_COMPLETE',(p/'reopen.log').read_text())
   self.assertIn('INDEPENDENT_ORIGINAL_STROKE_REOPEN_PASS',(p/'independent.log').read_text())
   for exitfile in ('exit-code','repeat-exit-code','independent-exit-code'):self.assertEqual((p/exitfile).read_text().strip(),'0')
 def test_repeat_engine_state_and_events(self):
  for name in NAMES:
   p=FIXTURES/name
   first=lines(p/'boundary.jsonl');second=lines(p/'repeat-boundary.jsonl')
   self.assertEqual([{k:v for k,v in e.items() if k!='monotonic_ns'} for e in first],[{k:v for k,v in e.items() if k!='monotonic_ns'} for e in second])
   self.assertEqual(lines(p/'seeds.jsonl'),lines(p/'repeat-seeds.jsonl'))
 def test_binary_scope_and_claims(self):
  build=read(FIXTURES/'build-manifest.json');summary=read(FIXTURES/'summary.json')
  self.assertEqual(build['preserved_original_binary']['sha256'],'85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1')
  self.assertNotEqual(build['capture_binary']['sha256'],build['preserved_original_binary']['sha256'])
  self.assertIn('synthetic',summary['scope'])
  self.assertTrue(summary['acceptance_gaps'])
if __name__=='__main__':unittest.main()

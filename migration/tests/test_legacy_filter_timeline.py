import hashlib,json,unittest
from pathlib import Path
F=Path(__file__).resolve().parents[1]/'fixtures/legacy-filter-timeline'
class TimelineEvidence(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.manifest=json.loads((F/'manifest.json').read_text());cls.data=json.loads((F/'timeline.json').read_text())
 def test_sealed_hashes(self):
  for name,digest in self.manifest['files'].items():
   with self.subTest(file=name):self.assertEqual(hashlib.sha256((F/name).read_bytes()).hexdigest(),digest)
 def test_raw_log_parsing_and_monotonic_clock(self):
  for key,record in self.data.items():
   rows=[]
   for line in (F/key/'capture.log').read_text().splitlines():
    if line.startswith('TL '):
     row=dict(p.split('=',1) for p in line.split()[1:]);row['time_us']=int(row['time_us']);rows.append(row)
   self.assertEqual(rows,record['events']);self.assertEqual(sorted(r['time_us'] for r in rows),[r['time_us'] for r in rows]);self.assertTrue(rows)
 def test_process_completion_and_unchanged_gimp_binary_only(self):
  for root in ['captures','repeat-captures']:
   self.assertEqual((F/root/'original-before.sha256').read_text(),(F/root/'original-after.sha256').read_text())
   for scene in ['async','stack','cancel','latecancel','detach','builtin']:
    self.assertEqual((F/root/scene/'exit-code').read_text().strip(),'0');self.assertIn('TIMELINE_COMPLETE scene='+scene,(F/root/scene/'capture.log').read_text())
 def events(self,root,scene,name='top',ordinal='1'):
  return [r['event'] for r in self.data[root+'/'+scene]['events'] if r['name']==name and r['ordinal']==ordinal]
 def test_async_execution_and_application_order(self):
  for root in ['captures','repeat-captures']:
   for ordinal in ['1','2']:
    es=self.events(root,'async',ordinal=ordinal)
    ordered=['run-enter','run-return-running','shadow-merge-enter','shadow-merge-applied','progress-end-callback','runner-on-end','propagation-begin','propagation-end']
    indexes=[es.index(x) for x in ordered];self.assertEqual(indexes,sorted(indexes))
   self.assertEqual(self.data[root+'/async']['states'][-1]['pending'],'0')
 def test_builtin_no_end_with_pending_cache(self):
  for root in ['captures','repeat-captures']:
   es=self.events(root,'builtin');self.assertIn('shadow-merge-applied',es);self.assertNotIn('progress-end-callback',es);self.assertLess(es.index('shadow-merge-applied'),es.index('run-return-running'));self.assertEqual(self.data[root+'/builtin']['states'][-1]['pending'],'1')
 def test_stack_stop_and_causal_requeue_order(self):
  for root in ['captures','repeat-captures']:
   rows=self.data[root+'/stack']['events']
   def pos(name,ordinal,event):return next(i for i,r in enumerate(rows) if (r['name'],r['ordinal'],r['event'])==(name,ordinal,event))
   initial=[pos('top','1','run-enter'),pos('top','1','stop-request-running'),pos('top','1','cancel-signal'),pos('bottom','1','run-enter'),pos('bottom','1','progress-end-callback'),pos('top','1','stack-unblocked-requeue'),pos('top','2','run-enter'),pos('top','2','shadow-merge-applied'),pos('top','2','progress-end-callback')]
   self.assertEqual(initial,sorted(initial));self.assertEqual(len(initial),len(set(initial)))
   edit=[pos('source','0','edit-source-request'),pos('bottom','2','run-enter'),pos('bottom','2','shadow-merge-applied'),pos('bottom','2','progress-end-callback'),pos('top','2','stack-unblocked-requeue'),pos('top','3','run-enter'),pos('top','3','shadow-merge-applied'),pos('top','3','progress-end-callback')]
   self.assertEqual(edit,sorted(edit));self.assertEqual(len(edit),len(set(edit)))
 def test_detached_completion_is_not_success(self):
  for root in ['captures','repeat-captures']:
   rows=self.data[root+'/detach']['events'];self.assertTrue(any(r['event']=='progress-end-callback' and r['attached']=='0' for r in rows));self.assertEqual(self.data[root+'/detach']['states'][-1]['attached'],'0');log=(F/root/'detach/capture.log').read_text();self.assertIn('fatal error: Segmentation fault',log);self.assertIn('Plug-in crashed: "edge"',log);self.assertLess(log.index('fatal error: Segmentation fault'),log.index('event=progress-end-callback'));self.assertNotIn('event=shadow-merge-applied',log)
 def test_cancel_before_end_and_following_edit_completes(self):
  for root in ['captures','repeat-captures']:
   for scene,request in [('cancel','explicit-cancel-request'),('latecancel','late-cancel-request')]:
    rows=self.data[root+'/'+scene]['events']
    def pos(name,ordinal,event):return next(i for i,r in enumerate(rows) if (r['name'],r['ordinal'],r['event'])==(name,ordinal,event))
    ordered=[pos('top','1','run-enter'),pos('top','1',request),pos('top','1','progress-end-callback'),pos('source','0','edit-source-request'),pos('top','2','run-enter'),pos('top','2','shadow-merge-applied'),pos('top','2','progress-end-callback')]
    self.assertEqual(ordered,sorted(ordered));self.assertEqual(len(ordered),len(set(ordered)))
    es=self.events(root,scene)
    if scene=='latecancel':
     self.assertLess(es.index('progress-value'),es.index(request));self.assertNotIn('shadow-merge-applied',es)
    else:self.assertLess(es.index(request),es.index('shadow-merge-applied'))
    self.assertEqual(self.data[root+'/'+scene]['states'][-1]['pending'],'0')
 def test_postcapture_runtime_inventory_scope(self):
  r=json.loads((F/'runtime-byte-inventory.json').read_text());self.assertIn('Postcapture',r['scope']);self.assertIn('not historical process maps',r['scope']);self.assertIn('not before/after',r['scope']);self.assertTrue(r['captured_executor_utc']);self.assertEqual(len(r['edge']['sha256']),64);self.assertGreater(r['edge']['size'],0);self.assertGreater(len(r['dependencies']),20)
  for row in r['dependencies'].values():self.assertEqual(len(row['sha256']),64);self.assertGreater(row['size'],0);self.assertTrue(row['loader_roots'])
 def test_stable_ids_and_repeated_cache_hashes(self):
  for record in self.data.values():
   for name in {e['name'] for e in record['events']}:
    rows=[e for e in record['events'] if e['name']==name];self.assertEqual(len({(e['ptr'],e['id']) for e in rows}),1)
  for scene in ['async','stack','cancel','latecancel','detach','builtin']:
   a=[l for l in (F/'captures'/scene/'capture.log').read_text().splitlines() if l.startswith('CACHE ')];b=[l for l in (F/'repeat-captures'/scene/'capture.log').read_text().splitlines() if l.startswith('CACHE ')];self.assertTrue(a);self.assertEqual(a,b)
 def test_authenticated_transport(self):
  for root in ['captures','repeat-captures']:
   for scene in ['async','stack','cancel','latecancel','detach','builtin']:
    r=json.loads((F/root/scene/'display/results.json').read_text());self.assertEqual(r['transport_validation'],'PASS');self.assertEqual(r['authenticated_client_exit'],0);self.assertNotEqual(r['unauthenticated_client_exit'],0);self.assertEqual(r['command_exit'],0)
 def test_observer_is_not_native_generation(self):
  self.assertIn('NOT an original generation',(F/'README.md').read_text());self.assertIn('afa43fae3e920210146abed514f136fd49f671b5',(F/'build-provenance.json').read_text());self.assertEqual((F/'patch-check.log').read_text().strip(),'exit=0')
if __name__=='__main__':unittest.main()

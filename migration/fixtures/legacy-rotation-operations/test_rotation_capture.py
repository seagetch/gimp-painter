#!/usr/bin/env python3
"""Validate measured source-callback evidence, never pretend it is GUI evidence."""
import csv,hashlib,json,math,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parent
class RotationEvidence(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.rows=list(csv.DictReader((ROOT/'source-captures/rotation.tsv').read_text().splitlines(),delimiter='\t'))
  cls.lookup={(r['scene'],r['step']):r for r in cls.rows}
 def angle(self,scene,step):return float(self.lookup[scene,step]['angle'])
 def test_shape_and_replay(self):
  self.assertEqual(len(self.rows),1232)
  self.assertEqual([int(r['seq']) for r in self.rows],list(range(1,1233)))
  self.assertEqual((ROOT/'source-captures/rotation.tsv').read_bytes(),(ROOT/'repeat-source-captures/rotation.tsv').read_bytes())
 def test_literal_half_steps(self):
  expected={1:0,2:15,3:15,4:0,5:0,6:345,8:30,9:345,10:0,11:0}
  for w in (100,101):
   for mirror in (0,1):
    for index,angle in expected.items():self.assertEqual(self.angle(f'stationary-w{w}-m{mirror}-a{index}','ctrl-add'),angle)
 def test_anchor_and_modifier_transitions(self):
  for w in (100,101):
   for m in (0,1):
    for a in range(18):
     scene=f'stationary-w{w}-m{m}-a{a}'
     start=self.lookup[scene,'start'];free=self.angle(scene,'free');snap=self.angle(scene,'ctrl-add')
     for step in ['ctrl-add','shift-remove','ctrl-remove','ctrl-readd','alt-add','ctrl-remove-alt','alt-remove']:
      r=self.lookup[scene,step];self.assertEqual(r['anchor'],start['anchor']);self.assertEqual(r['rotating'],'1')
     for step in ['ctrl-remove','ctrl-remove-alt','alt-remove']:self.assertEqual(self.angle(scene,step),free)
     for step in ['shift-remove','ctrl-readd','alt-add']:self.assertEqual(self.angle(scene,step),snap)
     stop=self.lookup[scene,'stop'];self.assertEqual(stop['rotating'],'0');self.assertEqual(stop['anchor'],'0');self.assertEqual(stop['grab'],'0')
 def test_statusbar_signed_half_steps(self):
  for w in (100,101):
   for mirror,left,right in [(0,-30,60),(1,60,-30)]:
    scene=f'stationary-w{w}-m{mirror}-a7'
    self.assertEqual(self.angle(scene,'left'),left);self.assertEqual(self.angle(scene,'right'),right)
   self.assertEqual(self.angle(f'stationary-w{w}-m0-a14','left'),-60)
   self.assertEqual(self.angle(f'stationary-w{w}-m0-a14','right'),30)
 def test_flip_and_reset(self):
  for r in self.rows:
   if r['step']=='flip':
    setup=self.lookup[r['scene'],'setup'];self.assertEqual(r['angle'],setup['angle']);self.assertNotEqual(r['mirror'],setup['mirror'])
   if r['step']=='reset':
    for k in ['angle','mirror','anchor','rotating']:self.assertEqual(float(r[k]),0)
    self.assertEqual(r['scale_requests'],'1');self.assertEqual(r['scale'],'1')
 def test_mirrored_quadrants(self):
  for w in (100,101):
   for mirror,expected in [(0,[270,180,90]),(1,[90,180,270])]:
    for p,a in enumerate(expected):self.assertEqual(self.angle(f'motion-w{w}-m{mirror}',f'p{p}-free'),a)
 def test_cairo_actual_matrices_roundtrip(self):
  for r in self.rows:
   vals={k:float(r[k]) for k in ['xx','yx','xy','yy','x0','y0','point_x','point_y','back_x','back_y','angle']}
   self.assertAlmostEqual(vals['back_x'],int(r['width'])//2+40,places=10)
   self.assertAlmostEqual(vals['back_y'],int(r['height'])//2+20,places=10)
   self.assertAlmostEqual(vals['xx']*vals['yy']-vals['xy']*vals['yx'],-1 if r['mirror']=='1' else 1,places=12)
 def test_body_identity(self):
  provenance=json.loads((ROOT/'source-provenance.json').read_text());harness=(ROOT/'source-capture.c').read_text()
  for name,entry in provenance['functions'].items():
   text=(ROOT/'original-sources'/entry['path']).read_text();body='\n'.join(text.splitlines()[entry['start_line']-1:entry['end_line']])
   self.assertIn(body,harness);self.assertEqual(hashlib.sha256(body.encode()).hexdigest(),entry['sha256'])
  for p,sha in provenance['sources'].items():self.assertEqual(hashlib.sha256((ROOT/'original-sources'/p).read_bytes()).hexdigest(),sha)
class GuiEvidence(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.rows=list(csv.DictReader((ROOT/'gui-captures/rotation.tsv').read_text().splitlines(),delimiter='\t'))
  cls.lookup={(r['scene'],r['step']):r for r in cls.rows}
 def test_complete_before_shutdown(self):
  self.assertEqual(len(self.rows),370)
  for directory in ('gui-captures','repeat-gui-captures'):
   report=json.loads((ROOT/directory/'display/results.json').read_text())
   self.assertEqual(report['transport_validation'],'PASS');self.assertEqual(report['authenticated_client_exit'],0);self.assertNotEqual(report['unauthenticated_client_exit'],0)
   self.assertEqual(report['command_exit'],-11) # Original teardown crash recorded, NOT a process pass.
   self.assertIn('ROTATION_CAPTURE_COMPLETE rows=370',(ROOT/directory/'display/command.log').read_text())
 def test_repeat_exact_states(self):
  self.assertEqual((ROOT/'gui-captures/rotation.tsv').read_bytes(),(ROOT/'repeat-gui-captures/rotation.tsv').read_bytes())
 def test_half_step_snap_literal(self):
  for mirror in (0,1):
   for index,expected in {1:0,2:15,3:15,4:0,5:0,6:345,7:30,8:345,9:0,10:0}.items():
    self.assertEqual(float(self.lookup[f'stationary-m{mirror}-i{index}','snap']['angle']),expected)
 def test_key_events_wait_for_motion_and_keep_drag(self):
  for mirror in (0,1):
   for i in range(11):
    scene=f'stationary-m{mirror}-i{i}';rows={step:r for (sc,step),r in self.lookup.items() if sc==scene}
    self.assertEqual(rows['ctrl-press']['angle'],rows['free']['angle'])
    self.assertEqual(rows['ctrl-release']['angle'],rows['snap']['angle'])
    self.assertEqual(rows['restored']['angle'],rows['free']['angle'])
    for step in ['free','ctrl-press','snap','shift-release','snap-without-shift','ctrl-release','restored']:
     self.assertEqual(rows[step]['rotating'],'1');self.assertEqual(rows[step]['grabbed'],'1');self.assertEqual(rows[step]['anchor'],rows['shift-middle']['anchor'])
    self.assertEqual(rows['middle-release']['rotating'],'0');self.assertEqual(rows['middle-release']['grabbed'],'0');self.assertEqual(rows['middle-release']['anchor'],'0')
 def test_actual_button_callbacks(self):
  for mirror,left,right in [(0,-30,60),(1,60,-30)]:
   scene=f'buttons-m{mirror}-i3';self.assertEqual(float(self.lookup[scene,'left']['angle']),left);self.assertEqual(float(self.lookup[scene,'right']['angle']),right)
  for r in self.rows:
   if r['step']=='flip':self.assertEqual(r['angle'],self.lookup[r['scene'],'setup']['angle'])
   if r['step']=='reset':self.assertEqual(r['angle'],'0');self.assertEqual(r['mirror'],'0')
 def test_gui_negative_fifteen_buttons(self):
  for mirror,left,right in [(0,-60,30),(1,30,-60)]:
   scene=f'buttons-m{mirror}-i4'
   self.assertEqual(float(self.lookup[scene,'setup']['angle']),-15)
   self.assertEqual(float(self.lookup[scene,'left']['angle']),left)
   self.assertEqual(float(self.lookup[scene,'right']['angle']),right)
 def test_fractional_event_coordinate_truncation(self):
  # Original callback takes gint coordinates: recorded 533.9/437.9 become
  # 533/437 relative to integer center 507/337, giving vector (26,100).
  # Assert against this independently computed geometric expectation and
  # distinguish it from the untruncated floating vector (26.9,100.9).
  for mirror in (0,1):
   row=self.lookup[f'quadrants-m{mirror}','fractional']
   cx=int(row['width'])//2;cy=int(row['height'])//2
   x=float(row['x']);y=float(row['y']);direction=-1 if mirror else 1
   angle=(-math.degrees(math.atan2(direction*(int(x)-cx),int(y)-cy)))%360
   untruncated=(-math.degrees(math.atan2(direction*(x-cx),y-cy)))%360
   self.assertAlmostEqual(float(row['angle']),angle,places=10)
   self.assertGreater(abs(float(row['angle'])-untruncated),0.1)
   snap=self.lookup[f'quadrants-m{mirror}','fractional-snap']
   self.assertEqual(float(snap['angle']),15 if mirror else 345)
 def test_mirror_motion_and_coordinate_roundtrip(self):
  for mirror,angles in [(0,[270,180,90]),(1,[90,180,270])]:
   for step,value in zip(['right','top','left'],angles):self.assertEqual(float(self.lookup[f'quadrants-m{mirror}',step]['angle']),value)
  for r in self.rows:
   self.assertAlmostEqual(float(r['back_x']),float(r['device_x']),places=10);self.assertAlmostEqual(float(r['back_y']),float(r['device_y']),places=10)
if __name__=='__main__':unittest.main(verbosity=2)

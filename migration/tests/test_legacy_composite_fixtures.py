"""Sealed original-app evidence checks, not a replacement legacy runtime."""
import hashlib,json,tarfile,unittest
from pathlib import Path
D=Path(__file__).resolve().parents[1]/'fixtures/legacy-composite'
class CompositeEvidence(unittest.TestCase):
 def test_sealed_files(self):
  for row in json.loads((D/'manifest.json').read_text())['files']:
   p=D/row['path'];self.assertEqual(p.stat().st_size,row['bytes']);self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),row['sha256'])
 def test_real_capture_and_presets(self):
  r=json.loads((D/'capture-report.json').read_text());self.assertTrue(r['capture_processes_exit0']);self.assertTrue(r['reference_install_untouched'])
  for scene in ('anime','watercolor'):
   log=(D/scene/'capture.log').read_text();self.assertIn('ORIGINAL_PRESET_APPLIED ',log);self.assertIn('COMPOSITE_CAPTURE_COMPLETE scene='+scene,log)
   preset=json.loads((D/('new-anime-layer.json' if scene=='anime' else 'watercolor.json')).read_text());self.assertEqual(preset['replacement-layer'][0]['type'],'group')
   for state in ('initial','edited'):self.assertTrue((D/scene/f'{scene}-{state}.xcf').read_bytes().startswith(b'gimp xcf '))
 def test_anime_reopen_and_pixels(self):
  for a,b in [('initial','initial-reopened'),('redo','edited-reopened'),('recolored','undo'),('opacity','redo')]:
   self.assertEqual((D/'anime'/f'anime-{a}.projection.rgba').read_bytes(),(D/'anime'/f'anime-{b}.projection.rgba').read_bytes())
  log=(D/'anime/capture.log').read_text()
  for stage in ('initial','initial-reopened','edited-reopened'):
   self.assertTrue(any('MODEL stage=anime-'+stage+' ' in line and 'type=GimpCloneLayer name=mask' in line and 'mode=27' in line and 'source=paint source' in line for line in log.splitlines()))
  self.assertNotEqual((D/'anime/anime-initial.projection.rgba').read_bytes(),(D/'anime/anime-recolored.projection.rgba').read_bytes())
  for state in ('initial','edited'):
   self.assertEqual((D/'anime'/f'independent-{state}/exit-code').read_text().strip(),'0');self.assertIn('INDEPENDENT_ORIGINAL_COMPOSITE_REOPEN_PASS',(D/'anime'/f'independent-{state}/reopen.log').read_text())
 def test_all_anime_layer_bytes_survive(self):
  for a,b in [('initial','initial-reopened'),('redo','edited-reopened')]:
   files=list((D/'anime').glob(f'anime-{a}.layer-*.pixels'));self.assertEqual(len(files),5)
   for p in files:self.assertEqual(p.read_bytes(),p.with_name(p.name.replace(f'anime-{a}.',f'anime-{b}.',1)).read_bytes())
 def test_watercolor_negative_not_success(self):
  for state in ('initial','edited'):
   self.assertEqual((D/'watercolor'/f'independent-{state}/exit-code').read_text().strip(),'139');self.assertNotIn('INDEPENDENT_ORIGINAL_COMPOSITE_REOPEN_PASS',(D/'watercolor'/f'independent-{state}/reopen.log').read_text())
  log=(D/'watercolor/capture.log').read_text();self.assertIn('procedure=plug-in-edge',log);self.assertIn('index=3 type=gdouble value=2.000000',log)
 def test_lossless_historical_archive(self):
  rows=json.loads((D/'historical-flat-seed-members.json').read_text())['members']
  self.assertEqual(len(rows),102)
  with tarfile.open(D/'historical-flat-seed.tar.gz','r:gz') as archive:
   self.assertEqual(sorted(archive.getnames()),sorted(row['path'] for row in rows))
   for row in rows:
    member=archive.getmember(row['path']);self.assertTrue(member.isfile())
    data=archive.extractfile(member).read();self.assertEqual(len(data),row['bytes']);self.assertEqual(hashlib.sha256(data).hexdigest(),row['sha256'])
 def test_nonuniform_watercolor_effect(self):
  root=D/'watercolor'
  data=lambda stage:(root/f'watercolor-{stage}.projection.rgba').read_bytes()
  self.assertNotEqual(data('recolored'),data('opacity'))
  self.assertEqual(sum(a!=b for a,b in zip(data('recolored'),data('opacity'))),480)
  self.assertEqual(data('recolored'),data('undo'));self.assertEqual(data('opacity'),data('redo'))
  with tarfile.open(D/'historical-flat-seed.tar.gz','r:gz') as archive:
   self.assertEqual(archive.extractfile('watercolor/watercolor-recolored.projection.rgba').read(),archive.extractfile('watercolor/watercolor-opacity.projection.rgba').read())
  source=(root/'watercolor-initial.layer-paint source.pixels').read_bytes()
  self.assertEqual(len(set(zip(*[iter(source)]*4))),2)
 def test_live_opacity_undo_redo(self):
  for scene in ('anime','watercolor'):
   log=(D/scene/'capture.log').read_text()
   for stage,opacity in [('opacity','0.59999999999999998'),('undo','1' if scene=='anime' else '0.20000000000000001'),('redo','0.59999999999999998')]:
    self.assertIn('STAGE '+scene+'-'+stage,log)
    self.assertTrue(any('MODEL stage='+scene+'-'+stage+' ' in line and ('name=shadow' if scene=='anime' else 'type=GimpFilterLayer name=mask') in line and 'opacity='+opacity+' ' in line for line in log.splitlines()))
   self.assertEqual((D/scene/f'{scene}-opacity.projection.rgba').read_bytes(),(D/scene/f'{scene}-redo.projection.rgba').read_bytes())
if __name__=='__main__':unittest.main()

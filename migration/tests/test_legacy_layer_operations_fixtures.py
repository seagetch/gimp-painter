"""Validate sealed actual-old-runtime records; does not execute or establish port parity."""
import hashlib
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1] / 'fixtures' / 'legacy-layer-operations'

def read(name):
    return json.loads((ROOT / name).read_text())

def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))

class LegacyLayerOperations(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.obs = read('observations.json')

    def obj(self, scene, step, id):
        return next(x for x in self.obs[scene]['objects'] if x['step'] == step and x['id'] == id)

    def pix(self, scene, step, id):
        return next(x['sha256'] for x in self.obs[scene]['pixels'] if x['step'] == step and x['id'] == id)

    def test_manifest(self):
        manifest = read('manifest.json')['files']
        self.assertEqual({x['path'] for x in manifest}, {str(p.relative_to(ROOT)) for p in ROOT.rglob('*') if p.is_file() and p.name != 'manifest.json'})
        for x in manifest:
            data = (ROOT / x['path']).read_bytes()
            self.assertEqual(len(data), x['bytes'], x['path'])
            self.assertEqual(hashlib.sha256(data).hexdigest(), x['sha256'], x['path'])

    def test_provenance_and_completion(self):
        report = read('capture-report.json')
        self.assertEqual(report['source_commit'], 'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(report['state_counts'], {'clone': 30, 'filter': 17})
        self.assertEqual(report['pixel_count'], 329)
        self.assertTrue(report['reference_install_untouched'])
        self.assertTrue(all(report['completed'].values()))
        for x in read('source-provenance.json'):
            if x['path'] not in ('app/core/gimpfilterlayer.cpp', 'app/tests/test-xcf.c'):
                self.assertTrue(x['unchanged'], x['path'])
        self.assertIn('exit=0', (ROOT / 'patch-check.log').read_text())

    def test_log_derived_observations(self):
        prefixes = ('STATE ', 'OBJECT ', 'EVENT ', 'PIXELS ', 'ARG ', 'ACTION ', 'RESULT ', 'HISTORY ', 'SAVE ', 'REGISTER ', 'OPERATIONS_COMPLETE')
        for scene, observation in self.obs.items():
            lines = (ROOT / scene / 'capture.log').read_text().splitlines()
            self.assertEqual(observation['exit_code'], int((ROOT / scene / 'exit-code').read_text()))
            self.assertEqual(observation['exit_code'], 0)
            self.assertEqual(lines.count(f'OPERATIONS_COMPLETE scene={scene}'), 1)
            self.assertTrue(read('capture-report.json')['completed'][scene])
            self.assertEqual(observation['trace'], [x for x in lines if x.startswith(prefixes)])
            for key, prefix in [('states','STATE '), ('objects','OBJECT '), ('pixels','PIXELS '), ('events','EVENT '), ('args','ARG ')]:
                self.assertEqual(observation[key], [fields(x) for x in lines if x.startswith(prefix)])
            states = [x['step'] for x in observation['states']]
            self.assertEqual(len(states), len(set(states)))
            for state in states:
                self.assertTrue((ROOT / scene / f'{state}.xcf').read_bytes().startswith(b'gimp xcf '))
                self.assertIn(f'SAVE step={state} success=1', lines)
            for line in lines:
                if line.startswith('RESULT '): self.assertEqual(fields(line)['success'], '1')

    def test_pixels_and_repeated_capture(self):
        for scene, obs in self.obs.items():
            for x in obs['pixels']:
                data = (ROOT / scene / f"{x['step']}.{x['id']}.pixels").read_bytes()
                self.assertEqual(len(data), int(x['width']) * int(x['height']) * int(x['bpp']))
                self.assertEqual(hashlib.sha256(data).hexdigest(), x['sha256'])
            comparisons = read(f'{scene}/repeat/comparison.json')
            self.assertEqual(len(comparisons), len(obs['pixels']))
            repeat = [fields(x) for x in (ROOT / scene / 'repeat/capture.log').read_text().splitlines() if x.startswith('PIXELS ')]
            self.assertEqual(obs['pixels'], repeat)
            for x in comparisons:
                self.assertTrue(x['equal'])
                self.assertTrue(x['repeat_log_hash_matches'])
                self.assertEqual(x['primary_sha256'], x['repeat_sha256'])
            self.assertEqual(int((ROOT / scene / 'repeat/exit-code').read_text()), 0)

    def test_move_and_reorder_identity(self):
        for step in ['baseline','move-source','move-source-undo','move-source-redo','move-clone','move-clone-undo','move-clone-redo','reorder-source','reorder-source-undo','reorder-source-redo']:
            source = self.obj('clone',step,'source'); clone = self.obj('clone',step,'clone')
            self.assertEqual(clone['source_pointer'], source['pointer'])
        self.assertEqual(self.obj('clone','move-source','source')['xy'], '13,15')
        self.assertEqual(self.obj('clone','move-source','clone')['xy'], '36,8')
        self.assertEqual(self.obj('clone','move-source-undo','source')['xy'], '8,8')
        self.assertEqual(self.obj('clone','move-clone','clone')['xy'], '33,12')
        self.assertEqual(self.obj('clone','move-clone-undo','clone')['xy'], '36,8')
        self.assertEqual(self.obj('clone','reorder-source','source')['index'], '0')
        self.assertEqual(self.obj('clone','reorder-source-undo','source')['index'], '2')

    def test_duplicate_remapping_and_update_routing(self):
        for step in ['duplicate-group','duplicate-group-undo','duplicate-group-redo','edit-original','edit-copy','edit-external']:
            self.assertEqual(self.obj('clone',step,'direct-copy')['source_pointer'], self.obj('clone',step,'source')['pointer'])
            self.assertEqual(self.obj('clone',step,'copy-clone')['source_pointer'], self.obj('clone',step,'copy-source')['pointer'])
            self.assertNotEqual(self.obj('clone',step,'copy-source')['pointer'], self.obj('clone',step,'source')['pointer'])
            for name in ['external-clone','copy-external-clone']:
                self.assertEqual(self.obj('clone',step,name)['source_pointer'], self.obj('clone',step,'external')['pointer'])
        for step, source, clones in [('edit-original','source',['clone','direct-copy']),('edit-copy','copy-source',['copy-clone']),('edit-external','external',['external-clone','copy-external-clone'])]:
            for clone in clones:
                self.assertEqual(self.pix('clone',step,source), self.pix('clone',step,clone))
                self.assertTrue(any(x['step']==step and x['id']==clone and x['event']=='update' for x in self.obs['clone']['events']))
        self.assertNotEqual(self.pix('clone','edit-original','source'),self.pix('clone','edit-original','copy-source'))
        self.assertNotEqual(self.pix('clone','edit-copy','source'),self.pix('clone','edit-copy','copy-source'))
        self.assertIn("gimp_drawable_project_region: assertion 'gimp_item_is_attached", (ROOT/'clone/capture.log').read_text())

    def test_delete_undo_redo_and_restored_updates(self):
        for prefix, target in [('delete-source','source'),('delete-clone','clone'),('delete-group','group-copy')]:
            pointer = self.obj('clone',prefix,target)['pointer']
            for suffix, attached in [('', '0'),('-undo','1'),('-redo','0')]:
                o = self.obj('clone',prefix+suffix,target)
                self.assertEqual(o['pointer'],pointer)
                self.assertEqual(o['alive'],'1')
                self.assertEqual(o['attached'],attached)
        for step in ['delete-source','delete-source-undo','delete-source-redo','restore-source','edit-restored']:
            for clone in ['clone','direct-copy']:
                self.assertEqual(self.obj('clone',step,clone)['source_pointer'],self.obj('clone',step,'source')['pointer'])
        for clone in ['clone','direct-copy']:
            self.assertEqual(self.pix('clone','edit-restored',clone),self.pix('clone','edit-restored','source'))
            self.assertTrue(any(x['step']=='edit-restored' and x['id']==clone and x['event']=='update' for x in self.obs['clone']['events']))

    def test_filter_definitions_references_and_completion(self):
        for o in self.obs['filter']['objects']:
            if o['id'].startswith('filter'):
                self.assertEqual(o['procedure'],'plug-in-edge')
                self.assertGreater(int(o['completions']),0)
                if o['attached']=='1': self.assertEqual(o['pending'],'0')
                args={x['index']:x for x in self.obs['filter']['args'] if x['id']==o['id'] and x['step']==o['step']}
                self.assertEqual(set(args),set('012345'))
                self.assertEqual(args['2']['value'],o['core_id'])
                self.assertEqual(args['2']['type'],'GimpDrawableID')
                self.assertEqual([args[i]['value'] for i in '0345'],['1','2.000000','1','0'])
        self.assertNotEqual(self.pix('filter','baseline','source'),self.pix('filter','baseline','filter'))
        self.assertNotEqual(self.pix('filter','delete-source','filter'),self.pix('filter','delete-source-undo','filter'))
        self.assertEqual(self.pix('filter','delete-source','filter'),self.pix('filter','delete-source-redo','filter'))
        self.assertEqual(self.pix('filter','move-source','projection'),self.pix('filter','move-source-redo','projection'))
        for prefix,target in [('duplicate-filter','filter-copy'),('delete-source','source'),('delete-filter','filter-copy')]:
            for suffix,attached in [('', '1' if prefix=='duplicate-filter' else '0'),('-undo','0' if prefix=='duplicate-filter' else '1'),('-redo','1' if prefix=='duplicate-filter' else '0')]:
                self.assertEqual(self.obj('filter',prefix+suffix,target)['attached'],attached)

    def test_reader_successes_and_negative_filter_records(self):
        self.assertEqual(len(self.obs['clone']['independent_reopens']),5)
        self.assertEqual(len(self.obs['filter']['independent_reopens']),4)
        for scene, expected in [('clone',0),('filter',139)]:
            for name, code in self.obs[scene]['independent_reopens'].items():
                self.assertEqual(code,expected)
                log=(ROOT/scene/name/'reopen.log').read_text()
                if scene=='clone': self.assertIn('INDEPENDENT_ORIGINAL_REOPEN_PASS',log)
                else: self.assertNotIn('INDEPENDENT_ORIGINAL_REOPEN_PASS',log)

if __name__=='__main__': unittest.main()

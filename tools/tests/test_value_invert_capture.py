#!/usr/bin/env python3
"""Pure-Python control tests, never genuine runtime/owner evidence.

The synthetic scalar and event examples here exercise rejection logic only.
They are not written to any capture directory or substituted for oracle bytes.
"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import capture_legacy_value_invert as capture
import check_value_invert_evidence as evidence


def event(kind,progress=0,starts=1,ends=0,merges=0):
    return ['unit-only',kind,str(starts),str(ends),str(progress),str(merges)]


class PlanControls(unittest.TestCase):
    def test_finite_complete_safe_plan(self):
        base=capture.cases();large=capture.cases(True)
        self.assertEqual((len(base),sum(c['live'] for c in base),sum(c['writer'] for c in base)),(140,90,2))
        self.assertEqual(len(large),2)
        self.assertEqual({c['components'] for c in base+large},{3,4})
        self.assertEqual(len({c['id'] for c in base+large}),142)
        self.assertFalse(any('mode' in c for c in base+large))
        self.assertEqual({c['components'] for c in base if c['writer']},{3,4})
        self.assertTrue(all(c['width']*c['height']<2147483647 for c in base+large))
        self.assertTrue(all(c['live'] and c['phase']==2 and c['rerun']==2 for c in large))

    def test_plan_and_header_deterministic(self):
        a=capture.cases();b=capture.cases()
        self.assertEqual(json.dumps(a,sort_keys=True),json.dumps(b,sort_keys=True))
        a[0]['width']=123
        self.assertNotEqual(a,b)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);capture.write_header(p,b)
            first=(p/'legacy-value-invert-cases.h').read_bytes()
            capture.write_header(p,capture.cases())
            self.assertEqual(first,(p/'legacy-value-invert-cases.h').read_bytes())
            self.assertIn(b'offset_y, live',first)
            self.assertNotIn(b'threshold',first)

    def test_control_masks_and_late_context_coverage(self):
        base=capture.cases()
        self.assertEqual({c['phase'] for c in base},{0,1,2})
        self.assertEqual(sum(c['phase']==1 for c in base),14)
        self.assertEqual(sum(c['phase']==2 for c in base),14)
        self.assertTrue(any(c['selection']==3 and c['phase']==0 for c in base))
        self.assertTrue(any(c['final_selection']==3 and c['rerun']==2 for c in base))
        self.assertTrue(any(c['final_lock']==1 for c in base))
        for c in base:
            mask=evidence.generated_mask(c,c['selection'])
            self.assertEqual(len(mask),c['image_width']*c['image_height'])
        for kind,expected in ((5,(1,16,16,112,112)),(6,(1,63,63,65,65)),(7,(1,1,1,2,2))):
            c=next(t for t in base if t['selection']==kind)
            self.assertEqual(evidence.mask_roi(evidence.generated_mask(c,kind),c['image_width'],
                c['image_height'],c['offset_x'],c['offset_y'],c['width'],c['height']),expected)

    def test_source_generator_deterministic_and_distinguishing(self):
        for channels in (3,4):
            c=next(c for c in capture.cases() if c['components']==channels and c['width']==17)
            a=evidence.generated_source(c,0);b=evidence.generated_source(c,1)
            self.assertEqual(a,evidence.generated_source(c,0))
            self.assertNotEqual(a,b)
            self.assertEqual(len(a),17*13*channels)
            self.assertEqual(a[:3],bytes([0,0,0]))
            self.assertEqual(a[channels:channels+3],bytes([255,255,255]))
            self.assertEqual(a[2*channels:2*channels+3],bytes([90,90,90]))
            if channels==4:
                self.assertIn(0,a[3::4]);self.assertIn(255,a[3::4])
                self.assertTrue(any(a[i+3]==0 and evidence.raw_pixel(a[i:i+4])[:3]!=a[i:i+3]
                                    for i in range(0,len(a),4)))

    def test_hidden_pattern_and_variant(self):
        c=next(c for c in capture.cases() if c['zero_alpha'])
        self.assertFalse(any(evidence.generated_source(c,0)[3::4]))
        self.assertTrue(any(evidence.generated_source(c,1)[3::4]))


class PhaseControls(unittest.TestCase):
    def test_real_merge_entry_control(self):
        evidence.check_phase([event('start'),event('merge-entry-mutate'),
                              event('merge-before',merges=1)],1,1,'unit')

    def test_real_mid_work_control(self):
        evidence.check_phase([event('start'),event('progress',.25),
                              event('progress-mid-mutate',.25),event('merge-before',merges=1)],2,1,'unit')

    def test_no_phase_control(self):
        evidence.check_phase([event('progress',1),event('merge-before',merges=1)],0,0,'unit')

    def reject(self,events,phase=2,mutated=1):
        with self.assertRaises(ValueError):evidence.check_phase(events,phase,mutated,'unit')

    def test_progress_one_cannot_prove_midwork(self):
        for p in (0,1,-1,2,float('nan'),float('inf')):
            self.reject([event('progress',p),event('progress-mid-mutate',p)])

    def test_progress_one_cannot_prove_merge_entry(self):
        self.reject([event('progress',1),event('progress-mid-mutate',1),event('merge-before')],1)
        self.reject([event('merge-entry-mutate',1),event('merge-before')],1)

    def test_inferred_or_missing_hook_rejected(self):
        self.reject([event('progress',.25),event('merge-before')])
        self.reject([event('progress-mid-mutate',.25)])
        self.reject([event('progress',.25),event('progress-mid-mutate',.5)])

    def test_duplicate_late_or_wrong_count_hook_rejected(self):
        good=[event('progress',.25),event('progress-mid-mutate',.25)]
        self.reject(good+good)
        self.reject([event('end',ends=1)]+good)
        self.reject([event('merge-before',merges=1)]+good)
        self.reject([event('progress',.25),event('progress-mid-mutate',.25,starts=2)])
        self.reject(good,mutated=0)

    def test_merge_entry_must_be_adjacent(self):
        self.reject([event('merge-entry-mutate'),event('progress'),event('merge-before')],1)
        self.reject([event('merge-entry-mutate')],1)

    def test_unknown_phase_rejected(self):
        self.reject([],3,0)
        self.reject([event('progress-mid-mutate',.25)],0,0)


class ArithmeticControls(unittest.TestCase):
    def test_extrema_ties_rounding_and_all_alpha(self):
        for value in (0,1,90,127,128,254,255):
            self.assertEqual(evidence.raw_pixel(bytes([value]*3)),bytes([255-value]*3))
        for original,expected in (([200,200,20],[55,55,6]),([20,200,20],[6,55,6]),
                                  ([255,128,0],[0,0,0]),([1,2,3],[84,168,252])):
            self.assertEqual(evidence.raw_pixel(bytes(original)),bytes(expected))
            for alpha in range(256):
                self.assertEqual(evidence.raw_pixel(bytes(original+[alpha])),bytes(expected+[alpha]))

    def test_unsafe_capture_domains_rejected(self):
        for channels in (0,1,2,5):
            with self.assertRaises(ValueError):evidence.raw_pixel(bytes(channels))

    def test_hidden_raw_transforms_but_merge_retains(self):
        source=bytes([20,200,20,0]);raw=evidence.raw_pixel(source)
        self.assertNotEqual(source,raw)
        self.assertGreater(evidence.check_merge(source,raw,bytes([255]),source,4,15),0)
        with self.assertRaises(ValueError):evidence.check_merge(source,raw,bytes([255]),raw,4,15)

    def test_rgb_and_mask_inactive_controls(self):
        source=bytes([20,200,20]);raw=evidence.raw_pixel(source)
        evidence.check_merge(source,raw,bytes([255]),raw,3,7)
        evidence.check_merge(source,raw,bytes([0]),source,3,7)
        evidence.check_merge(source,raw,bytes([255]),source,3,0)


class ProvenanceControls(unittest.TestCase):
    def test_source_pin_accepts_only_matching_original_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp);(source/'unit.c').write_bytes(b'unchanged original\n')
            callback=source/'plug-ins/common/value-invert.c';callback.parent.mkdir(parents=True);callback.write_bytes(b'callback\n')
            callback_hash=hashlib.sha256(callback.read_bytes()).hexdigest()
            digest=hashlib.sha256((source/'unit.c').read_bytes()).hexdigest()
            with mock.patch.object(capture,'SOURCES',('unit.c','plug-ins/common/value-invert.c')),mock.patch.object(capture,'VALUE_INVERT_SOURCE_SHA256',callback_hash),mock.patch.object(capture.subprocess,'check_output',
                side_effect=[capture.COMMIT+'\n',b'unchanged original\n',b'callback\n']):
                self.assertEqual(capture.verify_sources(source),{'unit.c':digest,'plug-ins/common/value-invert.c':callback_hash})
            with mock.patch.object(capture,'SOURCES',('unit.c','plug-ins/common/value-invert.c')),mock.patch.object(capture,'VALUE_INVERT_SOURCE_SHA256',callback_hash),mock.patch.object(capture.subprocess,'check_output',
                side_effect=[capture.COMMIT+'\n',b'different original\n']):
                with self.assertRaisesRegex(ValueError,'Changed oracle source'):capture.verify_sources(source)

    def test_wrong_source_revision_rejected_before_hash_read(self):
        with mock.patch.object(capture.subprocess,'check_output',return_value='wrong\n'),\
             mock.patch.object(capture,'sha') as digest:
            with self.assertRaisesRegex(ValueError,'Wrong legacy'):capture.verify_sources(Path('/unit'))
            digest.assert_not_called()

    def test_required_source_coverage(self):
        for name in ('plug-ins/common/value-invert.c','libgimp/gimppixelrgn.c',
                     'app/core/gimpfilterlayer.cpp','app/paint-funcs/paint-funcs.c',
                     'app/core/gimpdrawable-shadow.c','app/xcf/xcf-save.c','app/xcf/xcf-write.c'):
            self.assertIn(name,capture.SOURCES)
        self.assertFalse(any('threshold' in p or 'blinds' in p for p in capture.SOURCES))

    def test_destinations_independent_and_nonempty(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);source=p/'source';out=p/'out';work=p/'work'
            capture.validate_destinations(source,out,work)
            for bad_out,bad_work in ((out,out),(out,out/'nested'),(source/'out',work)):
                with self.assertRaises(ValueError):capture.validate_destinations(source,bad_out,bad_work)
            out.mkdir();(out/'existing').write_text('preserve')
            with self.assertRaises(ValueError):capture.validate_destinations(source,out,work)

    def test_unsealed_archive_fails_closed(self):
        with self.assertRaisesRegex(ValueError,'not yet been sealed'):
            evidence.read_archive('/nonexistent','unit','','',100,10,100)

    def test_signature_is_three_strict_typed_slots(self):
        good=b'0\trun-mode\tGimpParamInt32\tGimpInt32\n1\timage\tGimpParamImageID\tGimpImageID\n2\tdrawable\tGimpParamDrawableID\tGimpDrawableID\n'
        evidence.check_signature({'signature.tsv':good})
        for bad in (good+b'3\tmax-p\tGimpParamInt32\tGimpInt32\n',good.replace(b'GimpInt32',b'gdouble'),good+good.splitlines()[0]+b'\n'):
            with self.assertRaises(ValueError):evidence.check_signature({'signature.tsv':bad})

    def test_unexpected_runtime_diagnostics_rejected(self):
        evidence.check_runtime_diagnostics(b'VALUE_INVERT_CASE_DONE=unit\n')
        with self.assertRaises(ValueError):evidence.check_runtime_diagnostics(b'CRITICAL unexpected pixel error\n')


class ProjectionControls(unittest.TestCase):
    def test_finite_plan_and_subset_sealing(self):
        import capture_legacy_value_invert_projection as producer
        import check_value_invert_projection_evidence as checker
        plan=producer.cases()
        self.assertEqual(len(plan),16)
        self.assertEqual({c['components'] for c in plan},{3,4})
        self.assertEqual(sum(c['seeded'] for c in plan),8)
        checker.check_plan(plan,full=True)
        for c in plan: checker.check_plan([c])
        with self.assertRaises(ValueError):checker.check_plan(plan[:-1],full=True)
        bad=copy.deepcopy(plan);bad[0]['components']=2
        with self.assertRaises(ValueError):checker.check_plan(bad)
        with self.assertRaises(ValueError):producer.cases(True)

    def test_projection_observed_phase_order(self):
        import check_value_invert_projection_evidence as checker
        events=[]
        for run in range(1,4):
            events += [event('projection-copy',starts=run-1,ends=run-1,merges=run-1),
                       event('start',starts=run,ends=run-1,merges=run-1),
                       event('progress',.25,starts=run,ends=run-1,merges=run-1),
                       event('merge-before',starts=run,ends=run-1,merges=run),
                       event('merge-after',starts=run,ends=run-1,merges=run),
                       event('end',starts=run,ends=run,merges=run)]
        checker.check_events(events,'unit-only')
        for first,second in ((0,1),(3,4),(4,5),(5,6)):
            bad=copy.deepcopy(events);bad[first],bad[second]=bad[second],bad[first]
            with self.assertRaises(ValueError):checker.check_events(bad,'unit-only')
        with self.assertRaises(ValueError):checker.check_events(events[:-1],'unit-only')

    def test_projection_header_determinism_and_original_hooks(self):
        import capture_legacy_value_invert_projection as producer
        with tempfile.TemporaryDirectory() as tmp:
            work=Path(tmp);producer.write_header(work,producer.cases())
            first=(work/'legacy-value-invert-projection-cases.h').read_bytes()
            producer.write_header(work,producer.cases())
            self.assertEqual(first,(work/'legacy-value-invert-projection-cases.h').read_bytes())
        for filename in ('app/core/gimpgrouplayer.c','app/core/gimpprojection.c','app/core/gimpchannel.c'):
            self.assertIn(filename,producer.SOURCES)


class IsolatedProfileControls(unittest.TestCase):
    def test_relative_prefix_symlink_resolves_outside_temporary_profile(self):
        import capture_legacy_value_invert_projection as projection
        from types import SimpleNamespace
        for producer in (capture,projection):
            prefix=Path('relative-legacy-prefix')
            expected=(prefix/'lib/gimp/2.0/plug-ins/value-invert').resolve()
            def inspect(command,**kwargs):
                link=Path(kwargs['env']['HOME'])/'plugins/value-invert'
                self.assertTrue(link.readlink().is_absolute())
                self.assertEqual(link.readlink(),expected)
                self.assertEqual(link.resolve(),expected)
                return SimpleNamespace(returncode=0)
            with tempfile.TemporaryDirectory() as tmp, mock.patch.object(producer.subprocess,'run',side_effect=inspect):
                code,_=producer.run(Path('/unit/no-execution'),prefix,Path(tmp),1)
                self.assertEqual(code,0)


class ArchiveControls(unittest.TestCase):
    def test_canonical_archive_determinism(self):
        import seal_legacy_value_invert_capture as seal
        first=seal.archive_bytes({'b.txt':b'unit only b','a.txt':b'unit only a'},'control',8*1024*1024)
        second=seal.archive_bytes({'a.txt':b'unit only a','b.txt':b'unit only b'},'control',8*1024*1024)
        self.assertEqual(first,second)

    def test_missing_real_report_cannot_be_sealed(self):
        import seal_legacy_value_invert_capture as seal
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError,'no completed report'):
                seal.inspect_directory(Path(tmp),'base')


class WriterControls(unittest.TestCase):
    """Synthetic parser controls only; never old-writer fixture observations."""
    def setUp(self):
        import struct
        self.pack=struct.pack
        self.case={'id':'unit-only-not-a-capture'}
        literal=evidence.LITERAL
        self.capsule=self.pack('>I',len(literal))+literal
        for tag,payload in ((2,self.pack('>i',1)),(1,b''),(10,b''),(0,b'')):
            self.capsule+=self.pack('>II',tag,len(payload))+payload
        self.assertEqual(len(self.capsule),56)
        self.data=b'unit-only-prefix'+self.pack('>II',32,56)+self.capsule+b'unit-only-tail'

    def files(self,data=None,capsule=None):
        return {self.case['id']+'.xcf':self.data if data is None else data,
                self.case['id']+'.definition.raw':self.capsule if capsule is None else capsule}

    def test_exact_predicted_three_slot_shape(self):
        shape=evidence.inspect_writer(self.files(),self.case)
        self.assertEqual(shape['payload_size'],56)
        self.assertEqual([(r['tag'],r['declared_size']) for r in shape['records']],evidence.WRITER_FIELDS)

    def test_wrong_name_duplicate_and_old_four_argument_shape_rejected(self):
        for data in (self.data.replace(b'vinvert',b'max-rgb'),self.data+self.data,
                     self.data.replace(self.pack('>II',32,56),self.pack('>II',32,68))):
            with self.assertRaises(ValueError):evidence.inspect_writer(self.files(data),self.case)

    def test_wrong_tag_length_payload_and_terminator_rejected(self):
        for offset,value in ((20,3),(24,8),(28,0),(32,2),(36,4),(40,11),(44,4),(48,2),(52,4)):
            raw=bytearray(self.capsule);raw[offset:offset+4]=self.pack('>I',value)
            data=self.data.replace(self.capsule,bytes(raw))
            with self.assertRaises(ValueError):evidence.inspect_writer(self.files(data,bytes(raw)),self.case)

    def test_retained_capsule_must_be_exact_observation(self):
        with self.assertRaises(ValueError):evidence.inspect_writer(self.files(capsule=self.capsule+b'tail'),self.case)

    def test_extractor_only_retains_existing_writer_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'unit-only.xcf';path.write_bytes(self.data)
            shape=capture.writer_shape(path)
            self.assertEqual(shape['payload_size'],56)
            self.assertEqual(path.with_suffix('.definition.raw').read_bytes(),self.capsule)
            self.assertEqual(path.read_bytes(),self.data)


if __name__=='__main__':unittest.main()

#!/usr/bin/env python3
"""Pure-Python capture-preparation tests. Never build or run an application.

Constructed parser/raster inputs below are unit controls, NOT genuine old Sobel
fixtures. The 304-record test reads the already sealed generic legacy context
corpus; it is not a new Sobel execution or acceptance claim.
"""
from pathlib import Path
import ast
import hashlib
import json
import struct
import sys
import tarfile
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import capture_legacy_sobel as capture
import capture_legacy_sobel_projection as projection
import check_sobel_evidence as check
import check_sobel_projection_evidence as projection_check


class SobelCapturePreparation(unittest.TestCase):
    def test_finite_base_and_large_plans(self):
        cases=capture.cases()
        self.assertEqual((len(cases),sum(c['live'] for c in cases),sum(c['writer'] for c in cases)),(410,228,64))
        self.assertEqual(len({c['id'] for c in cases}),len(cases))
        self.assertEqual(len(capture.COLUMNS),33)
        self.assertEqual(capture.COLUMNS,check.COLUMNS)
        for c in cases:
            self.assertTrue(all(-2**31<=c[k]<2**31 for k in ('horizontal','vertical','keep_sign')))
            self.assertLess(c['width']*c['height']*c['components'],2**31)
            if c['phase']: self.assertNotEqual(c['selection'],0)
        for source_channels in range(1,5):
            writer=[c for c in cases if c['writer'] and c['components']==source_channels and c['rerun']==1]
            canonical={tuple(c[k] for k in ('horizontal','vertical','keep_sign')) for c in writer
                       if all(c[k] in (0,1) for k in ('horizontal','vertical','keep_sign'))}
            self.assertEqual(len(canonical),8)
        large=capture.cases(True)
        self.assertEqual([c['components'] for c in large],[2,4])
        self.assertTrue(all(c['width']*c['height']>1048576 and c['width']>1024 for c in large))

    def test_projection_plan(self):
        cases=projection.cases()
        self.assertEqual(len(cases),24)
        self.assertEqual(sum(c['halo'] for c in cases),8)
        self.assertEqual(sum(c['grouped'] for c in cases),8)
        self.assertEqual(sum(c['seeded'] for c in cases),12)
        self.assertEqual(len(projection.COLUMNS),14)
        self.assertEqual(len(projection_check.COPY_COLUMNS),20)

    def test_preparation_headers_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            capture.write_header(directory,capture.cases())
            projection.write_header(directory,projection.cases())
            for name,count in (('legacy-sobel-cases.h',410),('legacy-sobel-projection-cases.h',24)):
                text=(directory/name).read_text()
                self.assertEqual(text.count('\n  {'),count)
                self.assertIn('gint width, height',text)
                self.assertIn('horizontal, vertical, keep_sign',text)

    def test_progress_trap_and_full_selected_masks(self):
        cases=capture.cases()
        trap=next(c for c in cases if c['phase']==3)
        mask=check.generated_mask(trap,8)
        self.assertEqual(sum(v==255 for v in mask),15)
        full=next(c for c in cases if c['phase']==2)
        self.assertEqual(full['selection'],10)
        mask=check.generated_mask(full,10)
        self.assertEqual(sum(v==255 for v in mask),full['width']*full['height'])

    def synthetic_writer(self):
        case={'id':'SYNTHETIC-UNIT-CONTROL','horizontal':-2147483648,'vertical':2147483647,'keep_sign':-1}
        raw=struct.pack('>I',len(check.LITERAL))+check.LITERAL
        payloads=[struct.pack('>i',1),b'',b'']+[struct.pack('>i',case[k]) for k in ('horizontal','vertical','keep_sign')]+[b'']
        for (tag,size),payload in zip(check.WRITER_FIELDS,payloads):
            raw+=struct.pack('>II',tag,size)+payload
        self.assertEqual(len(raw),90)
        data=b'SYNTHETIC-UNIT-ONLY'+struct.pack('>II',32,len(raw))+raw
        files={case['id']+'.xcf':data,case['id']+'.definition.raw':raw}
        return case,files

    def test_synthetic_writer_parser_preserves_signed_flags(self):
        case,files=self.synthetic_writer()
        result=check.inspect_writer(files,case)
        self.assertEqual(result['payload_size'],90)
        self.assertEqual([r['payload_hex'] for r in result['records'][3:6]],['80000000','7fffffff','ffffffff'])

    def test_synthetic_writer_parser_rejects_malformed(self):
        case,files=self.synthetic_writer();name=case['id']+'.xcf';data=files[name]
        for bad in (data[:-1],data+data,data.replace(check.LITERAL,b'plug-in-edge!\0')):
            with self.subTest(length=len(bad)),self.assertRaises((ValueError,struct.error)):
                check.inspect_writer(dict(files,**{name:bad}),case)
        wrong=dict(case,horizontal=0)
        with self.assertRaises(ValueError): check.inspect_writer(files,wrong)

    def test_synthetic_signed_round_and_alpha_controls(self):
        check.check_shadow(bytes([0,0,1,255,2,1]),bytes([127,255])*3,3,1,2,(0,0,3,1),(0,1,1))
        with self.assertRaises(ValueError):
            check.check_shadow(bytes([0,0,1,255,2,1]),bytes([126,255])*3,3,1,2,(0,0,3,1),(0,1,1))
        check.check_shadow(bytes([0,0,5,255,10,1]),bytes([5,0,10,0,5,0]),3,1,2,(0,0,3,1),(0,1,0))
        check.check_shadow(bytes([0,0,5,255,11,1]),bytes([5,0,11,255,6,0]),3,1,2,(0,0,3,1),(0,1,0))

    def test_synthetic_hidden_color_and_byte_wrap_merge(self):
        self.assertEqual(check.check_merge(bytes([7,12,3,100]),bytes(4),bytes([255]),bytes([7,12,3,0]),4,15),3)
        check.check_merge(bytes([157,0]),bytes([254,127]),bytes([3]),bytes([46,0]),2,1)
        with self.assertRaises(ValueError):
            check.check_merge(bytes([7,12,3,100]),bytes(4),bytes([255]),bytes(4),4,15)

    def test_existing_304_context_records_only(self):
        archive=ROOT/'migration/fixtures/legacy-filter-context.tar.gz'
        with tarfile.open(archive) as tar:
            files={Path(m.name).name:tar.extractfile(m).read() for m in tar if m.isfile()}
        count=hidden=0
        for line in files['merges.tsv'].decode().splitlines():
            ident,w,h,c,active,empty=line.split('\t')
            def raw(label): return files[ident+'-'+label+'.raw']
            hidden+=check.check_merge(raw('input'),raw('shadow'),raw('mask'),raw('output'),int(c),int(active))
            count+=1
        self.assertEqual((count,hidden),(304,36939))

    def test_ordinary_diagnostics_retained_unexpected_rejected(self):
        known=b"(process:1): GLib-GObject-CRITICAL **: 00:00:00: g_value_set_object: assertion 'G_VALUE_HOLDS_OBJECT (value)' failed\nFailed to parse tag cache: No such file or directory\n"
        self.assertEqual(len(check.check_runtime_diagnostics(known)),2)
        with self.assertRaises(ValueError):check.check_runtime_diagnostics(known+b'ERROR: a new failure\n')

    def test_unsealed_corpus_cannot_activate_acceptance(self):
        if not check.BASE_ARCHIVE_SHA256:
            with self.assertRaisesRegex(ValueError,'not yet been sealed'):check.verify()
        else:
            self.assertEqual(len(check.BASE_ARCHIVE_SHA256),64)

    def test_new_python_sources_parse(self):
        for p in (ROOT/'tools').glob('*sobel*.py'):
            ast.parse(p.read_text(),str(p))


if __name__=='__main__':unittest.main(verbosity=2)

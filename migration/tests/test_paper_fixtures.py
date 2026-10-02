#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip,hashlib,json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
FIXTURES={'legacy-paper':529,'legacy-paper-modes':1537,'legacy-paper-transform':2593,'legacy-paper-smudge':193,'legacy-paper-fill':37}
class PaperFixtures(unittest.TestCase):
    def test_hashes_and_independent_recapture(self):
        for name,count in FIXTURES.items():
            with self.subTest(name=name):
                folder=ROOT/'migration/fixtures'/name;report=json.loads((folder/'runtime.json').read_text());raw=gzip.decompress((folder/'pixels.tsv.gz').read_bytes())
                self.assertEqual(len(raw.splitlines()),count)
                self.assertEqual(hashlib.sha256(raw).hexdigest(),report['independent_repeat']['normalized_sha256'])
                self.assertEqual(len(raw),report['independent_repeat']['normalized_bytes'])
                for path,digest in report['fixture_sha256'].items():self.assertEqual(hashlib.sha256((folder/path).read_bytes()).hexdigest(),digest,path)
                self.assertEqual(hashlib.sha256((ROOT/report['harness_source']).read_bytes()).hexdigest(),report['harness_sha256'])
    def test_full_undo_redo(self):
        for name in ['legacy-paper','legacy-paper-modes','legacy-paper-smudge','legacy-paper-fill']:
            phases={}
            for line in gzip.decompress((ROOT/'migration/fixtures'/name/'pixels.tsv.gz').read_bytes()).splitlines():
                f=line.split()
                if f[0] not in [b'PAPER_STROKE',b'SMUDGE',b'BRUSH']:continue
                phases.setdefault(int(f[1]),{})[f[2]]=bytes.fromhex(f[-1].decode())
            for case,p in phases.items():
                with self.subTest(name=name,case=case):
                    self.assertEqual(p[b'finish'],p[b'redo'])
                    if b'initial' in p:self.assertEqual(p[b'initial'],p[b'undo'])
    def test_unsafe_old_opaque_dst_not_executed(self):
        ids=set()
        for line in gzip.decompress((ROOT/'migration/fixtures/legacy-paper-modes/pixels.tsv.gz').read_bytes()).splitlines():
            f=line.split()
            if f[0]==b'PAPER_STROKE':ids.add(int(f[1]))
        self.assertEqual(len(ids),384)
        for case in range(432):self.assertEqual(case in ids,not(case%24//6 in [0,2]and case//24%9 in [6,8]))
    def test_all_old_texture_inputs_present(self):
        for name,cases in [('legacy-paper',216),('legacy-paper-transform',1296)]:
            masks={}
            for line in gzip.decompress((ROOT/'migration/fixtures'/name/'pixels.tsv.gz').read_bytes()).splitlines():
                f=line.split()
                if f[0]==b'PAPER_MASK':
                    raw=bytes.fromhex(f[-1].decode());self.assertEqual(len(raw),int(f[4])*int(f[5]));masks[(int(f[1]),int(f[2]))]=raw
            self.assertEqual(len(masks),cases*2)
            for case in range(cases):self.assertEqual(len(masks[case,0]),len(masks[case,1]))
if __name__=='__main__':unittest.main()

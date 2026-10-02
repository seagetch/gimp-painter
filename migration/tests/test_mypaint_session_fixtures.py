"""Full-session provenance/data checks; native Meson test checks production code."""
import gzip,hashlib,json
from pathlib import Path
import unittest
FIX=Path(__file__).resolve().parents[1]/'fixtures/legacy-mypaint-session'
def records(path):return [x for x in gzip.decompress((FIX/path).read_bytes()).splitlines() if x.startswith(b'SESSION_')]
class SessionFixtures(unittest.TestCase):
    def test_sealed_files(self):
        m=json.loads((FIX/'manifest.json').read_text());self.assertEqual(m['source_commit'],'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(set(m['files']),{p.name for p in FIX.iterdir() if p.is_file() and p.name!='manifest.json'})
        for n,e in m['files'].items():
            with self.subTest(file=n):
                b=(FIX/n).read_bytes();self.assertEqual(len(b),e['bytes']);self.assertEqual(hashlib.sha256(b).hexdigest(),e['sha256'])
    def test_full_old_new_pixels_and_undo(self):
        report=json.loads((FIX/'comparison.json').read_text());a=records('old-runtime.log.gz');b=records('new-runtime.log.gz')
        self.assertEqual(a,b);self.assertEqual(a,records('session-values.tsv.gz'));self.assertEqual(len(a),129)
        data=b'\n'.join(a)+b'\n';self.assertEqual(len(data),9587537);self.assertEqual(hashlib.sha256(data).hexdigest(),report['old_sha256'])
        self.assertEqual(report['old_sha256'],report['new_sha256']);self.assertTrue(report['equal'])
        rows={}
        for line in a:
            fields=line.split()
            if fields[0]==b'SESSION_PIX':rows.setdefault(int(fields[1]),{})[fields[2]]=bytes.fromhex(fields[3].decode())
            elif fields[0]==b'SESSION_UNDO':self.assertEqual(fields[2:],[b'1']*5)
        initial=bytes(v for y in range(96) for x in range(130) for v in ((x*19+y*7)%256,(x*3+y*29)%256,(x*13+y*11)%256,0 if x<35 else (x*17+y*23)%256))
        self.assertEqual(len(rows),32)
        for row in rows.values():
            self.assertEqual(row[b'undo'],initial);self.assertEqual(row[b'finish'],row[b'redo']);self.assertNotEqual(row[b'finish'],initial)
    def test_cold_failure_is_separate(self):
        self.assertEqual((FIX/'cold.exit').read_text().strip(),'139');self.assertEqual((FIX/'cold-trace.exit').read_text().strip(),'139')
        trace=gzip.decompress((FIX/'cold-trace-runtime.stderr.gz').read_bytes())
        self.assertIn(b'gimp_item_get_height',trace);self.assertIn(b'begin_session',trace);self.assertIn(b'stroke_to',trace)
        self.assertNotIn(b'SESSION_CAPTURE_COMPLETE',gzip.decompress((FIX/'cold-runtime.log.gz').read_bytes()))
    def test_color_stimulus_failure_kept(self):
        old={l.split()[1]:bytes.fromhex(l.split()[3].decode()) for l in records('session-values.tsv.gz') if l.startswith(b'SESSION_PIX') and l.split()[2]==b'finish'}
        failed={l.split()[1]:bytes.fromhex(l.split()[3].decode()) for l in records('failed-color-stimulus.log.gz') if l.startswith(b'SESSION_PIX') and l.split()[2]==b'finish'}
        self.assertEqual(old.keys(),failed.keys())
        for n,v in old.items():self.assertEqual(v[3::4],failed[n][3::4]);self.assertNotEqual(v,failed[n])
if __name__=='__main__':unittest.main()

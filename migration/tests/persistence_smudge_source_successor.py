"""Exact committed Smudge source admission; saved property schema unchanged."""
import hashlib,io,json,pathlib,tarfile
PACKAGE='migration/tests/persistence-smudge-source-successor'
PATH='app/paint/gimppaintersmudge.cpp'
PINS={'sources.tar.gz': 'bf66b47e4abff11ad33b3194666dc41357f1bd5a7b3d649b7d969f9c83e86474', 'transition.json': 'c0241db3d0c4866e01d81feed21b6b06f6357f0a7be21cb2fa6535cf6a5fb467'}
def sha(data):return hashlib.sha256(data).hexdigest()
def exact_change(before,after):
    start=b'void motion (GimpPaintCore* core,';end=b'void interpolate (GimpPaintCore* core,'
    assert before.count(start)==after.count(start)==before.count(end)==after.count(end)==1
    a=before.index(start);b=after.index(start);ae=before.index(end,a);be=after.index(end,b)
    assert before[:a]==after[:b] and before[ae:]==after[be:], 'saved settings/property or unrelated source bytes changed'
    assert (a,len(before)-ae)==(17439,18425), 'motion boundary changed'
    for text in [b'struct Settings { double rate = 50; bool blending = false;',b'void options_set (',b'void options_get (',b'static void gimp_painter_smudge_options_class_init (']:
        assert text in before[:a]+before[ae:]
def reviewed(root):
    root=pathlib.Path(root);data={n:(root/PACKAGE/n).read_bytes() for n in PINS}
    for n,h in PINS.items():assert sha(data[n])==h,('Smudge proof changed',n)
    record=json.loads(data['transition.json']);assert record['revision']=='native' and record['path']==PATH
    with tarfile.open(fileobj=io.BytesIO(data['sources.tar.gz'])) as t:
        assert t.getnames()==['predecessor.cpp','successor.cpp'] and all(x.isfile() for x in t.getmembers())
        before=t.extractfile('predecessor.cpp').read();after=t.extractfile('successor.cpp').read()
    assert sha(before)==record['predecessor_sha256'] and sha(after)==record['successor_sha256']
    exact_change(before,after)
    assert (root/PATH).read_bytes()==after, 'unreviewed committed Smudge source drift'
    return record
def resolve_source(root,predecessor,actual):
    record=reviewed(root)
    assert predecessor==record['predecessor_sha256'], 'wrong Smudge predecessor'
    assert actual==record['successor_sha256'], 'wrong Smudge successor'
    return record.copy()

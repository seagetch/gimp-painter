"""Exact unsafe-Undo signal admission; no saved field or class-vfunc change."""
import hashlib,io,json,pathlib,tarfile
PACKAGE='migration/tests/persistence-unsafe-undo-successor'
PATH='app/core/gimpimage.c'
PINS={'sources.tar.gz': 'dd2bd122c805b3872ae6f9db153dc5eb1db72a3037402d97488e504056286291', 'transition.json': 'd94c34da6dcf576abaa047396c2df3cc01d0557d6905103224f4d85a04092c7e'}
def sha(b):return hashlib.sha256(b).hexdigest()
def exact_change(before,after):
    enum=b'  QUERY_UNSAFE_UNDO,\n'
    signal=b'  /* Unlike pending paint, only transactions which cannot be canceled by\n   * ordinary Undo notifications veto history movement here. */\n  gimp_image_signals[QUERY_UNSAFE_UNDO] =\n    g_signal_new ("query-unsafe-undo",\n                  G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0,\n                  g_signal_accumulator_true_handled, NULL,\n                  NULL, G_TYPE_BOOLEAN, 0);\n\n'
    getter=b'gboolean\ngimp_image_has_unsafe_undo (GimpImage *image)\n{\n  gboolean unsafe = FALSE;\n\n  g_return_val_if_fail (GIMP_IS_IMAGE (image), FALSE);\n  g_object_ref (image);\n  g_signal_emit (image, gimp_image_signals[QUERY_UNSAFE_UNDO], 0, &unsafe);\n  g_object_unref (image);\n  return unsafe;\n}\n\n'
    for addition in [enum,signal,getter]:
        assert after.count(addition)==1 and addition not in before, 'unknown unsafe-Undo source transformation'
        after=after.replace(addition,b'')
    assert after==before, 'property/serializer/unrelated source drift'
def reviewed(root):
    root=pathlib.Path(root);data={n:(root/PACKAGE/n).read_bytes()for n in PINS}
    for n,h in PINS.items():assert sha(data[n])==h,('unsafe-Undo proof changed',n)
    r=json.loads(data['transition.json']);assert r['revision']=='native' and r['path']==PATH
    names=[side+'/'+p for p in [PATH,r['header_path']]for side in ['predecessor','successor']]
    with tarfile.open(fileobj=io.BytesIO(data['sources.tar.gz']))as t:
        assert t.getnames()==names and all(m.isfile()for m in t.getmembers());blobs={n:t.extractfile(n).read()for n in names}
    before=blobs['predecessor/'+PATH];after=blobs['successor/'+PATH]
    assert sha(before)==r['predecessor_sha256'] and sha(after)==r['successor_sha256'];exact_change(before,after)
    oldh=blobs['predecessor/'+r['header_path']];newh=blobs['successor/'+r['header_path']]
    declaration=b'/* Read-only veto for non-cancelable paint/history transitions, not Save. */\ngboolean        gimp_image_has_unsafe_undo        (GimpImage          *image);\n'
    assert newh.count(declaration)==1 and newh.replace(declaration,b'')==oldh, 'class layout/vfunc/property drift'
    assert sha(oldh)==r['header_predecessor_sha256'] and sha(newh)==r['header_successor_sha256']
    assert (root/PATH).read_bytes()==after and (root/r['header_path']).read_bytes()==newh, 'unreviewed unsafe-Undo current source drift'
    assert sha((root/r['runtime_package']/'artifact-sha256.json').read_bytes())==r['runtime_manifest_sha256'], 'runtime predecessor changed'
    return r
def resolve_source(root,predecessor,actual):
    r=reviewed(root);assert predecessor==r['predecessor_sha256'] and actual==r['successor_sha256'], 'wrong unsafe-Undo source endpoints';return r.copy()

#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,re
P=pathlib.Path(__file__).resolve().parent
PINNED_OUTSIDE = {'app/core/gimpimage-duplicate.c': '183de98fa06c19feb54f5f6b9397a031e541d02e', 'app/core/gimpimage.c': '886102871dbc7a24e53b921b56a9988a04ab4c52', 'app/core/gimpimage-private.h': 'e86752331c8c03c516b3289309412217d7b0dc29', 'app/core/gimpimage-perspective-guide.c': '12e7e1705482314b381858cd16f6f4ec42e42545', 'app/core/gimpimage-perspective-guide.h': '20ebacb27dbf00b9b3a822e2367f9433433a9199', 'app/tools/gimpperspectiveguidetool.cpp': 'f78df23259748af0217c7fd26a3924bcbf0d9d9b'}
PINNED_XCF_TREE = '129ac77f8d4b94d0190391c5cb1358d5067fbacc'
def check(p=P):
 j=json.loads((p/'source.json').read_text());assert j['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 with tarfile.open(p/'old-persistence-sources.tar.gz','r:gz') as t:
  assert len(t.getnames())==21
  source={n:t.extractfile(n).read() for n in t.getnames()}
 assert set(source)=={r['path'] for r in j['rows']}
 tree=b''
 for r in sorted(j['rows'],key=lambda r:r['path']):
  b=source[r['path']];blob=hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
  assert blob==r['blob'] and hashlib.sha256(b).hexdigest()==r['sha256']
  mentions=[i+1 for i,l in enumerate(b.decode().splitlines()) if re.search(r'perspective.*guide|guide.*perspective',l,re.I)]
  assert mentions==r['perspective_guide_mentions']
  if r['path'].startswith('app/xcf/'):
   assert not mentions;tree+=b'100644 '+r['path'].split('/')[-1].encode()+b'\0'+bytes.fromhex(blob)
  else:assert blob==PINNED_OUTSIDE[r['path']]
  if r['path']=='app/core/gimpimage-duplicate.c':assert not mentions
 assert hashlib.sha1(b'tree '+str(len(tree)).encode()+b'\0'+tree).hexdigest()==PINNED_XCF_TREE
 print('PASS: pinned complete old XCF tree and duplicate omit guide;21 source blobs verified, source-only boundary')
if __name__=='__main__':check()

#!/usr/bin/env python3
"""Independent float oracle after explicit Babl storage roundtrip, not old captures."""
import ctypes,hashlib,importlib.util,json,pathlib,random,struct
ROOT=pathlib.Path(__file__).resolve().parents[3]
p=ROOT/'migration/tests/painter-float-kernel/run.py';spec=importlib.util.spec_from_file_location('o',p);o=importlib.util.module_from_spec(spec);spec.loader.exec_module(o)
babl=ctypes.CDLL('libbabl-0.1.so.0');babl.babl_init()
babl.babl_format.argtypes=[ctypes.c_char_p];babl.babl_format.restype=ctypes.c_void_p
babl.babl_fish.argtypes=[ctypes.c_void_p,ctypes.c_void_p];babl.babl_fish.restype=ctypes.c_void_p
babl.babl_process.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_long];babl.babl_process.restype=ctypes.c_long
babl.babl_format_get_bytes_per_pixel.argtypes=[ctypes.c_void_p];babl.babl_format_get_bytes_per_pixel.restype=ctypes.c_int
formats=["R'G'B'A "+x for x in ['u8','u16','u32','half','float','double']];ff=babl.babl_format(b"R'G'B'A float")
def convert(v,fmt):
 n=len(v)//4;src=(ctypes.c_float*len(v))(*v);dst=ctypes.create_string_buffer(n*babl.babl_format_get_bytes_per_pixel(fmt));out=(ctypes.c_float*len(v))()
 assert babl.babl_process(babl.babl_fish(ff,fmt),src,dst,n)==n
 assert babl.babl_process(babl.babl_fish(fmt,ff),dst,out,n)==n
 return list(out)
r=random.Random(1301203);dest=ROOT/'migration/fixtures/painter-float/storage-vectors.bin'
with dest.open('wb') as file:
 for fi,name in enumerate(formats):
  fmt=babl.babl_format(name.encode())
  for mode in range(9):
   for opacity in [0,.37,1]:
    a=[];b=[];mask=[];expected=[];opacity=o.f32(opacity)
    for n in range(16):
     a+=list(map(o.f32,[*[r.uniform(-2,3) for _ in range(3)],r.choice([0.,1.,r.random()])]))
     b+=list(map(o.f32,[*[r.uniform(-2,3) for _ in range(3)],r.choice([0.,1.,r.random()])]))
     mask.append(o.f32(r.choice([0.,1.,r.random()])))
    ac=convert(a,fmt);bc=convert(b,fmt)
    for n in range(16):expected+=o.reference(ac[4*n:4*n+4],bc[4*n:4*n+4],opacity,mask[n],mode)
    file.write(struct.pack('<iif208f',fi,mode,opacity,*a,*b,*mask,*expected))
dest.with_name('storage-manifest.json').write_text(json.dumps({'scope':'six Babl input storage formats into explicit float32 arithmetic; no lossless double or native-image storage claim','formats':formats,'seed':1301203,'records':162,'pixels':16,'format':'little-endian int format index, int private mode, float opacity; backdrop64/source64/mask16/expected64 floats','oracle_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'generator_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()},indent=2)+'\n')

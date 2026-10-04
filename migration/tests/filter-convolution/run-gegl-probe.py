#!/usr/bin/env python3
"""Measure pinned GEGL against real old PDB shadow bytes; no replacement oracle."""
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
FIXTURES = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else None
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
lib = C.CDLL(str(HERE / 'gegl-probe.so'))
lib.probe_convolve.argtypes = [C.c_int]*4 + [C.POINTER(C.c_double), C.c_double,
    C.c_double, C.POINTER(C.c_int), C.c_void_p, C.c_void_p]
assert lib.probe_init() == 1, 'GEGL convolution-matrix not available'
version = [C.c_int() for _ in range(3)]
lib.probe_version(*(C.byref(x) for x in version))
assert [x.value for x in version] == [0, 4, 62], [x.value for x in version]

def convolve(w, h, mode, border, matrix, divisor, offset, channels, data):
    source = (C.c_ubyte * len(data)).from_buffer_copy(data)
    result = (C.c_ubyte * len(data))()
    assert lib.probe_convolve(w,h,mode,border,(C.c_double*25)(*matrix),divisor,
        offset,(C.c_int*4)(*channels),source,result) == 1
    return bytes(result)

identity = [0.0]*25
identity[12] = 1.0
double_results = []
for mode, label in ((1,'linear'),(2,'nonlinear')):
    samples = []
    for p in range(9):
        samples.extend([0.25 + 2**-35 + p*2**-40,
                        0.5 + 2**-34 + p*2**-41,
                        0.75 + 2**-36 + p*2**-42, 1.0])
    packed = bytes((C.c_double*len(samples))(*samples))
    actual_raw = convolve(3,3,mode,0,identity,1,0,[1]*4,packed)
    actual = list((C.c_double*len(samples)).from_buffer_copy(actual_raw))
    deltas = [abs(a-b) for a,b in zip(samples,actual)]
    double_results.append(dict(trc=label,sample_count=len(samples),
        mismatch_count=sum(d != 0 for d in deltas),max_abs_delta=max(deltas),
        input=samples,output=actual,input_sha256=hashlib.sha256(packed).hexdigest(),
        output_sha256=hashlib.sha256(actual_raw).hexdigest()))

results=[]
if FIXTURES:
    doc=json.loads(FIXTURES.read_text())
    pixelpath=FIXTURES.parent/doc['pixel_file']
    pixels=pixelpath.read_bytes()
    assert hashlib.sha256(pixels).hexdigest()==doc['pixel_sha256']
    def get(case,label):
        entry=case['buffers'][label]
        out=pixels[entry['offset']:entry['offset']+entry['length']]
        assert hashlib.sha256(out).hexdigest()==entry['sha256']
        return out
    for case in doc['cases']:
        if case['live'] or case['selection'] or case['status'] != 3:
            continue
        if (case['width'],case['height']) != (9,8):
            continue
        w,h,n=case['width'],case['height'],case['components']
        source=get(case,'merge1-input')
        expected=get(case,'merge1-shadow')
        carrier=bytearray()
        for p in range(w*h):
            px=source[p*n:(p+1)*n]
            carrier.extend([px[0]]*3 if n<3 else px[:3])
            carrier.append(px[-1] if n in (2,4) else 255)
        red,green,blue=(case['channels'][1:4] if n>=3 else [case['channels'][0]]*3)
        channels=[red,green,blue,case['channels'][4] if n in (2,4) else 0]
        # check_config forces EXTEND for drawables without alpha, and disables
        # alpha weighting on every old noninteractive call.
        border=case['border'] if n in (2,4) else 0
        actual=convolve(w,h,0,border,case['matrix'],case['divisor'],
                        case['offset']/255.0,channels,carrier)
        native=bytearray()
        for p in range(w*h):
            px=actual[p*4:p*4+4]
            native.extend(px[:1] if n<3 else px[:3])
            if n in (2,4): native.append(px[3])
        assert len(expected)==len(native)==w*h*n
        deltas=[abs(a-b) for a,b in zip(native,expected)]
        results.append(dict(id=case['id'],kernel=case['kernel'],components=n,
            requested_border=case['border'],effective_border=border,
            requested_alpha_weight=case['alpha_weight'],effective_alpha_weight=False,
            normalize=False,divisor=case['divisor'],offset=case['offset']/255.0,
            channels=channels,sample_count=len(deltas),
            mismatch_count=sum(d != 0 for d in deltas),max_abs_delta=max(deltas),
            expected_shadow_sha256=hashlib.sha256(expected).hexdigest(),
            actual_shadow_sha256=hashlib.sha256(native).hexdigest()))

runtime_paths=sorted({line.split()[-1] for line in Path('/proc/self/maps').read_text().splitlines()
    if '/.deps-debian13/' in line and any(tag in line for tag in ('/gegl-0.4/','libgegl','libbabl'))})
runtime_hashes={p:sha(Path(p)) for p in runtime_paths}
lib.probe_exit()
report=dict(schema_version=1,gegl_version=[x.value for x in version],
    purpose='Backend comparison only; expected mismatches are not GIMP3 implementation failures',
    format='R\'G\'B\'A u8 input/output; GEGL operation internally RGBA float linear',
    orientation='PDB matrix[x*5+y] -> GEGL letter a+x and digit 1+y',
    settings=dict(normalize=False,alpha_weight=False,offset='old offset / 255',
        border='0 CLAMP, 1 LOOP, 2 NONE; force CLAMP for no-alpha old input',
        gray='triplicated carrier; compare first component without luminance conversion'),
    source_sha256=sha(HERE/'gegl-probe.c'),driver_sha256=sha(Path(__file__)),
    binary_sha256=sha(HERE/'gegl-probe.so'),runtime_sha256=runtime_hashes,
    double_identity=double_results,
    cases=results,case_count=len(results),
    mismatching_cases=sum(r['mismatch_count']>0 for r in results),
    total_mismatching_samples=sum(r['mismatch_count'] for r in results),
    maximum_byte_delta=max([r['max_abs_delta'] for r in results] or [0]))
if FIXTURES:
    report['oracle_fixture_sha256']=sha(FIXTURES)
    report['oracle_pixels_sha256']=sha(pixelpath)
(HERE/'gegl-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('cases','double_identity')},indent=2))
for row in double_results:
    print('double_identity',row['trc'],row['mismatch_count'],row['max_abs_delta'])
for kind in sorted(set(r['kernel'] for r in results)):
    rows=[r for r in results if r['kernel']==kind]
    print(kind,len(rows),sum(r['mismatch_count']>0 for r in rows),
          sum(r['mismatch_count'] for r in rows),max(r['max_abs_delta'] for r in rows))

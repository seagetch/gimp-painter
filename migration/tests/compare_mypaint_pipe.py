#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare exact pipe pixels/state/current axes; report unused previous axes.

Pinned and current native GimpBrushPipe selectors never read last_coords. We
retain all observed old coordinates, but do not call their reset a pixel defect.
The only omitted fields are the valid-last flag and seven last-coordinate axes
in PIPE_SELECT records; every other byte/record must match exactly.
"""
import argparse,collections,gzip,hashlib,json,pathlib,subprocess,sys

def records(data):return [line for line in data.splitlines() if line.startswith(b'PIPE_')]
def comparable(line):
    p=line.split()
    if p[0]!=b'PIPE_SELECT':return line
    if len(p) not in (14,21):raise ValueError('Malformed selector record')
    if p[6] not in (b'0',b'1') or len(p)!=(21 if p[6]==b'1' else 14):raise ValueError('Invalid coordinate record')
    return b' '.join(p[:6]+p[-7:])
def compare(expected,actual):
    old,new=records(expected),records(actual)
    a,b=list(map(comparable,old)),list(map(comparable,new))
    different=[i for i,(x,y) in enumerate(zip(a,b)) if x!=y]
    if len(a)!=len(b):different.extend(range(min(len(a),len(b)),max(len(a),len(b))))
    return {'equal':a==b,'expected_records':len(old),'actual_records':len(new),
      'actual_record_types':dict(collections.Counter(x.split()[0].decode() for x in new)),
      'compared_sha256':hashlib.sha256(b'\n'.join(b)+b'\n').hexdigest(),
      'raw_sha256':hashlib.sha256(b'\n'.join(new)+b'\n').hexdigest(),
      'unused_last_coordinate_differences':sum(x!=y and comparable(x)==comparable(y) for x,y in zip(old,new)),
      'mismatches':[{'record':i+1,'expected':old[i][:180].decode() if i<len(old) else '<missing>','actual':new[i][:180].decode() if i<len(new) else '<missing>'} for i in different[:20]],
      'different_records':len(different)}
def main():
    p=argparse.ArgumentParser();p.add_argument('--report',type=pathlib.Path);p.add_argument('fixture',type=pathlib.Path);p.add_argument('command',nargs=argparse.REMAINDER);a=p.parse_args()
    run=subprocess.run(a.command,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    report=compare(gzip.decompress(a.fixture.read_bytes()),run.stdout);report['exit_code']=run.returncode
    report['stderr']=run.stderr.decode(errors='replace')
    if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='stderr'},indent=2))
    if run.returncode or not report['equal']:sys.stderr.buffer.write(run.stderr);raise SystemExit(run.returncode or 1)
if __name__=='__main__':main()

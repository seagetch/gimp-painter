from pathlib import Path
import hashlib,json,os,subprocess,time

R=Path('/workspace/scratch/5b5281e79681/gimp-painter-port-active')
N=R.parent/'gimp-native-restore-20261008'
D=Path(__file__).resolve().parent
header=R/'app/core/core-enums.h'
original=header.read_bytes()
sha=lambda b:hashlib.sha256(b).hexdigest()
targets=['app/core/libappcore.a.p/gimpimage.c.o','app/core/libappcore.a.p/gimpclonelayer.cpp.o']
rows=[]
def run(variant,phase):
    envfile=N/('env-main.sh' if variant=='default' else 'env-http.sh')
    command=['bash','-c','source "$1"; exec ninja -f "$2" -j2 -v "$3" "$4"','native-enum-rebuild',str(envfile),str(N/(variant+'.ninja')),*targets]
    log=D/(variant+'-'+phase+'.log')
    with log.open('wb') as output:
        result=subprocess.run(command,cwd=N/('build-'+variant),stdout=output,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(str(log))
    return {'command':command,'exit_code':result.returncode,'log':log.name,'log_sha256':sha(log.read_bytes())}
def times(variant):
    build=N/('build-'+variant)
    return {p:(build/p).stat().st_mtime_ns for p in targets+['app/core/stamp-core-enums.h']}
try:
    for variant in ['default','http']:
        baseline=run(variant,'baseline');before=times(variant)
        changed=original+b'\n/* Incremental enum source dependency verification. */\n'
        header.write_bytes(changed)
        shadow=N/('generator-source-'+variant)/'app/core/core-enums.h'
        shadow_original=shadow.read_bytes()
        try:
            shadow.write_bytes(changed)
            rebuild=run(variant,'header-edit');after=times(variant)
            assert all(after[p]>before[p] for p in before),(variant,before,after)
            noop=run(variant,'no-op');unchanged=times(variant)
            assert after==unchanged
        finally:
            header.write_bytes(original);shadow.write_bytes(shadow_original)
        restored=run(variant,'restore')
        rows.append({'variant':variant,'baseline':baseline,'rebuild':rebuild,'no_op':noop,'restore':restored,'before':before,'after':after,'both_c_and_cpp_rebuilt':True,'source_generation_stamp_rebuilt':True,'noop_rebuilt_objects':False})
finally:
    header.write_bytes(original)
assert header.read_bytes()==original
report={'status':'PASS','scope':'Real parent Meson/Ninja C and C++ core objects after enum-header comment mutation; unchanged generated enum values; protected generators use identical isolated input bytes and preserve tracked outputs','header':'app/core/core-enums.h','source_sha256':sha(original),'mutated_sha256':sha(changed),'variants':rows,'source_restored':True}
(D/'native-header-rebuild.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'status':'PASS','variants':len(rows),'real_objects_rebuilt':len(rows)*len(targets),'source_restored':True}))

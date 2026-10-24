import pathlib,subprocess,shlex,hashlib,json,sys
B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');out=pathlib.Path(sys.argv[1]);paths=set();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for target in ['preset-catalog','preset-catalog-ui','preset-gui-entries']:
 cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=B,text=True).splitlines()[-1])
 for x in cmd:
  p=(B/x).resolve()
  if p.is_file() and (x.endswith('.o') or x.endswith('.a') or '.so' in p.name):
   paths.add(p)
   if x.endswith('.a') and p.read_bytes()[:8]==b'!<thin>\n':
    for member in subprocess.check_output(['ar','t',str(p)],cwd=B,text=True).splitlines():
     q=(B/member).resolve();assert q.is_file(),q;paths.add(q)
json.dump({str(p):sha(p) for p in sorted(paths)},open(out,'w'),indent=2)
print(len(paths),'actual native link input files sealed')

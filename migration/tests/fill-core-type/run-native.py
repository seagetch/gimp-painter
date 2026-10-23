from pathlib import Path
import subprocess,os,json,hashlib,re
root=Path(__file__).resolve().parents[3]
build=Path(os.environ['GIMP_TESTING_ABS_TOP_BUILDDIR'])
result=subprocess.run([str(build/'app/tests/gimp-fill-brush')],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
print(result.stdout,end='')
assert result.returncode==0 and len(re.findall(r'^ok \d+ ',result.stdout,re.M))==40

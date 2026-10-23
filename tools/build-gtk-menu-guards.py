#!/usr/bin/env python3
"""Build the explicitly selected, source-locked GTK guard dependency privately.

Requires the project's pinned build environment plus the extra build packages
listed in migration/patches/gtk-3.24.49-menu-guards/README.md. Never installs into
system directories or modifies the existing dependency root. Publication is not
performed. A new empty work directory is required; failed runs are preserved.
"""
import argparse, hashlib, json, pathlib, subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
PATCH=ROOT/'migration/patches/gtk-3.24.49-menu-guards'
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*a,**kw): subprocess.run([str(x) for x in a],check=True,**kw)
def main():
 p=argparse.ArgumentParser();p.add_argument('--archives',type=pathlib.Path,required=True);p.add_argument('--work',type=pathlib.Path,required=True);p.add_argument('--prepare-only',action='store_true');p.add_argument('-j',type=int,default=2);a=p.parse_args()
 lock=json.loads((PATCH/'source-lock.json').read_text());patch=PATCH/'0001-safe-menu-grab-accessible-ancestors.patch'
 if digest(patch)!=lock['patch_sha256']:raise SystemExit('Patch hash mismatch')
 for name,h in lock['source_archives'].items():
  if digest(a.archives/name)!=h:raise SystemExit('Source archive hash mismatch: '+name)
 work=a.work.resolve()
 if work.exists():raise SystemExit('Work directory must not already exist (preserve prior evidence)')
 work.mkdir(parents=True);source=work/'source';build=work/'build';prefix=work/'prefix'
 run('dpkg-source','--no-check','-x',a.archives.resolve()/'gtk+3.0_3.24.49-3.dsc',source)
 for name,h in lock['patch_inputs'].items():
  if digest(source/name)!=h['before_sha256']:raise SystemExit('Patch input mismatch: '+name)
 run('patch','--batch','--fuzz=0','-p1','-i',patch,cwd=source)
 for name,h in lock['patch_inputs'].items():
  if digest(source/name)!=h['after_sha256']:raise SystemExit('Patched source mismatch: '+name)
 (work/'verified-source-lock.json').write_text(json.dumps(lock,indent=2)+'\n')
 if a.prepare_only:return
 for theme in ['Adwaita','HighContrast']:
  run('sh','parse-sass.sh',cwd=source/'gtk/theme'/theme)
 run('meson','setup',build,source,'--prefix='+str(prefix),'--libdir=lib/x86_64-linux-gnu','--buildtype=debugoptimized','-Ddebug=false','-Db_ndebug=false','-Dx11_backend=true','-Dwayland_backend=true','-Dbroadway_backend=true','-Dcloudproviders=true','-Dprofiler=true','-Dprint_backends=cups,file','-Dcolord=yes','-Dintrospection=true','-Dgtk_doc=false','-Dman=false','-Ddemos=false','-Dexamples=false','-Dtests=false')
 run('meson','compile','-C',build,'-j'+str(a.j));run('meson','install','-C',build,'--no-rebuild')
 licenses=prefix/'share/doc/gtk-menu-guards';licenses.mkdir(parents=True)
 for path in ['COPYING','debian/copyright']:
  (licenses/path.replace('/','-')).write_bytes((source/path).read_bytes())
 (licenses/'source-lock.json').write_text(json.dumps(lock,indent=2)+'\n')
 manifest={str(f.relative_to(prefix)):digest(f) for f in sorted(prefix.rglob('*')) if f.is_file() and not f.is_symlink()}
 (work/'runtime-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Private GTK prefix:',prefix)
 print('Explicit opt-in only: prepend its lib/x86_64-linux-gnu to LD_LIBRARY_PATH.')
if __name__=='__main__':main()

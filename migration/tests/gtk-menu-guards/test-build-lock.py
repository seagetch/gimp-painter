#!/usr/bin/env python3
"""Small pre-build rejection probes; no dependency files are changed."""
import importlib.util,json,pathlib,shutil,sys,tempfile
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
s=importlib.util.spec_from_file_location('builder',R/'tools/build-gtk-menu-guards.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
a=pathlib.Path(sys.argv[1]).resolve();results=[]
for case in ['changed-source-archive','changed-patch','existing-work']:
 with tempfile.TemporaryDirectory(prefix='gtk-source-lock-') as d:
  p=pathlib.Path(d);archives=p/'archives';archives.mkdir();patch=p/'patch';shutil.copytree(R/'migration/patches/gtk-3.24.49-menu-guards',patch)
  for f in ['gtk+3.0_3.24.49-3.dsc','gtk+3.0_3.24.49.orig.tar.xz','gtk+3.0_3.24.49-3.debian.tar.xz']:(archives/f).symlink_to(a/f)
  if case=='changed-source-archive':f=archives/'gtk+3.0_3.24.49-3.dsc';f.unlink();f.write_text('not the pinned source')
  if case=='changed-patch':(patch/'0001-safe-menu-grab-accessible-ancestors.patch').write_text('not the pinned patch')
  work=p/'work'
  if case=='existing-work':work.mkdir();(work/'sentinel').write_text('preserve')
  m.PATCH=patch;sys.argv=['build','--archives',str(archives),'--work',str(work),'--prepare-only']
  try:m.main()
  except SystemExit as e:
   assert str(e) in ['Source archive hash mismatch: gtk+3.0_3.24.49-3.dsc','Patch hash mismatch','Work directory must not already exist (preserve prior evidence)'];results.append({'case':case,'rejected':str(e)})
  else:raise AssertionError(case+' incorrectly admitted')
  if case=='existing-work':assert (work/'sentinel').read_text()=='preserve'
  else:assert not work.exists()
(P/'build-lock-controls.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS3 build-input rejection controls; no mutation of existing work')

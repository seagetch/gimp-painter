#!/usr/bin/env python3
"""Capture legacy dependency checks, link sites and current local license evidence."""
import csv,hashlib,re,subprocess
from pathlib import Path
from audit_cpp_object_data_sites import split_args
ROOT=Path('migration/inventory')
REV='afa43fae'
def main():
 source=subprocess.check_output(['git','show',f'{REV}:configure.ac'],text=True)
 versions=dict(re.findall(r'm[45]_define\(\[([^]]+_required_version)\],\s*\[([^]]+)\]',source))
 rows=[]
 for m in re.finditer(r'\b(PKG_CHECK_MODULES|AC_CHECK_LIB|AM_PATH_GLIB_2_0|AM_PATH_GTK_2_0|AM_PATH_GLIB|AM_PATH_GTK|AM_PATH_PYTHON|AC_PATH_PROG)\s*\(',source):
  op=m[1];args=split_args(source,m.end());
  if op=='AC_PATH_PROG' and args[0] not in ('FREETYPE_CONFIG','WMF_CONFIG'):continue
  line=source.count('\n',0,m.start())+1
  module=args[1] if op in ('PKG_CHECK_MODULES','AC_PATH_PROG') else args[0];api=args[1] if op=='AC_CHECK_LIB' and len(args)>1 else module
  resolved=module
  resolved=re.sub(r'\b\w+_required_version\b',lambda x:versions.get(x[0],x[0]),resolved)
  module=module.strip().strip('[]');name=module.split()[0]
  if op.startswith('AM_PATH'):name={'AM_PATH_GLIB_2_0':'glib','AM_PATH_GTK_2_0':'gtk','AM_PATH_PYTHON':'python'}.get(op,op)
  aliases={'gio-2.0':'libglib2.0','gmodule-no-export-2.0':'libglib2.0','glib':'libglib2.0','gtk':'libgtk-3','gdk-pixbuf-2.0':'libgdk-pixbuf-2.0','atk':'libatk1.0','cairo':'libcairo2','cairo-pdf':'libcairo2','pangocairo':'libpango-1.0','libpng':'libpng16','poppler-glib':'libpoppler-glib','libsoup-2.4':'libsoup-2.4','fontconfig':'libfontconfig1','libcurl':'libcurl4','tiff':'libtiff6','jpeg':'libjpeg-turbo8','librsvg-2.0':'librsvg2','Xmu':'libxmu6','Xpm':'libxpm4','lcms2':'liblcms2','gudev-1.0':'libgudev'}
  notice=next(iter(sorted(Path('/usr/share/doc').glob(aliases.get(name,'__not_installed__')+'*/copyright'))),None)
  if notice:
   text=notice.read_text(errors='replace');licenses=sorted(set(re.findall(r'^License:\s*(.+)',text,re.M)));license=';'.join(licenses) or ('LGPL-2.1 OR MPL-1.1 (file exceptions)' if name.startswith('cairo') else 'permissive X11-style notices (see source evidence)');evidence=str(notice);digest=hashlib.sha256(notice.read_bytes()).hexdigest();scope='current installed package notice; exact legacy archive not attested'
  elif name=='json-glib-1.0':license='LGPL-2.1-or-later';evidence='https://gnome.pages.gitlab.gnome.org/json-glib/';digest='-';scope='current upstream declaration; legacy archive not attested'
  elif name=='babl':license='LGPL-3.0-or-later';evidence='https://gegl.org/babl/';digest='-';scope='current upstream declaration; legacy archive not attested'
  else:license='NOT_IN_REPOSITORY';evidence='exact archive COPYING/LICENSE required in 34.005/license-manifest';digest='-';scope='explicit unresolved external notice; no license approval inferred'
  rows.append((f'configure.ac:{line}',op,name,resolved.strip(),api.strip(),'pkg-config LIBS / optional platform link; internal libapp*.a statically linked','Linux x86_64; Windows x86_64; macOS arm64/x86_64 subject to optional platform checks',license,evidence,digest,scope,'34.005/license-manifest'))
 for path,license in [('app/paint/mypaintbrush-brush.hpp','ISC notice'),('app/core/mypaintbrush-brushsettings.c','ISC notice'),('app/paint-funcs/mypaint-brushmodes.hpp','GPL-2.0-or-later')]:
  text=subprocess.check_output(['git','show',f'{REV}:{path}'],text=True)
  assert 'Permission to use' in text if license.startswith('ISC') else 'either version 2' in text
  rows.append((path+':1','BUNDLED_SOURCE','extended brushlib','vendored source; upstream release not declared','Brush/mapping/surface/stroke and GIMP mask/texture adapters','compiled into libapppaint.a / GIMP executable','all painter targets',license,f'{REV}:{path}','-','legacy per-file notice verified; no external libmypaint link declared','20.001,34.005/license-manifest'))
 with (ROOT/'legacy-dependencies.tsv').open('w') as f:
  w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(('legacy_site','check_kind','package_or_library','version_requirement','api_or_probe','link_method','distribution_target','license_declaration','license_evidence','notice_sha256','evidence_scope','followup'));w.writerows(rows)
 # Preserve all flatpak pin and build options lines, including disabled modules.
 pins=[]
 for path in ('build/flatpak/io.github.seagetch.GIMP-PAINTER-BaseApp.yml','build/flatpak/io.github.seagetch.GIMP-PAINTER.yml'):
  text=subprocess.check_output(['git','show',f'{REV}:{path}'],text=True)
  for i,line in enumerate(text.splitlines(),1):
   if re.search(r'\b(?:name:|url:|sha256:|branch:|runtime:|runtime-version:|sdk:|config-opts:)',line):pins.append((f'{path}:{i}','COMMENTED_DISABLED' if line.lstrip().startswith('#') else 'ACTIVE',line.strip(),'34.005/license-manifest'))
 with (ROOT/'legacy-flatpak-pins.tsv').open('w') as f:
  w=csv.writer(f,delimiter='\t',lineterminator='\n');w.writerow(('legacy_site','activation','manifest_line','followup'));w.writerows(pins)
 print(f'{len(rows)} dependency checks/bundled sources; {len(pins)} package pins/options (active and disabled distinguished)')
if __name__=='__main__':main()

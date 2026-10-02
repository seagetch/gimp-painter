# Legacy painter raster semantics on current GEGL buffers and owned resources.
painter_mypaint_surface = static_library('painter-mypaint-surface',
  'gegl-surface.cpp', 'gimp-resources.cpp', 'paint-core.cpp',
  'legacy-mask-transform.cpp', 'legacy-generated-mask.cpp',
  include_directories: [rootInclude, rootAppInclude],
  dependencies: [painter_mypaint_dep, gegl, gtk3],
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
painter_mypaint_surface_dep = declare_dependency(
  link_with: [painter_mypaint_surface, libgimpmath],
  include_directories: [include_directories('.'), rootAppInclude, rootInclude],
  dependencies: [painter_mypaint_dep, gegl, gtk3],
)
foreach part : ['surface', 'generated']
  trace = executable('painter-mypaint-' + part + '-trace',
    'tests/' + part + '-trace.cpp',
    dependencies: painter_mypaint_surface_dep,
    cpp_args: painter_cpp_args,
    override_options: ['cpp_std=c++14'],
  )
  if not meson.is_cross_build()
    args = [files('../painter-mypaint/tests/compare-trace.py'),
            meson.project_source_root() / 'migration/fixtures/legacy-mypaint-surface' / (part + '-values.tsv.gz'), trace]
    if part == 'surface'
      args += [meson.project_source_root() / 'migration/fixtures/legacy-mypaint-surface/shape-masks.tsv']
    endif
    test('painter-mypaint-legacy-' + part, painter_brush_settings_python,
      args: args, suite: 'painter', timeout: 60)
  endif
endforeach

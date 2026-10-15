# Legacy painter raster semantics on current GEGL buffers and owned resources.
painter_mypaint_surface = static_library('painter-mypaint-surface',
  'gegl-surface.cpp', 'gimp-resources.cpp', 'paint-core.cpp',
  '../gimppainterpaper.cpp',
  'gimp-painter-options.cpp', 'gimp-painter-session.cpp',
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

# Defined two-byte Gray equations, independent of GIMP transaction fixtures.
painter_gray_equations = executable('painter-gray-alpha-equations',
  'tests/gray-alpha-equations.cpp', include_directories: [rootAppInclude],
  dependencies: [glib], cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-gray-alpha-equations', painter_gray_equations,
    suite: 'painter', timeout: 30)
endif

# Native precision is a defined extension with independent scalar/ICC expectations.
painter_native_precision = executable('painter-mypaint-native-precision',
  'tests/native-precision.cpp', dependencies: [painter_mypaint_surface_dep, lcms],
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-native-precision', painter_native_precision,
    suite: 'painter', timeout: 60)
endif

# Half-open GEGL domains on representative legacy-byte and native-double paths.
painter_coordinate_boundaries = executable('painter-mypaint-coordinate-boundaries',
  'tests/coordinate-boundaries.cpp', dependencies: painter_mypaint_surface_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-coordinate-boundaries', painter_coordinate_boundaries,
    suite: 'painter', timeout: 60)
endif

# Explicit finalization evidence for exposed buffers and provider resources.
painter_region_lifetime = executable('painter-mypaint-region-lifetime',
  'tests/region-lifetime.cpp', dependencies: painter_mypaint_surface_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-region-lifetime', painter_region_lifetime,
    suite: 'painter', timeout: 60)
endif

# Actual old GimpDrawable selection coverage: interior signed offsets/soft bytes
# plus nonempty outside-target controls. Captured brush masks isolate Surface.
painter_selection_trace = executable('painter-mypaint-selection-trace',
  'tests/selection-trace.cpp', dependencies: painter_mypaint_surface_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-legacy-selection', painter_brush_settings_python,
    args: [files('tests/compare-selection.py'),
      meson.project_source_root() / 'migration/fixtures/legacy-mypaint-selection/selection-values.tsv.gz',
      painter_selection_trace],
    suite: 'painter', timeout: 60)
endif

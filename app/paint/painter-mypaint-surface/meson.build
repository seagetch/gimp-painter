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

# Stroke-start isolation, including target writes before first tile touch.
painter_snapshot_isolation = executable('painter-mypaint-snapshot-isolation',
  'tests/snapshot-isolation.cpp', dependencies: painter_mypaint_surface_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-snapshot-isolation', painter_snapshot_isolation,
    suite: 'painter', timeout: 60)
endif

# Bounded native-format reads, output stride/capacity, translated selection edges.
painter_buffer_read = executable('painter-mypaint-buffer-read',
  'tests/buffer-read.cpp', dependencies: painter_mypaint_surface_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-buffer-read', painter_buffer_read,
    suite: 'painter', timeout: 60)
endif

# GNU ELF observation seams expose snapshot/floating-buffer finalization only
# in this dedicated test, never in application linkage.
painter_region_internal_wrap = ['-Wl,--wrap=gegl_buffer_new', '-Wl,--wrap=gegl_buffer_dup']
if host_machine.system() == 'linux' and cxx.has_multi_link_arguments(painter_region_internal_wrap)
  painter_region_internal = executable('painter-mypaint-region-internals',
    'tests/region-lifetime.cpp', dependencies: painter_mypaint_surface_dep,
    cpp_args: painter_cpp_args + ['-DGIMP_TEST_REGION_INTERNALS'],
    link_args: painter_region_internal_wrap, override_options: ['cpp_std=c++14'])
  test('painter-mypaint-region-internals', painter_region_internal,
    env: ['GEGL_USE_OPENCL=no'], suite: 'painter', timeout: 60)
endif

# Bounded per-operation scratch, sparse floating storage, rejection and retry.
painter_work_buffer = executable('painter-mypaint-work-buffer',
  'tests/work-buffer.cpp', dependencies: painter_mypaint_surface_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-work-buffer', painter_work_buffer,
    env: ['GEGL_USE_OPENCL=no', 'GEGL_SWAP=RAM', 'GEGL_THREADS=1'],
    suite: 'painter', timeout: 60)
endif

# Exactly-once evaluator/native conversion and intentional byte-Gray policy.
painter_color_placement = executable('painter-mypaint-color-placement',
  'tests/color-placement.cpp', dependencies: [painter_mypaint_surface_dep, lcms],
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-color-placement', painter_color_placement,
    env: ['GEGL_USE_OPENCL=no', 'GEGL_SWAP=RAM'], suite: 'painter', timeout: 60)
endif

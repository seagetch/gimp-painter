# Isolated painter engine/resource code; standard libmypaint stays untouched.
painter_mypaint = static_library('painter-mypaint',
  'resource.cpp', 'engine.cpp',
  include_directories: rootAppInclude,
  dependencies: [painter_dep, painter_brush_settings_dep, json_glib, gio],
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
painter_mypaint_dep = declare_dependency(
  link_with: painter_mypaint,
  include_directories: include_directories('.'),
  dependencies: [painter_dep, painter_brush_settings_dep, json_glib, gio],
)
painter_mypaint_resource_test = executable('painter-mypaint-resource',
  'tests/test-resource.cpp',
  dependencies: painter_mypaint_dep,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
test('painter-mypaint-resource', painter_mypaint_resource_test,
  args: [meson.project_source_root() / 'data/painter-mypaint-brushes'],
  suite: 'painter', timeout: 60)

painter_mypaint_resource_trace = executable('painter-mypaint-resource-trace',
  'tests/resource-trace.cpp',
  dependencies: painter_mypaint_dep,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
if not meson.is_cross_build()
  test('painter-mypaint-legacy-decoder', painter_brush_settings_python,
    args: [files('tests/compare-trace.py'),
           meson.project_source_root() / 'migration/fixtures/legacy-mypaint/resource-values.tsv.gz',
           painter_mypaint_resource_trace,
           meson.project_source_root() / 'data/painter-mypaint-brushes'],
    suite: 'painter', timeout: 60)
endif

painter_mypaint_engine_trace = executable('painter-mypaint-engine-trace',
  'tests/engine-trace.cpp',
  dependencies: painter_mypaint_dep,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
if not meson.is_cross_build()
  test('painter-mypaint-legacy-engine', painter_brush_settings_python,
    args: [files('tests/compare-trace.py'),
           meson.project_source_root() / 'migration/fixtures/legacy-mypaint/engine.trace.gz',
           painter_mypaint_engine_trace,
           meson.project_source_root() / 'data/painter-mypaint-brushes'],
    suite: 'painter', timeout: 60)
endif

# Immutable evaluator state prerequisite for paired native stroke Undo.
painter_mypaint_checkpoint_test = executable('painter-mypaint-checkpoint',
  'tests/test-checkpoint.cpp', dependencies: painter_mypaint_dep,
  link_args: ['-Wl,--wrap=_Znwm'],
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-mypaint-checkpoint', painter_mypaint_checkpoint_test,
  suite: 'painter', timeout: 60)

# Allocation-free evaluator exchange for paired historical native Undo.
painter_mypaint_prepared_test = executable('painter-mypaint-prepared-state',
  'tests/test-prepared-state.cpp',
  dependencies: painter_mypaint_dep,
  cpp_args: painter_cpp_args,
  link_args: ['-Wl,--wrap=_Znwm'],
  override_options: ['cpp_std=c++14'],
)
test('painter-mypaint-prepared-state', painter_mypaint_prepared_test,
  suite: 'painter', timeout: 60)

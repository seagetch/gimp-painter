# The bridge uses GObject; independent feature workers also need native threads.
# GIMP/GEGL adapters stay in their owning app modules; existing C stays C.
painter_threads = dependency('threads')
painter_include = include_directories('.')
painter_cpp_args = cxx.get_supported_arguments(['-fexceptions', '-fno-rtti'])
if cxx.get_id() == 'msvc'
  painter_cpp_args += ['/EHsc', '/GR-']
endif

libapppainter = static_library('apppainter',
  'gimp-painter-error.cpp',
  'gimp-painter-binding.cpp',
  'binding-store.cpp',
  'filter-scheduler.cpp',
  'filter-context.cpp',
  'filter-wire.cpp',
  'filter-process.cpp',
  'filter-lifetime.cpp',
  'filter-raster.cpp',
  'filter-spool.cpp',
  'filter-raster-kernels.cpp',
  'filter-edge.cpp',
  'filter-gauss.cpp',
  'filter-native-kernels.cpp',
  dependencies: [gobject, painter_threads],
  include_directories: painter_include,
  cpp_args: painter_cpp_args,
  gnu_symbol_visibility: 'hidden',
  override_options: ['cpp_std=c++14'],
)

painter_dep = declare_dependency(
  link_with: libapppainter,
  dependencies: [gobject, painter_threads],
  include_directories: painter_include,
)

painter_foundation_test = executable('painter-foundation',
  'tests/test-c-api.c',
  'tests/test-fixture.c',
  'tests/test-hierarchy.c',
  'tests/test-hierarchy.cpp',
  'tests/test-foundation.cpp',
  'tests/test-resources.cpp',
  'tests/test-gobject.cpp',
  'tests/test-reentry.cpp',
  dependencies: [painter_dep, painter_threads],
  cpp_args: painter_cpp_args,
  gnu_symbol_visibility: 'hidden',
  override_options: ['cpp_std=c++14'],
  link_language: 'cpp',
)
test('painter-foundation', painter_foundation_test,
  suite: 'painter',
  timeout: 60,
)

# The feature scheduler and exact legacy-edge executor own no GIMP objects.
foreach feature : ['filter-wire', 'filter-process', 'filter-scheduler', 'filter-owner-gates', 'filter-edge', 'filter-gauss', 'filter-gray', 'filter-gauss-alias', 'fair-dispatcher', 'work-admission', 'filter-raster', 'filter-spool', 'filter-raster-kernels', 'filter-native-kernels']
  feature_cpp_args = painter_cpp_args
  feature_link_args = []
  if feature == 'filter-process' and host_machine.system() == 'linux'
    feature_cpp_args += ['-DFILTER_PROCESS_WRAP_REAPER']
    feature_link_args += ['-Wl,--wrap=waitid', '-Wl,--wrap=kill']
  endif
  feature_test = executable('painter-' + feature,
    'tests/test-' + feature + '.cpp',
    dependencies: [painter_dep, painter_threads],
    cpp_args: feature_cpp_args,
    link_args: feature_link_args,
    override_options: ['cpp_std=c++14'],
  )
  feature_args = []
  if feature == 'filter-native-kernels'
    feature_args = [meson.project_source_root() / 'migration/fixtures/legacy-filter-points/fixtures.tsv']
  elif feature == 'filter-edge'
    feature_args = [meson.project_source_root() / 'migration/fixtures/legacy-edge/fixtures.tsv']
  elif feature == 'filter-gauss'
    feature_args = [meson.project_source_root() / 'migration/fixtures/legacy-gauss/fixtures.tsv']
  elif feature == 'filter-gauss-alias'
    feature_args = [meson.project_source_root() / 'migration/fixtures/legacy-gauss-alias/fixtures.tsv',
                    meson.project_source_root() / 'migration/fixtures/legacy-gauss-negative/fixtures.tsv']
  elif feature == 'filter-raster-kernels'
    feature_args = [meson.project_source_root() / 'migration/fixtures/legacy-edge/fixtures.tsv',
                    meson.project_source_root() / 'migration/fixtures/legacy-gauss/fixtures.tsv',
                    meson.project_source_root() / 'migration/fixtures/legacy-gray-filter/fixtures.tsv']
  elif feature == 'filter-gray'
    feature_args = [meson.project_source_root() / 'migration/fixtures/legacy-gray-filter/fixtures.tsv']
  endif
  test('painter-' + feature, feature_test,
    args: feature_args,
    suite: 'painter', timeout: 60)
endforeach

filter_context_test = executable('painter-filter-context',
  'tests/test-filter-context.cpp',
  dependencies: [painter_dep, painter_threads],
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('painter-filter-context', python,
  args: [meson.project_source_root() / 'migration/tests/run_filter_context_fixture_test.py',
         filter_context_test,
         meson.project_source_root() / 'migration/fixtures/legacy-filter-context.tar.gz',
         meson.project_source_root() / 'migration/fixtures/legacy-filter-context.tar.manifest.json'],
  depends: [filter_context_test], suite: 'painter', timeout: 60)

# Deterministically pause the real spool at its two publication stores. The
# generated observation copy is linked only into this test; the application
# uses the unchanged production spool implementation with no test hooks.
observed_filter_spool = custom_target('observed-filter-spool',
  input: 'filter-spool.cpp', output: 'observed-filter-spool.cpp',
  command: [python, files('tests/observe-filter-spool.py'), '@INPUT@', '@OUTPUT@'])
filter_import_completion_test = executable('painter-filter-import-completion',
  'tests/test-filter-import-completion.cpp', observed_filter_spool,
  dependencies: [painter_dep, painter_threads],
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('painter-filter-import-completion', filter_import_completion_test,
  suite: 'painter', timeout: 30)

# A hidden app-only enum slot must never shift the public plug-in identities.
test('painter-public-mode-abi', python,
  args: [meson.project_source_root() / 'tools/check_painter_public_modes.py'],
  depends: [stamp_enumcode], suite: 'painter', timeout: 30)

painter_bounded_fill = static_library('painter-bounded-fill', 'search.cpp',
  dependencies: [painter_dep, gegl], include_directories: rootAppInclude,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
painter_bounded_fill_dep = declare_dependency(link_with: painter_bounded_fill,
  include_directories: include_directories('.'), dependencies: [painter_dep, gegl])
trace = executable('painter-bounded-fill-trace', 'tests/trace.cpp',
  dependencies: painter_bounded_fill_dep, include_directories: rootInclude,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-bounded-fill-legacy', painter_brush_settings_python,
    args: [files('../painter-mypaint/tests/compare-trace.py'),
      meson.project_source_root() / 'migration/fixtures/legacy-bounded-fill/masks.tsv.gz', trace],
    suite: 'painter', timeout: 60)
endif
search_test = executable('painter-bounded-fill-test', 'tests/test-search.cpp',
  dependencies: painter_bounded_fill_dep,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-bounded-fill', search_test, suite: 'painter', timeout: 60)

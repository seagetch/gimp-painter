# GObject is the only production dependency of the common painter bridge.
# Feature adapters stay in their owning app modules; existing C stays C.
painter_include = include_directories('.')
painter_cpp_args = cxx.get_supported_arguments(['-fexceptions', '-fno-rtti'])
if cxx.get_id() == 'msvc'
  painter_cpp_args += ['/EHsc', '/GR-']
endif

libapppainter = static_library('apppainter',
  'gimp-painter-error.cpp',
  'gimp-painter-binding.cpp',
  'binding-store.cpp',
  dependencies: gobject,
  include_directories: painter_include,
  cpp_args: painter_cpp_args,
  gnu_symbol_visibility: 'hidden',
  override_options: ['cpp_std=c++14'],
)

painter_dep = declare_dependency(
  link_with: libapppainter,
  dependencies: gobject,
  include_directories: painter_include,
)

painter_foundation_test = executable('painter-foundation',
  'tests/test-c-api.c',
  'tests/test-fixture.c',
  'tests/test-foundation.cpp',
  'tests/test-resources.cpp',
  'tests/test-gobject.cpp',
  'tests/test-reentry.cpp',
  dependencies: [painter_dep, dependency('threads')],
  cpp_args: painter_cpp_args,
  gnu_symbol_visibility: 'hidden',
  override_options: ['cpp_std=c++14'],
  link_language: 'cpp',
)
test('painter-foundation', painter_foundation_test,
  suite: 'painter',
  timeout: 60,
)

# This file is included by app/paint/meson.build. It is also tested in isolation.
# Both outputs belong to one edge so no consumer can observe mismatched indices.
painter_brush_settings_python = import('python').find_installation()
painter_brush_settings_generated = custom_target('painter-brush-settings-generated',
  input: 'brush-settings.json',
  output: ['mypaintbrush-enum-settings.h', 'mypaintbrush-settings-data.h'],
  command: [painter_brush_settings_python, '-B', files('generate.py'),
            '--manifest', '@INPUT@', '--output-dir', '@OUTDIR@'],
  build_by_default: true,
)
painter_brush_settings_dep = declare_dependency(
  sources: painter_brush_settings_generated,
  include_directories: include_directories('.'),
)

painter_brush_metadata_c = executable('painter-brush-metadata-c',
  'tests/test-data.c',
  dependencies: painter_brush_settings_dep,
  override_options: ['c_std=c11', 'b_ndebug=false'],
)
painter_brush_metadata_cpp = executable('painter-brush-metadata-cpp',
  'tests/test-data.cpp',
  dependencies: painter_brush_settings_dep,
  override_options: ['cpp_std=c++14', 'b_ndebug=false'],
)
test('painter-brush-metadata-c', painter_brush_metadata_c, suite: 'painter')
test('painter-brush-metadata-cpp', painter_brush_metadata_cpp, suite: 'painter')

painter_brush_generator_test_args = [files('tests/test-generate.py')]
if meson.is_cross_build()
  # The two Meson executable tests use Meson's configured cross runner instead.
  painter_brush_generator_test_args += ['--skip-compile']
else
  foreach arg : meson.get_compiler('c').cmd_array()
    painter_brush_generator_test_args += ['--cc-arg=' + arg]
  endforeach
  foreach arg : meson.get_compiler('cpp').cmd_array()
    painter_brush_generator_test_args += ['--cxx-arg=' + arg]
  endforeach
endif
test('painter-brush-generator', painter_brush_settings_python,
  args: ['-B'] + painter_brush_generator_test_args,
  suite: 'painter',
  timeout: 60,
)

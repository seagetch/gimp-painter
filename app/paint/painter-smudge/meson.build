painter_smudge_pixels_trace = executable('painter-smudge-pixels-trace',
  'tests/pixels-trace.cpp', include_directories: include_directories('.'),
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-smudge-legacy-pixels', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_smudge_pixels.py',
           meson.project_source_root() / 'migration/fixtures/legacy-smudge-pixels/pixels.tsv.gz',
           painter_smudge_pixels_trace], suite: 'painter', timeout: 60)
endif

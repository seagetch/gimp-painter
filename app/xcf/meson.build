libappxcf_sources = [
  'painter-xcf-compat.cpp',
  'painter-xcf-load.cpp',
  'xcf-load.c',
  'xcf-read.c',
  'xcf-save.c',
  'xcf-seek.c',
  'xcf-utils.c',
  'xcf-write.c',
  'xcf.c',
]

libappxcf = static_library('appxcf',
  libappxcf_sources,
  include_directories: [ rootInclude, rootAppInclude, ],
  c_args: '-DG_LOG_DOMAIN="Gimp-XCF"',
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
  dependencies: [
    cairo, gegl, gdk_pixbuf, gexiv2, zlib
  ],
)

# Pure byte-record groundwork. This does not enable legacy files in normal Open.
painter_xcf_compat_test = executable('painter-xcf-compat-test',
  'painter-xcf-compat.cpp',
  'tests/test-painter-xcf-compat.cpp',
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
test('painter-xcf-compat', painter_xcf_compat_test,
  args: [
    meson.project_source_root() / 'migration/fixtures/legacy-runtime',
    meson.project_source_root() / 'migration/fixtures/gimp3-baseline.xcf',
  ],
  suite: 'painter',
  timeout: 60,
)

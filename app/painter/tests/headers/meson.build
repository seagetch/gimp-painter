# Build the probes with Meson's selected compilers/dependencies, including in
# cross builds. No generated target executable or host compiler is invoked.
painter_header_generator = files('generate-probes.py')
painter_header_http = painter_http_enabled ? '1' : '0'
painter_header_scope_args = [
  '--scope', files('../../../../migration/inventory/c-header-linkage.json'),
  '--private-scope', files('../../../../migration/inventory/cpp-header-boundary.json')]
painter_header_names = run_command(python, painter_header_generator,
  painter_header_scope_args, '--http', painter_header_http, '--list',
  check: true).stdout().strip().split('\n')
painter_header_input_names = run_command(python, painter_header_generator,
  painter_header_scope_args, '--http', painter_header_http, '--list-inputs',
  check: true).stdout().strip().split('\n')
painter_header_inputs = []
foreach path : painter_header_input_names
  painter_header_inputs += files(meson.project_source_root() / path)
endforeach
painter_header_sources = custom_target('painter-header-probe-sources',
  input: painter_header_inputs,
  output: painter_header_names,
  command: [python, painter_header_generator, '--http', painter_header_http,
            '--output-dir', '@OUTDIR@'],
  build_by_default: false)
painter_header_c_sources = []
painter_header_cpp_sources = []
foreach index : range(painter_header_names.length())
  if painter_header_names[index].endswith('.cpp')
    painter_header_cpp_sources += painter_header_sources[index]
  elif painter_header_names[index].endswith('.c')
    painter_header_c_sources += painter_header_sources[index]
  endif
endforeach
painter_header_c = static_library('painter-public-headers-c',
  painter_header_c_sources,
  include_directories: [rootInclude, rootAppInclude],
  dependencies: [gegl, gtk3, gexiv2],
  c_args: cc.get_supported_arguments(['-Wno-error=ignored-qualifiers']),
  override_options: ['c_std=c11', 'warning_level=2', 'werror=true'],
  build_by_default: false, install: false)
painter_header_cpp = static_library('painter-public-headers-cpp',
  painter_header_cpp_sources,
  include_directories: [rootInclude, rootAppInclude],
  dependencies: [gegl, gtk3, gexiv2, json_glib, painter_brush_settings_dep],
  cpp_args: painter_cpp_args + cxx.get_supported_arguments(['-Wno-error=ignored-qualifiers']),
  override_options: ['cpp_std=c++14', 'warning_level=2', 'werror=true'],
  build_by_default: false, install: false)
test('painter-public-header-compilation', python,
  args: [painter_header_generator, '--http', painter_header_http,
         '--verify-build', meson.current_build_dir(),
         '--library', painter_header_c.full_path(),
         '--library', painter_header_cpp.full_path(),
         '--report', meson.current_build_dir() / 'header-compile-result.json'],
  depends: [painter_header_c, painter_header_cpp],
  suite: 'painter', timeout: 60)

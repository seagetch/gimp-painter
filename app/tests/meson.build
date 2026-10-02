apptests_links = [
  libappconfig,
  libappactions,
  libappdialogs,
  libappdisplay,
  libappgui,
  libappmenus,
  libapppropgui,
  libapptools,
  libappwidgets,
  libgimpbase,
  libgimpcolor,
  libgimpconfig,
  libgimpmath,
  libgimpmodule,
  libgimpthumb,
  libgimpwidgets,
]


libapptestutils_sources = [
  'gimp-app-test-utils.c',
  'gimp-test-session-utils.c',
]

libapptestutils = static_library('apptestutils',
  libapptestutils_sources,
  dependencies: libapp_dep,
  link_with: apptests_links,
)

apptests_links += libapptestutils

# TODO: we need to implement proper GUI unit testing, actually based on
# the real GIMP and simulating pointer clicks or keyboard inputs the
# proper way.
app_tests = [
  'core',
  'gimp-clone-layer',
  'gimp-filter-layer',
  'painter-layer-ui',
  'gimp-painter-mybrush',
  'gimp-painter-mybrush-factory',
  'gimpidtable',
  'gimp-tool-owner',
#'session-2-8-compatibility-multi-window',
#'session-2-8-compatibility-single-window',
#'single-window-mode',
#'tools',
#'ui',
  'xcf',
  'painter-xcf-open',
  'painter-xcf-roundtrip',
  'painter-layer-modes',
  'painter-navigation-ui',
  'painter-navigation-events',
  'painter-perspective',
  'painter-perspective-ui',
  'painter-perspective-events',
  'gimp-painter-context',
]

# Prevent parallel builds for the tests
# The tests must not be run in parallel or in a different order as specified

prio = 1000
foreach test_name : app_tests
  test_exe = executable(test_name,
    'test-@0@.c'.format(test_name),
    'tests.c',
    test_name == 'gimp-clone-layer' ? ['test-gimp-clone-layout.cpp'] : [],
    test_name == 'gimp-filter-layer' ? ['test-gimp-filter-layout.cpp'] : [],
    dependencies: [ libapp_dep, appstream ],
    link_with: apptests_links,
  )

  test(test_name,
    test_exe,
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR='  + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR='+ meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS='      + meson.project_build_root()/'plug-ins'/'common',
      'UI_TEST=yes',
    ],
    suite: 'app',
    timeout: 60,
    is_parallel : false,
    priority: prio,
  )

  prio = prio - 10

endforeach

## Newer Tests using GIMP directly ##

if not meson.can_run_host_binaries()
  warning('XCF loading unit testing disabled in cross-building or similar environments.')
  subdir_done()
endif

tests = [
  {
    'name': 'save-and-export',
  }
]

test_env=gimp_run_env
test_env.set('GIMP_TESTING_ABS_TOP_SRCDIR', meson.project_source_root())
test_env.set('GIMP_TESTING_ABS_TOP_BUILDDIR', meson.project_build_root())

run_python_test = find_program(meson.project_source_root() / 'libgimp/tests/libgimp-run-python-test.py')
foreach testinfo : tests
  test_name = testinfo['name']
  basename = 'test-' + testinfo['name']

  py_test  = meson.current_source_dir() / basename + '.py'
  if testinfo.has_key('input')
    test(test_name, run_python_test,
         args: [ gimp_exe.full_path(), py_test, testinfo['input']],
         env: test_env,
         suite: ['app'],
         timeout: 90)
  else
    test(test_name, run_python_test,
         args: [ gimp_exe.full_path(), py_test ],
         env: test_env,
         suite: ['app'],
         timeout: 90)
  endif
endforeach

painter_surface_test = executable('gimp-painter-surface',
  'test-gimp-painter-surface.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
test('gimp-painter-surface', painter_surface_test,
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'UI_TEST=yes',
  ], suite: 'app', is_parallel: false, timeout: 60)

# Real drawable/Undo pixels against the independent warmed old application.
painter_session_trace = executable('painter-mypaint-session-trace',
  'painter-mypaint-session-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
if not meson.is_cross_build()
  test('painter-mypaint-legacy-session', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_mypaint_sessions.py',
           meson.project_source_root() / 'migration/fixtures/legacy-mypaint-session/session-values.tsv.gz',
           painter_session_trace],
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
      'GIMP3_DIRECTORY=' + meson.project_build_root() / 'app/tests/gimpdir-output',
      'UI_TEST=yes',
    ], suite: 'painter', is_parallel: false, timeout: 60)
endif

painter_options_test = executable('gimp-painter-options',
  'test-gimp-painter-options.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
test('gimp-painter-options', painter_options_test,
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'UI_TEST=yes',
  ], suite: 'app', is_parallel: false, timeout: 60)

painter_session_test = executable('gimp-painter-session',
  'test-gimp-painter-session.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
test('gimp-painter-session', painter_session_test,
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'UI_TEST=yes',
  ], suite: 'app', is_parallel: false, timeout: 60)

painter_hover_test = executable('gimp-painter-hover',
  'test-gimp-painter-hover.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
test('gimp-painter-hover', painter_hover_test,
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'UI_TEST=yes',
  ], suite: 'app', is_parallel: false, timeout: 60)


# Native Fill Brush transaction trace against pinned old real paint/Undo output.
painter_fill_trace = executable('painter-fill-brush-trace',
  'painter-fill-brush-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_bounded_fill_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])

fill_core_test = executable('gimp-fill-brush', 'test-gimp-fill-brush.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_bounded_fill_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('gimp-fill-brush', fill_core_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)

if not meson.is_cross_build()
  foreach fill_variant : ['', 'queued']
    test('painter-fill-brush-legacy' + fill_variant, painter_brush_settings_python,
      args: [meson.project_source_root() / 'migration/tests/compare_fill_brush.py',
             meson.project_source_root() / 'migration/fixtures/legacy-fill-brush/pixels.tsv.gz',
             painter_fill_trace] + (fill_variant == '' ? [] : [fill_variant]),
      env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
            'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
            'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
            'GIMP3_DIRECTORY=' + meson.project_build_root() / 'app/tests/gimpdir-output',
            'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 60)
  endforeach
endif

# Real native owner-context Fill Brush tool lifecycle, including queued input.
fill_brush_ui_test = executable('painter-fill-brush-ui', 'test-painter-fill-brush-ui.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_bounded_fill_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-fill-brush-ui', fill_brush_ui_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

layer_presets_test = executable('gimp-layer-presets', 'test-gimp-layer-presets.c', 'tests.c',
  dependencies: [libapp_dep, json_glib, appstream], link_with: apptests_links)
test('gimp-layer-presets', layer_presets_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 90)

layer_presets_ui_test = executable('layer-presets-ui', 'test-layer-presets-ui.c', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links)
test('layer-presets-ui', layer_presets_ui_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

painter_tool_test = executable('painter-mypaint-tool', 'test-painter-mypaint-tool.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-mypaint-tool', painter_tool_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

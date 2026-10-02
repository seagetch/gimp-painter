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

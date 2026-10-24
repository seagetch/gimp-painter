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


# tests.c already prepends this supported build-tree icon search root.
gimp_test_icon_output_dir = meson.current_build_dir() / 'gimp-test-icon-theme'
subdir('icon-theme-fixture')

libapptestutils_sources = [
  'gimp-app-test-utils.c',
  'gimp-test-session-utils.c',
]

libapptestutils = static_library('apptestutils',
  libapptestutils_sources + gimp_test_icon_dependencies,
  dependencies: libapp_dep,
  link_with: apptests_links,
)

apptests_links += libapptestutils

# TODO: we need to implement proper GUI unit testing, actually based on
# the real GIMP and simulating pointer clicks or keyboard inputs the
# proper way.
app_tests = [
  'core',
  'projection-update-bounds',
  'gimp-test-config-fixture',
  'painter-type-startup',
  'file-load-pipeline',
  'gimp-clone-layer',
  'gimp-filter-layer',
  'painter-layer-ui',
  'painter-layer-tree',
  'painter-canvas-ui',
  'painter-toolbar-window',
  'painter-session-restore-barrier',
  'painter-compact-options',
  'gimp-painter-mybrush',
  'gimp-painter-mybrush-factory',
  'gimpidtable',
  'gimp-container-deserialize',
  'gimp-tool-owner',
#'session-2-8-compatibility-multi-window',
#'session-2-8-compatibility-single-window',
#'single-window-mode',
#'tools',
#'ui',
  'xcf',
  'painter-xcf-open',
  'painter-xcf-adversarial',
  'painter-xcf-roundtrip',
  'painter-provenance',
  'painter-layer-modes',
  'painter-mode-ui',
  'painter-dynamics',
  'painter-core-metadata',
  'painter-ancestry-contract',
  'painter-navigation-ui',
  'painter-navigation-events',
  'device-dialog-events',
  'painter-perspective',
  'painter-perspective-ui',
  'painter-perspective-events',
  'gimp-painter-context',
  'mypaint-selector-contracts',
]

# Pure helper coverage is separate from the real top-level lifecycle test.
update_geometry = executable('update-geometry', 'test-update-geometry.c',
  include_directories: [rootAppInclude],
  dependencies: [glib, math], override_options: ['c_std=c11'])
test('update-geometry', update_geometry, suite: 'app',
  is_parallel: false, timeout: 30)
test('projection-update-sanitizer-runner', python,
  args: [meson.project_source_root() / 'migration/tests/test_projection_update_sanitizers.py'],
  suite: 'app', is_parallel: false, timeout: 30)

# Prevent parallel builds for the tests
# The tests must not be run in parallel or in a different order as specified

# This one target can simulate gdk_pixbuf_copy() allocation failure without
# changing production code or wrapping any allocator process-wide.
mybrush_copy_fault_args = []
mybrush_copy_fault_link_args = []
mybrush_copy_fault_wrap = '-Wl,--wrap=gdk_pixbuf_copy'
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_link_argument(mybrush_copy_fault_wrap)
  mybrush_copy_fault_args = ['-DGIMP_TEST_MYBRUSH_WRAP_PIXBUF_COPY']
  mybrush_copy_fault_link_args = [mybrush_copy_fault_wrap]
endif

# Inject one selected PNG encoder failure without changing the provider.
mybrush_encode_fault_wrap = '-Wl,--wrap=gdk_pixbuf_save_to_buffer'
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_link_argument(mybrush_encode_fault_wrap)
  mybrush_copy_fault_args += ['-DGIMP_TEST_MYBRUSH_WRAP_PIXBUF_ENCODE']
  mybrush_copy_fault_link_args += [mybrush_encode_fault_wrap]
endif

# Prove named clone setters traverse their original C symbols without adding
# instrumentation to the production provider.
clone_route_args = []
clone_route_link_args = []
clone_source_wrap = '-Wl,--wrap=gimp_clone_layer_set_source'
clone_name_wrap = '-Wl,--wrap=gimp_clone_layer_set_source_by_name'
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_link_argument(clone_source_wrap) and cxx.has_link_argument(clone_name_wrap)
  clone_route_args = ['-DGIMP_TEST_CLONE_WRAP_SETTERS']
  clone_route_link_args = [clone_source_wrap, clone_name_wrap]
endif

# Prove the owning ContextRef method delegates to the original C factory.
selector_route_args = []
selector_route_link_args = []
selector_factory_wrap = '-Wl,--wrap=gimp_mypaint_brush_button_with_popup'
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_link_argument(selector_factory_wrap)
  selector_route_args = ['-DGIMP_TEST_MYPAINT_SELECTOR_WRAP_FACTORY']
  selector_route_link_args = [selector_factory_wrap]
endif

prio = 1000
foreach test_name : app_tests
  test_exe = executable(test_name,
    'test-@0@.@1@'.format(test_name, test_name == 'painter-type-startup' ? 'cpp' : 'c'),
    'tests.c',
    test_name == 'painter-perspective' ? ['test-painter-perspective-handle.cpp'] : [],
    test_name == 'painter-perspective-ui' ? ['test-painter-perspective-path-oracle.cpp'] : [],
    test_name == 'mypaint-selector-contracts' ? ['test-mypaint-selector-contracts.cpp'] : [],
    test_name == 'painter-compact-options' ? ['test-widget-orientation-api.c', 'test-widget-orientation-api.cpp'] : [],
    test_name == 'gimp-clone-layer' ? ['test-gimp-clone-layout.cpp'] : [],
    test_name == 'gimp-painter-mybrush' ? ['test-painter-mybrush-duplicate-handle.cpp'] : [],
    test_name == 'painter-xcf-roundtrip' ? ['test-painter-xcf-argument-resources.cpp'] : [],
    test_name == 'painter-layer-ui' ? ['test-filter-editor-metadata.cpp', 'test-filter-argument-patch.cpp'] : [],
    test_name == 'gimp-filter-layer' ? ['test-gimp-filter-layout.cpp', 'test-filter-owner-context.cpp', 'test-filter-parameters.cpp'] : [],
    c_args: test_name == 'gimp-painter-mybrush' ? mybrush_copy_fault_args : [],
    cpp_args: (test_name == 'gimp-painter-mybrush' ? mybrush_copy_fault_args : []) +
      (test_name == 'gimp-clone-layer' ? clone_route_args : []) +
      (test_name == 'mypaint-selector-contracts' ? painter_cpp_args + selector_route_args : []) +
      (test_name == 'painter-perspective' ? painter_cpp_args : []),
    link_args: (test_name == 'gimp-painter-mybrush' ? mybrush_copy_fault_link_args : []) +
      (test_name == 'gimp-clone-layer' ? clone_route_link_args : []) +
      (test_name == 'mypaint-selector-contracts' ? selector_route_link_args : []) +
      (test_name == 'painter-perspective' ? ['-Wl,--wrap=_ZN11GimpPainter12BindingStore6ensureEP8_GObject', '-Wl,--wrap=_ZN11GimpPainter12BindingStore8activateEv'] : []),
    dependencies: [ libapp_dep, appstream ] +
      (test_name == 'gimp-painter-mybrush' ? [painter_brush_settings_dep] : []) +
      (test_name == 'mypaint-selector-contracts' ? [painter_mypaint_surface_dep] : []),
    link_with: apptests_links,
    override_options: (['painter-compact-options', 'gimp-painter-mybrush', 'painter-layer-tree', 'gimp-clone-layer', 'mypaint-selector-contracts'].contains(test_name) ? ['c_std=c11', 'cpp_std=c++14'] : []) +
      (test_name == 'painter-perspective' ? ['c_std=c11', 'cpp_std=c++14'] : []),
  )

  # Filter tests include real 8193x8193 spill and hundreds of old Gauss buffers.
  test_timeout = test_name == 'gimp-filter-layer' ? 600 : 60
  if test_name == 'mypaint-selector-contracts'
    test_timeout = 180
  endif
  if ['painter-toolbar-window', 'painter-session-restore-barrier'].contains(test_name)
    # Every case uses fresh GUI startup; session coverage executes two passes.
    test_timeout = 240
  endif
  if test_name == 'painter-compact-options'
    # The first real-input case has >60s of required settle intervals alone.
    # All 47 unchanged cases took 99.38s on authenticated portrait TCP Xvfb.
    test_timeout = 180
  endif
  if test_name == 'painter-xcf-open'
    # Genuine Sharpen/Threshold writer epochs plus vector/spill active saves.
    test_timeout = 180
  endif
  if test_name == 'painter-xcf-roundtrip'
    # Include the >256 MiB public Save/Open case in the registered aggregate.
    # The unchanged large case alone measured 38.96s on portrait TCP Xvfb.
    test_timeout = 180
  endif
  if test_name == 'painter-layer-ui'
    # All 63 cases took 83.69s under Xvfb; keep the full suite within its budget.
    test_timeout = 180
  endif

  test_args = []
  if ['gimp-filter-layer', 'painter-xcf-open'].contains(test_name)
    test_args = [meson.project_source_root() /
      'migration/tests/run_filter_owner_context_fixture_test.py', '--', test_exe]
  elif test_name == 'painter-xcf-roundtrip'
    test_args = ['-m', 'slow']
  endif

  if test_name == 'painter-perspective-ui'
    # Preserve the original 24-case process budget; register every added case below.
    test_args = ['--skip-prefix', '/perspective-ui/damage',
                 '--skip-prefix', '/perspective-ui/old-path']
  endif

  test(test_name,
    ['gimp-filter-layer', 'painter-xcf-open'].contains(test_name) ? python : test_exe,
    args: test_args,
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR='  + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR='+ meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS='      + meson.project_build_root()/'plug-ins'/'common',
      'UI_TEST=yes',
    ] + (test_name == 'painter-canvas-ui' ?
      ['GIMP_TEST_PAINTER_CANVAS_GROUP=original'] : []),
    suite: 'app',
    depends: gimp_test_icon_dependencies,
    timeout: test_timeout,
    is_parallel : false,
    priority: prio,
  )

  if test_name == 'painter-perspective-ui'
    # Each disjoint scope keeps the original 60-second timeout, without skips.
    foreach scope : [
      ['geometry', ['-p', '/perspective-ui/damage-ink-bounds',
                    '-p', '/perspective-ui/old-path-pixels']],
      ['damage-0', ['-p', '/perspective-ui/damage-mutations/0']],
      ['damage-1', ['-p', '/perspective-ui/damage-mutations/1']],
      ['damage-2', ['-p', '/perspective-ui/damage-mutations/2']],
    ]
      test('painter-perspective-ui-' + scope[0], test_exe,
        args: scope[1],
        env: [
          'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes',
        ],
        suite: 'app', depends: gimp_test_icon_dependencies,
        timeout: 60, is_parallel: false, priority: prio,
      )
    endforeach
  endif

  if test_name == 'painter-canvas-ui'
    # Both registrations use the same binary and preserve their original IDs.
    test('painter-right-docks-ui', test_exe,
      env: [
        'GIMP_TESTING_ABS_TOP_SRCDIR='  + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR='+ meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS='      + meson.project_build_root()/'plug-ins'/'common',
        'UI_TEST=yes',
        'GIMP_TEST_PAINTER_CANVAS_GROUP=right-docks',
      ],
      suite: 'app',
      depends: gimp_test_icon_dependencies,
      timeout: test_timeout,
      is_parallel: false,
      # Preserve the full harness's right-docks-before-original-cases order.
      priority: prio + 1,
    )
    test('painter-dock-session-ui', test_exe,
      env: [
        'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root()/'plug-ins'/'common',
        'UI_TEST=yes',
        'GIMP_TEST_PAINTER_CANVAS_GROUP=dock-session',
      ],
      suite: 'app',
      depends: gimp_test_icon_dependencies,
      timeout: test_timeout,
      is_parallel: false,
      priority: prio + 2,
    )
    test('painter-dock-input-ui', test_exe,
      env: [
        'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root()/'plug-ins'/'common',
        'UI_TEST=yes',
        'GIMP_TEST_PAINTER_CANVAS_GROUP=dock-input',
      ],
      suite: 'app',
      depends: gimp_test_icon_dependencies,
      timeout: test_timeout,
      is_parallel: false,
      priority: prio + 3,
    )
    test('painter-dock-layout-ui', test_exe,
      env: [
        'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root()/'plug-ins'/'common',
        'UI_TEST=yes',
        'GIMP_TEST_PAINTER_CANVAS_GROUP=dock-layout',
      ],
      suite: 'app',
      depends: gimp_test_icon_dependencies,
      timeout: test_timeout,
      is_parallel: false,
      priority: prio + 4,
    )
  endif

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

test('in-build-gimp-wrapper', python,
  args: ['-B', meson.project_source_root() / 'migration/tests/test_in_build_gimp.py'],
  suite: 'app', is_parallel: false, timeout: 60)
test('script-fu-startup', python,
  args: ['-B', meson.project_source_root() / 'migration/tests/test_script_fu_startup.py'],
  env: test_env, suite: 'app', is_parallel: false, timeout: 480)

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

# Private ELF test seam: inject before a protected widget operation starts.
painter_widget_exception_wrap = '-Wl,--wrap=_ZNK11GimpPainter12BindingStore12check_threadEv'
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_link_argument(painter_widget_exception_wrap)
  painter_widget_exceptions_test = executable('painter-widget-exceptions',
    'test-painter-widget-exceptions.c',
    'test-painter-widget-exception-fault.cpp',
    'tests.c',
    dependencies: [libapp_dep, appstream],
    link_with: apptests_links,
    cpp_args: painter_cpp_args,
    link_args: [painter_widget_exception_wrap],
    override_options: ['c_std=c11', 'cpp_std=c++14'])
  test('painter-widget-exceptions', painter_widget_exceptions_test,
    depends: gimp_test_icon_dependencies,
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
      'UI_TEST=yes',
    ], suite: 'app', is_parallel: false, timeout: 60)
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

# Exercise exported Options/Session entry points from an actual C translation unit.
painter_options_session_c = executable('painter-options-session-c',
  'test-painter-options-session-c.c', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  override_options: ['c_std=c11'],
)
test('painter-options-session-c', painter_options_session_c,
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'UI_TEST=yes',
  ], suite: 'app', is_parallel: false, timeout: 60)

painter_qdata_test = executable('gimp-painter-qdata-operations',
  'test-gimp-painter-qdata-operations.cpp', 'tests.c',
  observed_binding_store, binding_store_observation,
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('gimp-painter-qdata-operations', painter_qdata_test,
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
  foreach fill_variant : ['', 'queued', 'async']
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

painter_tool_test = executable('painter-mypaint-tool', 'test-painter-mypaint-tool.cpp',
  'test-mypaint-colorpicker-c.c', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['c_std=c11', 'cpp_std=c++14'])
test('painter-mypaint-tool', painter_tool_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Captured native pre-refactor interpolation, compared at arbitrary pause points.
native_interpolation_trace = executable('painter-brush-interpolation-trace',
  'painter-brush-interpolation-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_bounded_fill_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-native-interpolation', painter_brush_settings_python,
  args: [meson.project_source_root() / 'migration/tests/compare_native_interpolation.py',
         meson.project_source_root() / 'migration/fixtures/native-brush-interpolation/trace.tsv.gz',
         native_interpolation_trace],
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'GIMP3_DIRECTORY=' + meson.project_build_root() / 'app/tests/gimpdir-output',
        'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)

# Preview snapshots isolate native pipe selection and process-global randomness.
painter_preview_test = executable('gimp-painter-preview', 'test-gimp-painter-preview.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('gimp-painter-preview', painter_preview_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)

# Real old RGB-u8 sessions, including actual alpha lock and erasing.
painter_rgb_trace = executable('painter-mypaint-rgb-session-trace',
  'painter-mypaint-rgb-session-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-legacy-rgb-session', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_mypaint_rgb_sessions.py',
           meson.project_source_root() / 'migration/fixtures/legacy-mypaint-rgb-session/session-values.tsv.gz',
           painter_rgb_trace],
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)
  test('painter-mypaint-rgb-session-fixtures', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/test_mypaint_rgb_session_fixtures.py'],
    suite: 'painter', is_parallel: false, timeout: 60)
endif

painter_smudge_trace = executable('painter-smudge-trace', 'painter-smudge-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])

# All six pinned old generic routes are ordinary, independently reported tests.
if not meson.is_cross_build()
  foreach generic_family : ['fill-brush', 'smudge']
    generic_trace = generic_family == 'fill-brush' ? painter_fill_trace : painter_smudge_trace
    foreach generic_route : ['raw', 'path', 'boundary']
      generic_name = 'painter-owned-generic-' + generic_family + '-' + generic_route
      test(generic_name, painter_brush_settings_python,
        args: ['-B', meson.project_source_root() / 'migration/tests/compare_owned_generic.py',
               meson.project_build_root(), '--family', generic_family,
               '--route', generic_route, '--binary', generic_trace,
               '--report', meson.current_build_dir() / (generic_name + '.json')],
        depends: [generic_trace],
        suite: 'painter', is_parallel: false, timeout: 120)
    endforeach
  endforeach
endif
test('painter-owned-generic-comparator', painter_brush_settings_python,
  args: ['-B', meson.project_source_root() / 'migration/tests/test_owned_generic_comparator.py'],
  suite: 'painter', is_parallel: false, timeout: 30)

if not meson.is_cross_build()
  test('painter-smudge-legacy-strokes', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_smudge_strokes.py',
           meson.project_source_root() / 'migration/fixtures/legacy-smudge/pixels.tsv.gz',
           painter_smudge_trace],
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)
endif

painter_smudge_test = executable('gimp-painter-smudge', 'test-gimp-painter-smudge.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('gimp-painter-smudge', painter_smudge_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)
if not meson.is_cross_build()
  test('painter-smudge-owned-strokes', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_smudge_strokes.py',
           meson.project_source_root() / 'migration/fixtures/legacy-smudge/pixels.tsv.gz',
           painter_smudge_trace, 'owned'],
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)
endif

painter_editor_test = executable('painter-mypaint-editor', 'test-painter-mypaint-editor.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-mypaint-editor', painter_editor_test,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Keep the original editor suite and its 120s budget unchanged.
# The two additional page-memory lifecycle regressions have their own bounded run.
test('painter-editor-page-memory', painter_editor_test,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TEST_EDITOR_PAGE_MEMORY=1','GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Direct name/save/delete ownership has a separate unchanged 120s budget.
test('painter-editor-control-lifetimes', painter_editor_test,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TEST_EDITOR_CONTROL_LIFETIMES=1','GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

test('painter-mypaint-preview-fixture', painter_brush_settings_python,
  args: ['-B', meson.project_source_root() / 'migration/tests/test_mypaint_preview_fixture.py'],
  suite: 'painter', timeout: 30)

painter_smudge_ui_test = executable('painter-smudge-ui', 'test-painter-smudge-ui.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-smudge-ui', painter_smudge_ui_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 180)

painter_save_continuation_test = executable('painter-save-continuation',
  'test-painter-save-continuation.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_bounded_fill_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-save-continuation', painter_save_continuation_test,
  # Exercise save/dialog continuations without requiring persistent dconf/D-Bus.
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes', 'GSETTINGS_BACKEND=memory'], suite: 'app', is_parallel: false, timeout: 120)

painter_batch_test = executable('gimp-painter-batch', 'test-gimp-painter-batch.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('gimp-painter-batch', painter_batch_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Real old one-byte Gray-u8 sessions, including actual alpha lock and erasing.
painter_gray_trace = executable('painter-mypaint-gray-session-trace',
  'painter-mypaint-gray-session-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
if not meson.is_cross_build()
  test('painter-mypaint-legacy-gray-session', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_mypaint_gray_sessions.py',
           meson.project_source_root() / 'migration/fixtures/legacy-mypaint-gray-session/session-values.tsv.gz',
           painter_gray_trace],
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)
  test('painter-mypaint-gray-session-fixtures', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/test_mypaint_gray_session_fixtures.py'],
    suite: 'painter', is_parallel: false, timeout: 60)
endif


painter_gray_test = executable('gimp-painter-gray', 'test-gimp-painter-gray.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('gimp-painter-gray', painter_gray_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

painter_gray_extension_test = executable('gimp-painter-gray-extension', 'test-gimp-painter-gray-extension.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('gimp-painter-gray-extension', painter_gray_extension_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Grammar-aware first-run profile import is independently testable without GTK.
painter_profile_parser = executable('painter-profile-parser',
  'test-painter-profile-parser.cpp', '../core/gimppainterprofile.cpp',
  include_directories: [rootAppInclude], dependencies: [gio],
  override_options: ['cpp_std=c++14'])
test('painter-profile-parser', painter_profile_parser, suite: 'painter',
  is_parallel: false, timeout: 60)
painter_profile_native = executable('painter-profile-native',
  'test-painter-profile-native.cpp', 'tests.c',
  'test-painter-profile-image-generator-c.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('painter-profile-native', painter_profile_native,
  # The first-run installer copies generated tags, even in an uninstalled build.
  depends: [gimp_tags_default],
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'GIMP3_DATADIR=' + meson.project_build_root() / 'data',
    'UI_TEST=yes',
  ], suite: 'painter', is_parallel: false, timeout: 120)

painter_paper_trace = executable('painter-paper-trace', 'painter-paper-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])

painter_paper_test = executable('painter-paper', 'test-painter-paper.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-paper', painter_paper_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)

painter_paper_ui_test = executable('painter-paper-ui', 'test-painter-paper-ui.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-paper-ui', painter_paper_ui_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)

if not meson.is_cross_build()
  foreach case : ['legacy-paper', 'legacy-paper-modes']
    paper_args = [meson.project_source_root() / 'migration/tests/compare_paper.py',
                  meson.project_source_root() / 'migration/fixtures' / case / 'pixels.tsv.gz',
                  painter_paper_trace]
    if case == 'legacy-paper-modes'
      paper_args += ['--modes']
    endif
    test('painter-' + case, painter_brush_settings_python, args: paper_args,
      env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
            'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
            'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
            'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 120)
  endforeach
endif

# Active native brush pipes: independent warmed legacy pixels and selector state.
painter_pipe_trace = executable('painter-mypaint-pipe-trace',
  'painter-mypaint-pipe-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links,
  cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'],
)
if not meson.is_cross_build()
  test('painter-mypaint-legacy-pipe', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/compare_mypaint_pipe.py',
           meson.project_source_root() / 'migration/fixtures/legacy-mypaint-pipe/pipe-values.tsv.gz',
           painter_pipe_trace],
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
      'GIMP3_DIRECTORY=' + meson.project_build_root() / 'app/tests/gimpdir-output',
      'UI_TEST=yes',
    ], suite: 'painter', is_parallel: false, timeout: 60)
endif

# Explicit-provenance ordinary geometry: real old and untouched-modern oracles.
painter_brush_geometry_trace = executable('painter-brush-geometry-trace', 'painter-brush-geometry-trace.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
painter_brush_geometry_test = executable('painter-brush-geometry', 'test-painter-brush-geometry.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('painter-brush-geometry', painter_brush_geometry_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes', 'GSETTINGS_BACKEND=memory'], suite: 'painter', is_parallel: false, timeout: 120)
if not meson.is_cross_build()
  foreach geometry_case : ['strokes', 'pixmap']
    test('painter-brush-geometry-' + geometry_case, painter_brush_settings_python,
      args: [meson.project_source_root() / 'migration/tests/compare_brush_geometry.py',
             painter_brush_geometry_trace,
             '--report', meson.current_build_dir() / ('brush-geometry-' + geometry_case + '.json')] +
            (geometry_case == 'pixmap' ? ['--pixmap'] : []),
      env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
            'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
            'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
            'UI_TEST=yes'], suite: 'painter', is_parallel: false, timeout: 180)
  endforeach
  test('painter-brush-geometry-fixtures', painter_brush_settings_python,
    args: [meson.project_source_root() / 'migration/tests/test_brush_geometry_fixtures.py'],
    suite: 'painter', is_parallel: false, timeout: 60)
endif

if painter_http_enabled
  painter_http_test = executable('painter-http', 'test-painter-http.cpp', 'tests.c',
    dependencies: [libapp_dep, appstream, json_glib, painter_http_soup],
    link_with: apptests_links, cpp_args: painter_cpp_args,
    override_options: ['cpp_std=c++14'])
  test('painter-http', painter_http_test,
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes', 'GSETTINGS_BACKEND=memory'], suite: 'painter', is_parallel: false, timeout: 90)
endif

if painter_http_enabled
  painter_http_ui_test = executable('painter-http-ui', 'test-painter-http-ui.cpp', 'tests.c',
    dependencies: [libapp_dep, appstream, json_glib, painter_http_soup],
    link_with: [apptests_links, libapphttpdgui], cpp_args: painter_cpp_args,
    override_options: ['cpp_std=c++14'])
  test('painter-http-ui', painter_http_ui_test,
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes', 'GSETTINGS_BACKEND=memory'], suite: 'painter', is_parallel: false, timeout: 90)
endif

# Every native GimpPrecision/channel model with builtin and non-default ICC.
painter_precision_test = executable('painter-mypaint-precision',
  'test-painter-mypaint-precision.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('painter-mypaint-precision', painter_precision_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 180)

# Fixture producer for observing actual console Quit with live private workers.
filter_quit_fixture = executable('filter-quit-fixture',
  'test-filter-quit-fixture.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream],
  link_with: apptests_links,
)

editor_dropdown_test = executable('editor-dropdown', 'test-editor-dropdown.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('editor-dropdown', editor_dropdown_test,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

editor_dropdown_preset_control = executable('editor-dropdown-preset-control',
  'test-editor-dropdown-preset-control.c', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  override_options: ['c_std=c11'])
test('editor-dropdown-preset-control', editor_dropdown_preset_control,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Focused owning-route and C-entry admission controls for popup renderers.
painter_popup_handles_test = executable('painter-popup-handles',
  'test-painter-popup-handles.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('painter-popup-handles', painter_popup_handles_test,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Original FilterLayer queries, loaded-cache routes and checked Blinds preparation.
filter_entry_args = []
filter_entry_link_args = []
filter_entry_wraps = ['-Wl,--wrap=gimp_filter_layer_new',
                      '-Wl,--wrap=gimp_filter_layer_get_procedure',
                      '-Wl,--wrap=gimp_filter_layer_get_procedure_args',
                      '-Wl,--wrap=gimp_filter_layer_mark_as_loaded',
                      '-Wl,--wrap=gimp_filter_layer_set_legacy_procedure']
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_multi_link_arguments(filter_entry_wraps)
  filter_entry_args = ['-DGIMP_TEST_FILTER_WRAP_ENTRIES']
  filter_entry_link_args = filter_entry_wraps
endif
filter_entry_contracts_test = executable('filter-entry-contracts',
  'test-filter-entry-contracts.c', 'test-filter-entry-contracts.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args + filter_entry_args, link_args: filter_entry_link_args,
  override_options: ['c_std=c11', 'cpp_std=c++14'])
test('filter-entry-contracts', python,
  args: [meson.project_source_root() / 'migration/tests/run_small_tiles_entry_fixture_test.py',
         '--', filter_entry_contracts_test],
  depends: gimp_test_icon_dependencies + [filter_entry_contracts_test],
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Original deferred LayerTileView construction and native canvas routing.
layer_tile_entry_args = []
layer_tile_entry_link_args = []
layer_tile_entry_wraps = ['-Wl,--wrap=gimp_layer_tile_view_new',
                          '-Wl,--wrap=gimp_layer_tile_view_get_type']
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_multi_link_arguments(layer_tile_entry_wraps)
  layer_tile_entry_args = ['-DGIMP_TEST_LAYER_TILE_WRAP_ENTRIES']
  layer_tile_entry_link_args = layer_tile_entry_wraps
endif
layer_tile_entry_contracts_test = executable('layer-tile-entry-contracts',
  'test-layer-tile-entry-contracts.c', 'test-layer-tile-entry-contracts.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream, json_glib], link_with: apptests_links,
  cpp_args: painter_cpp_args + layer_tile_entry_args, link_args: layer_tile_entry_link_args,
  override_options: ['c_std=c11', 'cpp_std=c++14'])
test('layer-tile-entry-contracts', layer_tile_entry_contracts_test,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)
if painter_http_enabled
  application_owned_lifecycle = executable('application-owned-lifecycle',
    'test-application-owned-lifecycle.cpp',
    'test-application-owned-lifecycle-glue.c',
    dependencies: [libapp_dep, appstream, json_glib, painter_http_soup],
    link_with: [apptests_links, libapphttpdgui], cpp_args: painter_cpp_args,
    override_options: ['c_std=c11', 'cpp_std=c++14'])
  test('application-owned-lifecycle', application_owned_lifecycle,
    depends: gimp_test_icon_dependencies,
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes', 'GSETTINGS_BACKEND=memory'],
    suite: 'painter', is_parallel: false, timeout: 180)
endif

# Deferred device discovery must not overwrite an unread profile on early exit.
device_save_lifecycle = executable('device-save-lifecycle',
  'test-device-save-lifecycle.c', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, override_options: ['c_std=c11'])
test('device-save-lifecycle', device_save_lifecycle,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'GIMP3_DATADIR=' + meson.project_build_root() / 'data',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Welcome can be created directly at startup, outside the dialog factory.
welcome_shutdown_links = apptests_links
if painter_http_enabled
  welcome_shutdown_links += libapphttpdgui
endif
welcome_shutdown = executable('welcome-shutdown',
  'test-welcome-shutdown.c', 'test-application-owned-lifecycle-glue.c',
  dependencies: [libapp_dep, appstream], link_with: welcome_shutdown_links,
  override_options: ['c_std=c11'])
test('welcome-shutdown', welcome_shutdown,
  depends: gimp_test_icon_dependencies,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes', 'GSETTINGS_BACKEND=memory'],
  suite: 'app', is_parallel: false, timeout: 180)

# Bounded sparse-canvas Surface transfers; not allocation/RSS measurement.
painter_roi_transfers = executable('painter-roi-transfers',
  'test-painter-roi-transfers.cpp', 'tests.c',
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('painter-roi-transfers', painter_roi_transfers,
  env: [
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
    'UI_TEST=yes',
  ], suite: 'app', is_parallel: false, timeout: 60)

# Real C caller / C++ initializer failure seams; never present in the app link.
painter_layer_init_wrap = [
  '-Wl,--wrap=_ZN11GimpPainter12BindingStore6ensureEP8_GObject',
  '-Wl,--wrap=_ZN11GimpPainter12BindingStore8activateEv',
]
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_multi_link_arguments(painter_layer_init_wrap)
  painter_layer_initializer_test = executable('painter-layer-initializer',
    'test-painter-layer-initializer.c', 'test-painter-layer-initializer.cpp', 'tests.c',
    dependencies: [libapp_dep, appstream], link_with: apptests_links,
    cpp_args: painter_cpp_args, link_args: painter_layer_init_wrap,
    override_options: ['c_std=c11', 'cpp_std=c++14'])
  test('painter-layer-initializer', painter_layer_initializer_test,
    depends: gimp_test_icon_dependencies,
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
      'UI_TEST=yes',
    ], suite: 'app', is_parallel: false, timeout: 60)
endif

# Actual C TempBuf/pixbuf bridge with a GEGL Surface consumer; no replacement model.
painter_tempbuf_test = executable('painter-tempbuf',
  'test-painter-tempbuf.cpp', '../core/gimptempbuf.c', '../../libgimpcolor/gimppixbuf.c',
  dependencies: [painter_mypaint_surface_dep, gdk_pixbuf],
  include_directories: [rootInclude, rootAppInclude],
  c_args: ['-DGIMP_COLOR_COMPILATION'], cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('painter-tempbuf', painter_tempbuf_test,
  env: ['GEGL_USE_OPENCL=no'], suite: 'app', is_parallel: false, timeout: 60)

# Real C caller / C++ signal dispatch continuation seam; never present in the app link.
painter_layer_dispatch_wrap = [
  '-Wl,--wrap=g_weak_ref_get',
]
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_multi_link_arguments(painter_layer_dispatch_wrap)
  painter_layer_dispatch_test = executable('painter-layer-dispatch',
    'test-painter-layer-dispatch.c', 'test-painter-layer-dispatch.cpp', 'tests.c',
    dependencies: [libapp_dep, appstream], link_with: apptests_links,
    cpp_args: painter_cpp_args, link_args: painter_layer_dispatch_wrap,
    override_options: ['c_std=c11', 'cpp_std=c++14'])
  test('painter-layer-dispatch', painter_layer_dispatch_test,
    depends: gimp_test_icon_dependencies,
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
      'UI_TEST=yes',
    ], suite: 'app', is_parallel: false, timeout: 60)
endif

# Checked nonfloating canvas factory, real C callers and C++ fault seams.
painter_canvas_factory_wrap = [
  '-Wl,--wrap=_ZN11GimpPainter12BindingStore6ensureEP8_GObject',
  '-Wl,--wrap=_ZN11GimpPainter12BindingStore8activateEv',
  '-Wl,--wrap=g_signal_parse_name',
  '-Wl,--wrap=g_signal_connect_data',
]
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_multi_link_arguments(painter_canvas_factory_wrap)
  painter_canvas_factory_test = executable('painter-canvas-factory',
    'test-painter-canvas-factory.c', 'test-painter-canvas-factory.cpp', 'tests.c',
    dependencies: [libapp_dep, appstream], link_with: apptests_links,
    cpp_args: painter_cpp_args, link_args: painter_canvas_factory_wrap,
    override_options: ['c_std=c11', 'cpp_std=c++14'])
  test('painter-canvas-factory', painter_canvas_factory_test,
    depends: gimp_test_icon_dependencies,
    env: [
      'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
      'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
      'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
      'UI_TEST=yes',
    ], suite: 'app', is_parallel: false, timeout: 60)
endif

# Native missing-icon paths; wrappers are confined to this test executable.
painter_icon_fallback_wrap = ['-Wl,--wrap=gtk_icon_theme_load_icon',
  '-Wl,--wrap=gimp_cairo_surface_create_from_pixbuf',
  '-Wl,--wrap=gimp_viewable_get_icon_name']
if host_machine.system() == 'linux' and cxx.get_id() in ['gcc', 'clang'] and cxx.has_multi_link_arguments(painter_icon_fallback_wrap)
  painter_icon_fallback_test = executable('painter-icon-fallback',
    'test-painter-icon-fallback.c', 'tests.c',
    dependencies: [libapp_dep, appstream], link_with: apptests_links,
    link_args: painter_icon_fallback_wrap, override_options: ['c_std=c11'])
  test('painter-icon-fallback', painter_icon_fallback_test,
    depends: gimp_test_icon_dependencies,
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)
endif

# Generic JSON resource C entries and the shared native preset owner.
json_resource_test = executable('gimp-json-resource',
  'test-gimp-json-resource.c', 'test-gimp-json-resource.cpp', 'tests.c',
  dependencies: [libapp_dep, json_glib, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('gimp-json-resource', json_resource_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)

# Original preset GUI C routes, distinct from native layer-preset actions.
preset_gui_entries_test = executable('preset-gui-entries',
  'test-preset-gui-entries.c', 'test-preset-gui-entries.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('preset-gui-entries', preset_gui_entries_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)

# Original BucketFillBrush C entry names share the sole native FillBrush provider.
bucket_fill_entry_test = executable('bucket-fill-brush-entry',
  'test-bucket-fill-brush-entry.c', 'test-bucket-fill-brush-entry.cpp', 'tests.c',
  link_args: ['-Wl,--wrap=gimp_bucket_fill_brush_tool_register'],
  dependencies: [libapp_dep, painter_bounded_fill_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('bucket-fill-brush-entry', bucket_fill_entry_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)

# Native one-owner feature registration/startup/close boundaries.
if painter_http_enabled
  feature_lifecycle_test = executable('feature-lifecycle',
    'test-feature-lifecycle.c', 'test-feature-lifecycle.cpp', 'tests.c',
    dependencies: [libapp_dep, appstream], link_with: apptests_links,
    link_args: ['-Wl,--wrap=gimp_painter_httpd_from_environment'],
    cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
  test('feature-lifecycle', feature_lifecycle_test,
    env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
          'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
          'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
          'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)
endif

# Original consuming public paint-info registration, shared with native startup.
paint_registration_test = executable('paint-registration',
  'test-paint-registration.c', 'test-paint-registration.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('paint-registration', paint_registration_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Independent original popup-window C factory; no cell-renderer/decorator path.
native_popover_entry_test = executable('native-popover-entry',
  'test-native-popover-entry.c', 'test-native-popover-entry.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  link_args: ['-Wl,--wrap=_ZN11GimpPainter12BindingStore6ensureEP8_GObject',
              '-Wl,--wrap=_ZN11GimpPainter12BindingStore8activateEv'],
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('native-popover-entry', native_popover_entry_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 60)

# Per-mutation chain witnesses and coherent reentrant GListModel publication.
container_reentry_test = executable('container-reentry',
  'test-container-reentry.c', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  override_options: ['c_std=c11'])
test('container-reentry', container_reentry_test, suite: 'app',
  is_parallel: false, timeout: 120)

# Prepared typed Undo publication before fallible controller/pixel commit.
prepared_undo_test = executable('prepared-undo',
  'test-prepared-undo.c', 'test-prepared-undo.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('prepared-undo', prepared_undo_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)


# Retained unique-type tool options retire without adopting a fallback tool.
tool_options_retirement_test = executable('tool-options-retirement',
  'test-tool-options-retirement.c', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('tool-options-retirement', tool_options_retirement_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Original consuming public tool-info registration, shared with native startup.
tool_registration_test = executable('tool-registration',
  'test-tool-registration.c', 'test-tool-registration.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('tool-registration', tool_registration_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Optional guarded native Undo preflight before stack movement.
undo_pop_guard = executable('undo-pop-guard',
  ['test-undo-pop-guard.c', 'tests.c'],
  include_directories: [ rootInclude, rootAppInclude ],
  dependencies: [ libapp_dep, appstream ],
  link_with: apptests_links,
  c_args: ['-DG_LOG_DOMAIN="Gimp-App-Test"'],
  override_options: ['c_std=c11'],
)
test('undo-pop-guard', undo_pop_guard, suite: 'app', timeout: 120)

# Prepare-only native pixel rollback payload; no history publication.
prepared_pixels_test = executable('painter-prepared-pixels',
  'test-painter-prepared-pixels.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, link_args: ['-Wl,--wrap=gegl_buffer_new', '-Wl,--wrap=_Znwm'],
  override_options: ['c_std=c11', 'cpp_std=c++14'])
test('painter-prepared-pixels', prepared_pixels_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Typed paired native Undo payload lifecycle; controller integration is separate.
stroke_undo_test = executable('painter-stroke-undo',
  'test-painter-stroke-undo.cpp', 'tests.c',
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['c_std=c11', 'cpp_std=c++14'])
test('painter-stroke-undo', stroke_undo_test,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Original options type/constructor entries, sole native model and owned result.
mypaint_options_construction = executable('mypaint-options-construction',
  ['test-mypaint-options-construction.cpp', 'test-mypaint-options-construction-c.c', 'tests.c'],
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  link_args: ['-Wl,--wrap=_ZN11GimpPainter12BindingStore6ensureEP8_GObject'],
  override_options: ['cpp_std=c++14'])
test('mypaint-options-construction', mypaint_options_construction,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Causal endpoint acquisition and native-construction callback reentry.
mypaint_options_construction_reentry = executable('mypaint-options-construction-reentry',
  ['test-mypaint-options-construction-reentry.cpp', 'tests.c'],
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['cpp_std=c++14'])
test('mypaint-options-construction-reentry', mypaint_options_construction_reentry,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Paired historical evaluator/resource and native pixel Undo integration.
paired_stroke_undo = executable('paired-stroke-undo',
  ['test-paired-stroke-undo.cpp', 'tests.c'],
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  link_args: ['-Wl,--wrap=_Znwm'], override_options: ['cpp_std=c++14'],
)
test('paired-stroke-undo', paired_stroke_undo,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Actual rendered paired Undo/Redo evidence, using direct controller API.
paired_stroke_ui = executable('paired-stroke-ui',
  ['test-paired-stroke-ui.cpp', 'tests.c'],
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  link_args: ['-Wl,--wrap=_Znwm'], override_options: ['cpp_std=c++14'])
test('paired-stroke-ui', paired_stroke_ui,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Real source-node invalidation and projection tile compute observations.
graph_invalidation_test = executable('painter-graph-invalidation',
 'test-painter-graph-invalidation.cpp', 'tests.c',
 dependencies: [libapp_dep, appstream], link_with: apptests_links,
 cpp_args: painter_cpp_args,
 link_args: ['-Wl,--wrap=gegl_node_blit', '-Wl,--wrap=gegl_node_blit_buffer'],
 override_options: ['c_std=c11', 'cpp_std=c++14'])
test('painter-graph-invalidation', graph_invalidation_test,
 env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
       'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
       'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
       'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Original native MyPaint Undo opaque name and typed ownership facade.
mypaint_undo_ref = executable('mypaint-undo-ref',
  ['test-mypaint-undo-ref.cpp', 'tests.c'],
  dependencies: [libapp_dep, appstream], link_with: apptests_links,
  cpp_args: painter_cpp_args, override_options: ['cpp_std=c++14'])
test('mypaint-undo-ref', mypaint_undo_ref,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

# Original MyPaint GUI entries share the sole native editor/model.
mypaint_options_gui = executable('mypaint-options-gui',
  ['test-mypaint-options-gui.cpp', 'test-mypaint-options-gui-c.c', 'tests.c'],
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['c_std=c11', 'cpp_std=c++14'])
test('mypaint-options-gui', mypaint_options_gui,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 180)

# Original constant MyPaint rendering-mode helper, not stored mode selection.
mypaint_options_mode = executable('mypaint-options-mode',
  ['test-mypaint-options-mode.cpp', 'test-mypaint-options-mode-c.c', 'tests.c'],
  dependencies: [libapp_dep, painter_mypaint_surface_dep, appstream],
  link_with: apptests_links, cpp_args: painter_cpp_args,
  override_options: ['c_std=c11', 'cpp_std=c++14'])
test('mypaint-options-mode', mypaint_options_mode,
  env: ['GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
        'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
        'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
        'UI_TEST=yes'], suite: 'app', is_parallel: false, timeout: 120)

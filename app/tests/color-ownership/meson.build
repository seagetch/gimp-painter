# Separate supplementary tests; do not alter the protected common test bundle.
# Current-color lifetime and public renderer state are not private-color leak proof.
display_renderer_ownership = executable('display-renderer-ownership',
  'test-display-renderer.c',
  dependencies: [libapp_dep, appstream],
  link_with: apptests_links,
  override_options: ['c_std=c11'])
test('display-renderer-ownership', display_renderer_ownership,
  suite: 'painter', is_parallel: false, timeout: 120)

canvas_ownership = executable('canvas-ownership',
  'test-canvas.c',
  dependencies: [libapp_dep, appstream],
  link_with: apptests_links,
  override_options: ['c_std=c11'])
test('canvas-ownership', canvas_ownership,
  suite: 'painter', is_parallel: false, timeout: 120)

# Independent assertion control: exact historical implementation, same test.
# The direct object precedes static app archives; hosted map verification must
# establish it supplies every canvas API and no fixed canvas member is pulled.
canvas_old_provider = executable('canvas-ownership-old-provider',
  ['test-canvas.c', 'fixtures/gimpcanvas-c9.c'],
  include_directories: include_directories('../../display'),
  dependencies: [libapp_dep, appstream],
  link_with: apptests_links,
  link_args: ['-Wl,--cref,-Map,' + meson.current_build_dir() / 'canvas-ownership-old-provider.map'],
  override_options: ['c_std=c11'])
test('canvas-ownership-old-provider', canvas_old_provider,
  suite: 'painter-negative-control', is_parallel: false, timeout: 120,
  should_fail: true)

# Bounded mask-window provider proof; keep the protected107-case registration.
# Candidate and historical controls compile the SAME complete regression TU.
# GNU link maps are Linux-only provenance; no linker-symbol substitution.
if host_machine.system() == 'linux'
  mask_proof_env = [
    # tests.c resolves the shared icon fixture via G_TEST_BUILT. These
    # executables live one directory below its app/tests fixture root.
    'G_TEST_BUILDDIR=' + meson.project_build_root()/'app'/'tests',
    'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
    'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
    'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root()/'plug-ins'/'common',
    'GIMP_TEST_PAINTER_CANVAS_GROUP=original',
    'GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas',
    'UI_TEST=yes',
  ]
  canvas_mask_fixed = executable('canvas-mask-fixed-provider',
    ['../test-painter-canvas-ui.c', '../tests.c'],
    dependencies: [libapp_dep, appstream],
    link_with: apptests_links,
    link_args: ['-Wl,--cref,-Map,' + meson.current_build_dir() / 'canvas-mask-fixed-provider.map'])
  test('canvas-mask-fixed-provider', canvas_mask_fixed,
    env: mask_proof_env, suite: 'painter-mask-proof',
    depends: gimp_test_icon_dependencies, timeout: 180, is_parallel: false)

  if get_option('b_sanitize') == 'none'
    canvas_mask_old = executable('canvas-mask-old-provider',
      ['../test-painter-canvas-ui.c', '../tests.c',
       'fixtures/gimppainterlayertiles-e25.cpp'],
      include_directories: include_directories('../../widgets'),
      dependencies: [libapp_dep, appstream, painter_mypaint_surface_dep],
      link_with: apptests_links,
      cpp_args: painter_cpp_args,
      override_options: ['cpp_std=c++14'],
      link_args: ['-Wl,--cref,-Map,' + meson.current_build_dir() / 'canvas-mask-old-provider.map'])
    test('canvas-mask-old-provider', canvas_mask_old,
      env: mask_proof_env, suite: 'painter-mask-negative-control',
      depends: gimp_test_icon_dependencies, timeout: 180,
      is_parallel: false, should_fail: true)
  endif
endif

# XXX: we have a bunch of (manually run?) tests inside libgimp/test/.
# These should either be deleted or transformed into real unit tests.

if not meson.can_run_host_binaries()
  warning('libgimp unit testing disabled in cross-building or similar environments.')
  subdir_done()
endif

# The fixture uses the native GimpPlugIn lifecycle and real wire protocol.
# POSIX auxiliary pipes make the close-after-return race deterministic without
# changing the normal GIMP host, library timing, or shutdown behavior.
if host_machine.system() != 'windows'
  plug_in_protocol_fixture = executable('plug-in-protocol-fixture',
    'plug-in-protocol-fixture.c',
    dependencies: [ libgimp_dep, pango ],
    install: false)
  plug_in_protocol_test = executable('test-plug-in-protocol',
    'test-plug-in-protocol.c',
    dependencies: [ libgimp_dep, pango ],
    install: false)
  plug_in_protocol_env = gimp_run_env
  plug_in_protocol_env.set('GIMP_TEST_PROTOCOL_FIXTURE',
                          plug_in_protocol_fixture.full_path())
  plug_in_protocol_env.unset('GIMP_TEST_EXPECT_REDUNDANT_QUIT')
  test('plug-in-protocol', plug_in_protocol_test,
    env: plug_in_protocol_env,
    depends: plug_in_protocol_fixture,
    suite: ['libgimp', 'C'],
    timeout: 180)
endif

# Config creation only needs libgimp; no GIMP process or display is required.
procedure_config_test = executable('test-procedure-config',
  'test-procedure-config.c',
  dependencies: [ libgimp_dep, pango ],
  install: false)
test('procedure-config', procedure_config_test,
  env: gimp_run_env,
  suite: ['libgimp', 'C'],
  timeout: 30)

tests = [
  'color-parser',
  'export-options',
  'image',
  'palette',
  'selection-float',
  'unit',
]

# Unit testing environment is based on gimp_run_env with additional environment
# variables and added temporary test plug-ins. Assignment is a deep copy, so
# test_env here is a new object.
# See: https://github.com/mesonbuild/meson/issues/13045
test_env=gimp_run_env
test_env.append('GIMP_TESTING_PLUGINDIRS', meson.project_build_root() / 'libgimp/tests/c-tests/')
test_env.set('GIMP_TESTING_ABS_TOP_SRCDIR', meson.project_source_root())

run_python_test = find_program('./libgimp-run-python-test.py')
run_c_test      = find_program('./libgimp-run-c-test.py')
foreach test_name : tests
  basename = 'test-' + test_name

  py_test  = meson.current_source_dir() / basename + '.py'
  test(test_name, run_python_test,
       args: [ gimp_exe.full_path(), py_test ],
       env: test_env,
       suite: ['libgimp', 'python3'],
       timeout: 90)

  c_test_name  = basename + '.c'
  c_test = custom_target(c_test_name,
                         input: [ 'c-test-header.c', c_test_name ],
                         output: c_test_name,
                         command: [python, '-c', 'import sys; [sys.stdout.write(open(f).read()) for f in sys.argv[1:]]','@INPUT@'],
                         capture: true,
                         install: false)
  c_test_exe = executable(basename,
                          c_test,
                          dependencies: [ libgimp_dep, pango ],
                          install: false)

  # Same ugly trick as in plug-ins/common/meson.build to detect plug-ins in a
  # non-installed build directory.
  custom_target(basename + '.dummy',
                input: [ c_test_exe ],
                output: [ basename + '.dummy' ],
                command: [ python, meson.project_source_root() / 'tools/cp-plug-in-subfolder.py',
                           c_test_exe, meson.current_build_dir() / 'c-tests' / basename,
                           '@OUTPUT@' ],
                build_by_default: true,
                install: false)
  plugin_executables += [meson.current_build_dir() / 'c-tests' / basename / fs.name(c_test_exe.full_path())]

  test(test_name, run_c_test,
       args: [ gimp_exe.full_path(), meson.current_source_dir() / c_test_name, basename ],
       env: test_env,
       suite: ['libgimp', 'C'],
       timeout: 90)
endforeach

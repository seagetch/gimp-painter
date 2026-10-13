# The parent gimp-data recipes export their installed asset groups. Mirror the
# current vector install layout here without modifying that pinned submodule.
# Bitmap production custom targets have no exported dependency collection, so
# this first fixture deliberately leaves vector-icons=false builds unchanged.
gimp_test_icon_dependencies = []
icon_fixture_generator = files('generate.py')
test('gimp-test-icon-theme-generator', python,
  args: ['-B', files('test-generate.py')], suite: ['app', 'test-fixtures'])

if have_vector_icons
  icon_fixture_root = meson.project_source_root()
  icon_fixture_plan = ['mode\tvector']
  icon_fixture_inputs = []
  icon_fixture_recipe_files = [
    'gimp-data/icons/meson.build',
    'gimp-data/icons/Default/meson.build',
    'gimp-data/icons/Legacy/meson.build',
    'gimp-data/icons/hicolor/meson.build',
    'app/tests/icon-theme-fixture/meson.build',
    'app/tests/icon-theme-fixture/generate.py',
  ]
  foreach recipe : icon_fixture_recipe_files
    # fs.read registers a configure dependency, including list membership.
    fs.read(icon_fixture_root / recipe)
    icon_fixture_plan += ['input\t' + recipe]
    icon_fixture_inputs += files(icon_fixture_root / recipe)
  endforeach

  foreach theme : ['Default', 'Legacy', 'hicolor']
    src = 'gimp-data/icons' / theme / 'index.theme'
    icon_fixture_plan += ['file\t' + src + '\t' + theme / 'index.theme']
    icon_fixture_inputs += files(icon_fixture_root / src)
  endforeach

  # install_icons and rtl_icons are the actual pinned Default production recipe.
  foreach install : install_icons
    names = []
    foreach list_name : install['files']
      list_path = 'gimp-data/icons/icon-lists' / (list_name + '.list')
      contents = fs.read(icon_fixture_root / list_path)
      icon_fixture_plan += ['input\t' + list_path]
      icon_fixture_inputs += files(icon_fixture_root / list_path)
      foreach line : contents.split('\n')
        name = line.strip()
        if name != '' and not name.startswith('#') and not names.contains(name)
          names += [name]
        endif
      endforeach
    endforeach
    foreach suffix : install['suffixes']
      foreach name : names
        src = 'gimp-data/icons/Default' / install['indir'] / (name + suffix)
        dst = 'Default' / install['outdir'] / 'apps' / (name + suffix)
        if name == 'gimp-wilber' and suffix == '.svg'
          src = 'gimp-data/images/logo/gimp-logo-shadow.svg'
          dst = 'Default' / install['outdir'] / 'logo/unisize/gimp-wilber.svg'
        elif name == 'gimp-wilber' and suffix == '-symbolic.svg'
          src = 'gimp-data/images/logo/gimp-wilber-symbolic.svg'
          dst = 'Default' / install['outdir'] / 'logo/unisize/gimp-wilber-symbolic.svg'
        endif
        icon_fixture_plan += ['file\t' + src + '\t' + dst]
        icon_fixture_inputs += files(icon_fixture_root / src)
        if name == 'gimp-wilber-eek'
          icon_fixture_plan += ['file\t' + src + '\tDefault' / install['outdir'] / 'apps' / ('image-missing' + suffix)]
        endif
      endforeach
      if suffix == '.svg'
        foreach rtl : rtl_icons
          foreach index : [0, 1]
            foreach symbolic : ['', '-symbolic']
              src = 'gimp-data/icons/Default' / install['indir'] / (rtl[index] + symbolic + '.svg')
              dst = 'Default' / install['outdir'] / 'apps' / (rtl[1 - index] + symbolic + '-rtl.svg')
              icon_fixture_plan += ['file\t' + src + '\t' + dst]
              icon_fixture_inputs += files(icon_fixture_root / src)
            endforeach
          endforeach
        endforeach
      endif
    endforeach
  endforeach

  # These are the exact groups already evaluated by the pinned Legacy recipe.
  foreach group : all_legacy_icons + all_legacy_tools
    foreach icon : group[1]
      src = 'gimp-data/icons/Legacy' / icon
      dst = 'Legacy' / group[0] / 'apps' / fs.name(icon)
      icon_fixture_plan += ['file\t' + src + '\t' + dst]
      icon_fixture_inputs += files(icon_fixture_root / src)
    endforeach
  endforeach
  foreach group : generic_icons
    src = 'gimp-data/icons/hicolor' / group[1]
    dst = 'hicolor' / group[0] / fs.name(group[1])
    icon_fixture_plan += ['file\t' + src + '\t' + dst]
    icon_fixture_inputs += files(icon_fixture_root / src)
  endforeach

  icon_fixture_plan_file = configure_file(
    output: 'gimp-test-icon-theme-plan.tsv',
    command: [python, '-c', 'import sys; sys.stdout.write(sys.argv[1])',
              '\n'.join(icon_fixture_plan) + '\n'], capture: true)
  gimp_test_icon_fixture = custom_target('gimp-test-icon-theme-fixture',
    input: icon_fixture_plan_file,
    output: 'gimp-test-icon-theme-ready.h',
    command: [python, '-B', icon_fixture_generator,
      '--source-root', icon_fixture_root, '--plan', '@INPUT@',
      '--output', gimp_test_icon_output_dir, '--stamp', '@OUTPUT@'],
    depend_files: icon_fixture_inputs,
    build_by_default: true,
    # Reconstruct deleted copied files and validate bytes on ordinary rebuilds.
    # The generator preserves the ready header's mtime when bytes are unchanged.
    build_always_stale: true)
  gimp_test_icon_dependencies = [gimp_test_icon_fixture]
endif

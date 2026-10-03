#!/usr/bin/env python3
"""Run the current build with an isolated, disposable GIMP profile."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


def has_config_override(arguments):
    """Recognize config options without interpreting batch text as options."""
    value_options = {
        '--batch', '--batch-interpreter', '--session', '--pdb-compat-mode',
        '--stack-trace-mode', '--display', '--name', '--class',
    }
    arguments = iter(arguments)
    for argument in arguments:
        if argument == '--':
            break
        option = argument.split('=', 1)[0]
        if option in ('--gimprc', '--system-gimprc'):
            return True
        if option in value_options:
            if '=' not in argument:
                next(arguments, None)
        elif argument.startswith('-') and not argument.startswith('--'):
            for index, short_option in enumerate(argument[1:], 1):
                if short_option == 'g':
                    return True
                if short_option == 'b':
                    if index == len(argument) - 1:
                        next(arguments, None)
                    break
    return False


def stage_script_fu(source_root, build_root, profile):
    """Copy exactly Meson's installed extension and initialization scripts.

    Source tests and standalone interpreter plug-ins are not extension scripts.
    Using the install map keeps this list authoritative without recursively
    scanning the source tree or loading stale scripts from an installed GIMP.
    """
    source_scripts = (source_root / 'plug-ins/script-fu/scripts').resolve()
    with (build_root / 'meson-info/intro-installed.json').open() as stream:
        installed = json.load(stream)
    staged = []
    for source, destination in sorted(installed.items()):
        source = Path(source)
        destination = Path(destination)
        if source.suffix != '.scm':
            continue
        if (source.parent == source_scripts and
                destination.parent.name == 'scripts'):
            relative = Path(source.name)
        elif (source.parent == source_scripts / 'init' and
              destination.parent.name == 'scriptfu-init'):
            relative = Path('scriptfu-init') / source.name
        else:
            continue
        target = profile / 'scripts' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        staged.append(relative.as_posix())
    if 'scriptfu-init/init.scm' not in staged:
        raise RuntimeError('Meson install map contains no Script-Fu init.scm')
    return staged


def main(arguments):
    build_root = Path(os.environ.get('GIMP_GLOBAL_BUILD_ROOT', '.')).resolve()
    source_root = Path(os.environ['GIMP_GLOBAL_SOURCE_ROOT']).resolve()
    environment = os.environ.copy()
    added_rpaths = []
    rpath_array = [str(build_root / library) for library in (
        'libgimp', 'libgimpbase', 'libgimpcolor', 'libgimpconfig',
        'libgimpmath', 'libgimpmodule', 'libgimpthumb', 'libgimpwidgets',
        'plug-ins/script-fu/libscriptfu',
    )]

    # TemporaryDirectory creates a private directory and removes it even when
    # the child fails. Neither the caller's profile nor config files are edited.
    with tempfile.TemporaryDirectory(prefix='.GIMP3-build-config-',
                                     dir=build_root) as temporary:
        profile = Path(temporary)
        environment['GIMP3_DIRECTORY'] = str(profile)
        print(f'INFO: temporary GIMP configuration directory: {profile}', flush=True)
        stage_script_fu(source_root, build_root, profile)
        if not has_config_override(arguments):
            (profile / 'gimprc').write_text(
                '(script-fu-path "${gimp_dir}/scripts")\n', encoding='utf-8')

        try:
            # macOS SIP prevents DYLD_LIBRARY_PATH from selecting these libs.
            # Restore only the rpaths that this invocation actually added.
            for binary in environment.get('GIMP_TEMP_UPDATE_RPATH', '').split(':'):
                if not binary:
                    continue
                result = subprocess.run(['otool', '-l', binary],
                                        stdout=subprocess.PIPE, check=True)
                existing = re.findall(r'path (.+?) \(offset',
                                      result.stdout.decode('utf-8', errors='replace'))
                for new_rpath in rpath_array:
                    if new_rpath not in existing:
                        subprocess.run(['install_name_tool', '-add_rpath',
                                        new_rpath, binary], check=True)
                        added_rpaths.append((binary, new_rpath))

            # Ensure plug-ins use Meson's Python when the default lacks GI.
            configured_python = environment.get('GIMP_PYTHON_WITH_GI', sys.executable)
            default_python = shutil.which('python3')
            needs_python = default_python is None
            if default_python and not os.path.samefile(default_python, configured_python):
                probe = subprocess.run([
                    default_python, '-c',
                    "import sys, gi; sys.exit(gi.check_version('3.0'))",
                ], check=False)
                needs_python = probe.returncode != 0
            if needs_python:
                python_directory = profile / 'tmp_python'
                python_directory.mkdir()
                (python_directory / 'python3').symlink_to(configured_python)
                environment['PATH'] = (str(python_directory) + os.pathsep +
                                       environment.get('PATH', ''))

            command = [environment['GIMP_SELF_IN_BUILD'], *arguments]
            if 'GIMP_DEBUG_SELF' in environment and shutil.which('gdb'):
                command = [
                    'gdb', '--return-child-result', '--batch', '-x',
                    str(source_root / 'tools/debug-in-build-gimp.py'),
                    '--args', *command,
                ]
            print('RUNNING: ' + ' '.join(command), flush=True)
            result = subprocess.run(command, stdin=sys.stdin, env=environment,
                                    check=False)
            return result.returncode if result.returncode >= 0 else 128 - result.returncode
        finally:
            for binary, new_rpath in reversed(added_rpaths):
                subprocess.run(['install_name_tool', '-delete_rpath',
                                new_rpath, binary], check=True)


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except (OSError, RuntimeError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Error: {error}', file=sys.stderr)
        sys.exit(1)

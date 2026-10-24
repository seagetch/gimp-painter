#!/usr/bin/env python3
"""Label immediate *.png previews with the original ImageMagick operation order.

This developer utility overwrites selected previews, not .myb files. Run on a
working copy: bundled assets are immutable until explicitly regenerated and
their asset lock reviewed. ImageMagick and its original 'fixed' font are used;
font/version differences are not claimed pixel-compatible across installations.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def caption(name):
    # The original Bash substitutions replace the first occurrence, not only a
    # suffix, and replace every underscore. Non-preview .png files are included.
    return name.replace('_prev.png', '', 1).replace('_', '-')


def command(convert, source, destination):
    return [convert, str(source), '-fill', 'black', '-font', 'fixed',
            '-gravity', 'South', '-background', '#FFF8', '-size', '124x26',
            'caption:' + caption(source.name), '-composite',
            '-bordercolor', '#CCC9', '-border', '2',
            '-bordercolor', '#9999', '-border', '1',
            '-resize', '128x128', '-unsharp', '0x.5', '-flatten', '+matte',
            str(destination)]


def label(directory, convert='convert'):
    directory = Path(directory).resolve(strict=True)
    if not directory.is_dir():
        raise ValueError('expected preview directory')
    sources = sorted(p for p in directory.glob('*.png') if not p.name.startswith('.'))
    for source in sources:
        if source.is_symlink() or not source.is_file():
            raise ValueError('preview must be a regular non-symlink file: ' + str(source))
    for source in sources:
        # Do not truncate an original if conversion fails. Each successful
        # output replaces the same basename; no directory recursion is added.
        fd, temporary = tempfile.mkstemp(prefix='.painter-label-', suffix='.png', dir=directory)
        os.close(fd)
        temporary = Path(temporary)
        try:
            subprocess.run(command(convert, source, temporary), check=True)
            if temporary.stat().st_size == 0:
                raise RuntimeError('ImageMagick produced no preview: ' + str(source))
            shutil.copymode(source, temporary)
            temporary.replace(source)
        finally:
            temporary.unlink(missing_ok=True)
    return len(sources)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path, nargs='?', default=Path.cwd())
    args = parser.parse_args()
    print('Labeled %d previews' % label(args.directory))


if __name__ == '__main__':
    main()

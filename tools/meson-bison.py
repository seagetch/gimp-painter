#!/usr/bin/env python3
"""Run Bison unchanged, then give its complete header C linkage in C++.

Bison emits debug declarations before %code requires, so grammar code blocks
cannot wrap the whole interface.  Keep the checked-in imagemap fallback headers
in sync with this boundary; the parser implementation is left untouched.
"""

from pathlib import Path
import os
import re
import signal
import subprocess
import sys


def add_c_linkage(text):
    guard = re.search(r'(?m)^#ifndef (\w+)\n# define \1\n', text)
    if guard is None:
        raise ValueError('Bison header has no recognized include guard')

    closing = re.search(r'(?m)^#endif /\* !' + re.escape(guard[1]) +
                        r'\s+\*/\s*\Z', text)
    if closing is None:
        raise ValueError('Bison header has no matching final include guard')

    opening = '\n#ifdef __cplusplus\nextern "C" {\n#endif\n\n'
    ending = '#ifdef __cplusplus\n}\n#endif\n\n'
    return (text[:guard.end()] + opening + text[guard.end():closing.start()] +
            ending + text[closing.start():])


def main():
    if len(sys.argv) < 3:
        print('usage: meson-bison.py HEADER BISON [ARGUMENTS...]', file=sys.stderr)
        return 2

    result = subprocess.run(sys.argv[2:])
    if result.returncode < 0:
        signum = -result.returncode
        if signum not in (signal.SIGKILL, signal.SIGSTOP):
            signal.signal(signum, signal.SIG_DFL)
        os.kill(os.getpid(), signum)
    if result.returncode:
        return result.returncode

    header = Path(sys.argv[1])
    try:
        header.write_text(add_c_linkage(header.read_text(encoding='utf-8')),
                          encoding='utf-8')
    except (OSError, ValueError) as error:
        print(f'meson-bison.py: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())

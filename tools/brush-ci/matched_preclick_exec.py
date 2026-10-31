#!/usr/bin/env python3
"""Exact diagnostic leaf exec with one emitted-byte stream; no environment edits."""
import os
from pathlib import Path
import sys

LEAVES={'matched-preclick':'/matched-preclick/primary',
        'painter-compact-options':'/brush-thumbnail-native-popup/private-target-release'}

def validated(argv):
    if len(argv)!=3 or argv[1]!='-p':raise ValueError()
    target=Path(argv[0])
    if not target.is_absolute() or target.resolve(strict=True)!=target:raise ValueError()
    if LEAVES.get(target.name)!=argv[2] or not target.is_file() or not os.access(target,os.X_OK):raise ValueError()
    return str(target)

def main(argv=None):
    args=list(sys.argv[1:] if argv is None else argv)
    try:target=validated(args)
    except (OSError,ValueError):return 2
    os.dup2(1,2)
    os.execv(target,args)
    return 2
if __name__=='__main__':raise SystemExit(main())

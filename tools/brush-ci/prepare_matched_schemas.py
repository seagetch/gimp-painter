#!/usr/bin/env python3
"""Prepare only the owned extracted schema cache; export finite evidence only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

COMPILER = 'usr/lib/x86_64-linux-gnu/glib-2.0/glib-compile-schemas'
SCHEMAS = 'usr/share/glib-2.0/schemas'
LIMIT = 128 * 1024

def prepare(root, log, receipt):
    root, log, receipt = map(Path, (root, log, receipt))
    result = {'schema': 'matched-schema-preparation-v1', 'acceptance_eligible': False,
              'stage': 'preflight', 'compiler_exit': None, 'compiler_sha256': None,
              'cache_sha256': None, 'loader_error_observed': False,
              'raw_output_exported': False, 'loader_policy': 'dependency-libdir-only'}
    code = 1
    try:
        if not root.is_absolute() or root.is_symlink() or root.resolve() != root:
            result['stage'] = 'unsafe-root'
        else:
            compiler, schemas = root / COMPILER, root / SCHEMAS
            if (not compiler.is_file() or compiler.is_symlink() or
                    not compiler.resolve().is_relative_to(root) or not os.access(compiler, os.X_OK)):
                result['stage'] = 'missing-or-unsafe-compiler'
            elif (not schemas.is_dir() or schemas.is_symlink() or
                  schemas.resolve() != schemas):
                result['stage'] = 'missing-or-unsafe-schema-directory'
            elif (schemas / 'gschemas.compiled').is_symlink():
                result['stage'] = 'unsafe-output'
            else:
                result['compiler_sha256'] = hashlib.sha256(compiler.read_bytes()).hexdigest()
                env = dict(os.environ, LD_LIBRARY_PATH=str(root / 'usr/lib/x86_64-linux-gnu'))
                if env.get('LD_PRELOAD') or env.get('LD_AUDIT'):
                    result['stage'] = 'loader-injection-rejected'
                else:
                    result['stage'] = 'compiler'
                    try:
                        with log.open('wb') as output:
                            run = subprocess.run([str(compiler), '--strict', str(schemas)],
                                                 env=env, stdout=output, stderr=subprocess.STDOUT,
                                                 timeout=90, check=False)
                        result['compiler_exit'] = run.returncode
                        code = (128 - run.returncode if -127 <= run.returncode < 0 else
                                run.returncode if 0 < run.returncode < 256 else 1)
                        result['stage'] = 'compiler-failed'
                        cache = schemas / 'gschemas.compiled'
                        if run.returncode == 0:
                            result['stage'] = 'missing-output'
                            if cache.is_file() and not cache.is_symlink() and cache.stat().st_size:
                                result['cache_sha256'] = hashlib.sha256(cache.read_bytes()).hexdigest()
                                result['stage'], code = 'complete', 0
                    except subprocess.TimeoutExpired:
                        result['stage'], code = 'compiler-timeout', 124
                    except OSError:
                        result['stage'], code = 'compiler-unavailable', 127
                    if log.is_file():
                        with log.open('rb') as stream:
                            observed = stream.read(LIMIT + 1)
                        result['diagnostic_truncated'] = len(observed) > LIMIT
                        observed = observed[:LIMIT]
                        result['diagnostic_sha256'] = hashlib.sha256(observed).hexdigest()
                        result['loader_error_observed'] = b'error while loading shared libraries:' in observed
    except OSError:
        result['stage'], code = 'preparation-io-error', 1
    try:
        receipt.write_text(json.dumps(result, indent=2) + '\n')
    except OSError:
        return 1  # Never emit a path-bearing exception if receipt storage fails.
    return code

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', required=True, type=Path)
    p.add_argument('--log', required=True, type=Path)
    p.add_argument('--receipt', required=True, type=Path)
    a = p.parse_args()
    return prepare(a.root, a.log, a.receipt)

if __name__ == '__main__':
    raise SystemExit(main())

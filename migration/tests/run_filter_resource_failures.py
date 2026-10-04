#!/usr/bin/env python3
"""Exercise existing worker failures and a child-only GLib allocation limit.

The caller must hold the shared build/test lock and load the configured build
environment. This is not a whole-application OOM or memory-pressure campaign.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shlex
import signal
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
PROBE = r'''#include <glib.h>
#include <stdio.h>
#include <string.h>
int main (int argc, char **argv) {
  const gsize size = 256u * 1024u * 1024u;
  if (argc != 2) return 2;
  printf ("GLib %u.%u.%u; requested=%zu\n", glib_major_version,
          glib_minor_version, glib_micro_version, (size_t) size);
  fflush (stdout);
  if (!strcmp (argv[1], "try")) {
    gpointer memory = g_try_malloc (size);
    if (memory) { g_free (memory); return 3; }
    puts ("TRY_ALLOCATION_FAILED_WITHOUT_ABORT");
    return 0;
  }
  if (!strcmp (argv[1], "fatal")) {
    gpointer memory = g_malloc (size);
    g_free (memory);
    return 4;
  }
  return 2;
}
'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sources = ['app/painter/filter-process.cpp', 'app/painter/filter-scheduler.cpp',
               'app/painter/filter-spool.cpp', 'app/painter/tests/test-filter-process.cpp',
               'app/painter/tests/test-filter-scheduler.cpp', 'app/painter/tests/test-filter-spool.cpp',
               'migration/tests/run_filter_resource_failures.py']
    before = {name: sha(ROOT/name) for name in sources}
    results = []

    def run(name, command, env=None, preexec=None, timeout=90):
        start = time.monotonic()
        result = subprocess.run(command, cwd=build, env=env, preexec_fn=preexec,
                                capture_output=True, text=True, timeout=timeout)
        log = out/(name+'.log')
        log.write_text(result.stdout+result.stderr)
        item = dict(name=name, command=[str(v) for v in command], exit_code=result.returncode,
                    seconds=time.monotonic()-start, log=log.name, log_sha256=sha(log))
        return result, item

    registry = json.loads((build/'meson-info/intro-tests.json').read_text())
    expected = {'painter-filter-process': '115 Filter process cases passed',
                'painter-filter-scheduler': '# End of painter-filter-scheduler tests',
                'painter-filter-spool': '# End of filter-spool tests'}
    for name, marker in expected.items():
        test = next(t for t in registry if t['name'] == name)
        environment = os.environ.copy()
        prior = environment.get('LD_LIBRARY_PATH', '')
        environment.update(test.get('env', {}))
        if 'LD_LIBRARY_PATH' in test.get('env', {}):
            environment['LD_LIBRARY_PATH'] += ':'+prior
        command = [str((build/arg).resolve()) if index == 0 and not Path(arg).is_absolute()
                   else arg for index, arg in enumerate(test['cmd'])]
        result, item = run(name, command, environment)
        item.update(binary_sha256=sha(Path(command[0])), passed=result.returncode == 0 and
                    marker in result.stdout and '# SKIP' not in result.stdout)
        results.append(item)
        if not item['passed']:
            raise RuntimeError('Worker boundary test failed: '+name)

    source, binary = out/'glib-allocation-probe.c', out/'glib-allocation-probe'
    source.write_text(PROBE)
    flags = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', 'glib-2.0'], text=True))
    command = ['cc', str(source), '-o', str(binary), *flags]
    result, item = run('compile-glib-probe', command)
    item['passed'] = result.returncode == 0
    results.append(item)
    if result.returncode:
        raise RuntimeError('GLib probe compilation failed')

    def bounded_child():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (128*1024*1024, 128*1024*1024))
        resource.setrlimit(resource.RLIMIT_CPU, (5, 5))

    for mode in ('try', 'fatal'):
        result, item = run('glib-'+mode, [str(binary), mode], preexec=bounded_child, timeout=10)
        item.update(address_space_limit_bytes=128*1024*1024, request_bytes=256*1024*1024,
                    binary_sha256=sha(binary), source_sha256=sha(source))
        if mode == 'try':
            item['passed'] = result.returncode == 0 and 'TRY_ALLOCATION_FAILED_WITHOUT_ABORT' in result.stdout
        else:
            item['passed'] = result.returncode in (-signal.SIGTRAP, -signal.SIGABRT) and \
                             'failed to allocate' in result.stderr
            item['meaning'] = 'Expected upstream fatal g_malloc behavior, not component recovery'
        results.append(item)
    after = {name: sha(ROOT/name) for name in sources}
    report = dict(scope='Existing isolated worker/process/scheduler failure boundaries and child-only GLib allocator behavior',
                  results=results, source_sha256=before, source_unchanged=before == after,
                  all_passed=all(item['passed'] for item in results) and before == after,
                  limitations=['No host exhaustion or whole-application/whole-dependency OOM injection',
                               'Native schema/editor scoped C++ and g_try allocation injection is recorded separately',
                               'Fatal g_malloc behavior is an upstream baseline, not a claimed recovery path'])
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())

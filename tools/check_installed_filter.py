#!/usr/bin/env python3
"""Verify an installed Filter route, its executable locations and saved pixels."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('filter_owned_processes', ROOT/'tools/check_filter_active_quit.py')
observer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(observer)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, environment, executables, output):
    output.mkdir(parents=True, exist_ok=False)
    oracle_path = ROOT/'migration/fixtures/legacy-blinds-package-smoke.json'
    oracle = json.loads(oracle_path.read_text())
    fixture = ROOT/oracle['fixture']
    if not oracle['capture_passed'] or sha(fixture) != oracle['fixture_sha256']:
        raise RuntimeError('Installed Filter smoke oracle/fixture provenance changed')
    script = ROOT/'migration/tests/filter-package-smoke.py'
    (output/'observer.executed.py').write_bytes(Path(__file__).read_bytes())
    (output/'batch.executed.py').write_bytes(script.read_bytes())
    events, start, result = output/'events.jsonl', output/'start', output/'batch-result.json'
    env = {**environment, 'GIMP_PAINTER_FILTER_ORACLE':str(oracle_path),
           'GIMP_PAINTER_FILTER_FIXTURE':str(fixture), 'GIMP_PAINTER_FILTER_RESULT':str(result),
           'GIMP_PAINTER_FILTER_EVENTS':str(events), 'GIMP_PAINTER_FILTER_START':str(start)}
    arguments = [*command, '--new-instance', '--no-interface', '--no-data', '--no-fonts', '--no-splash',
                 '--batch-interpreter=python-fu-eval', '-b', '-', '--quit']
    observed, profiles, errors = {}, set(), []
    ready = False
    began = time.monotonic()
    expected = {name:str(Path(path).resolve()) for name,path in executables.items()}
    with (output/'console.log').open('w') as log:
        process = subprocess.Popen([str(value) for value in arguments], env=env,
                                   stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
                                   text=True, start_new_session=True)
        try:
            process.stdin.write(script.read_text())
            process.stdin.close()
            while process.poll() is None:
                current = observer.descendants(process.pid)
                observed.update({(item['pid'],item['start_ticks']):item for item in current.values()})
                helpers = [item for item in current.values() if item['exe'] == expected['helper'] and item['state'] != 'Z']
                for item in helpers:
                    if len(item['argv']) == 3 and item['argv'][1] in ('--filter-worker-v1','--filter-worker-v2'):
                        profiles.add(item['argv'][2])
                if not ready and any(item['event']=='READY' for item in observer.events(events)):
                    if helpers:
                        errors.append('A helper started before explicit lower-source invalidation')
                    ready = True
                    start.write_text('Observed initial cache and no preexisting helper\n')
                if time.monotonic()-began > 150:
                    errors.append('Installed Filter smoke exceeded150 seconds')
                    os.killpg(process.pid, signal.SIGKILL)
                    break
                time.sleep(0.005)
            code = process.wait(timeout=5)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    contents = (output/'console.log').read_text(errors='replace')
    if code != 0 or 'INSTALLED_FILTER_EXACT_SAVE_REOPEN_OK' not in contents:
        errors.append('Console did not confirm exact Filter output and Save/reopen')
    for marker in observer.SANITIZER_DIAGNOSTICS + ('batch command experienced', 'Traceback (most recent call last)', 'Filter cleanup exceeded'):
        if marker in contents:
            errors.append('Runtime diagnostic: '+marker)
    helpers = [item for item in observed.values() if item['exe'] == expected['helper']]
    helper_ids = {item['pid'] for item in helpers}
    plugins = [item for item in observed.values() if item['exe'] == expected['plugin'] and item['parent'] in helper_ids]
    if not ready or not helpers or not plugins:
        errors.append('Missing initial readiness or actual installed helper/native plugin observation')
    # No occurrence of a Filter helper/plugin from another installation may
    # count as success even if the completed pixel checksum happens to match.
    all_helper_ids = {item['pid'] for item in observed.values() if Path(item['exe']).name == 'gimp-painter-filter-worker'}
    wrong = [item for item in observed.values()
             if (Path(item['exe']).name == 'gimp-painter-filter-worker' and item['exe'] != expected['helper']) or
                (Path(item['exe']).name == 'blinds' and item['parent'] in all_helper_ids and item['exe'] != expected['plugin'])]
    if wrong:
        errors.append('A Filter executable was resolved outside the intended runtime')
    survivors = [item for item in observed.values() if observer.still_same(item)]
    leftovers = [path for path in sorted(profiles) if Path(path).exists()]
    if survivors or leftovers:
        errors.append('Observed process or private worker profile survived console exit')
    # Failure cleanup only signals groups whose test-owned identity still
    # exists. A numeric PID/group after ownership loss is never enough.
    if errors:
        for item in survivors:
            if observer.still_same(item):
                try:
                    os.killpg(item['group'], signal.SIGKILL)
                except ProcessLookupError:
                    pass
    batch_result = json.loads(result.read_text()) if result.exists() else None
    if not batch_result or batch_result.get('status') != 'passed' or batch_result.get('expected_sha256') != oracle['expected_rgba_sha256']:
        errors.append('Missing exact batch result record')
    report = dict(status='passed' if not errors else 'failed', errors=errors,
                  exit_code=code, seconds=time.monotonic()-began,
                  command=[str(value) for value in arguments], expected_executables=expected,
                  observed_helpers=helpers, observed_plugins=plugins, wrong_installation=wrong,
                  survivors=survivors, leftover_profiles=leftovers,
                  events=observer.events(events),
                  batch_result=batch_result,
                  fixture_sha256=sha(fixture), oracle_sha256=sha(oracle_path),
                  batch_source_sha256=sha(script), observer_source_sha256=sha(Path(__file__)),
                  console_log_sha256=sha(output/'console.log'))
    (output/'report.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def installed_executables(bundle):
    result = {}
    for kind, pattern in [('helper','usr/**/gimp-painter-filter-worker'), ('plugin','usr/**/plug-ins/blinds/blinds')]:
        matches = [p for p in bundle.glob(pattern) if p.is_file()]
        if len(matches) != 1:
            raise RuntimeError('Expected exactly one installed '+kind+' executable')
        result[kind] = matches[0]
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundle', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    bundle = args.bundle.resolve()
    profile = args.output.resolve().parent/'filter-smoke-profile'
    profile.mkdir(exist_ok=False)
    environment = {'PATH':'/usr/bin:/bin', 'LANG':'C.UTF-8', 'HOME':str(profile),
                   'XDG_CONFIG_HOME':str(profile/'config'), 'XDG_CACHE_HOME':str(profile/'cache'),
                   'XDG_DATA_HOME':str(profile/'data')}
    report = run([bundle/'AppRun','--console'], environment, installed_executables(bundle), args.output.resolve())
    print(json.dumps(dict(status=report['status'], errors=report['errors'])))
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    sys.exit(main())

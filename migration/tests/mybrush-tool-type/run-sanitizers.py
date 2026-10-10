from pathlib import Path
import subprocess, json, hashlib, re, os
root = Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree')
out = Path(__file__).resolve().parent
reports = {kind: json.loads((out / (kind + '-sanitizer-build.json')).read_text()) for kind in ('tool', 'editor')}
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
for kind, report in reports.items():
    assert not report['changed_during_build']
    assert sha(Path(report['executable'])) == report['executable_sha256']
    for path, digest in report['sources_sha256'].items(): assert sha(root / path) == digest, path
jobs = [('tool', 'standalone-lifecycle'), ('tool', 'public-press-last-ref'), ('editor', '13-owner-destruction-orders'), ('editor', '09-close-during-refresh')]
env = os.environ.copy()
env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
results = []
for kind, case in jobs:
    report = reports[kind]
    label = 'asan-' + kind + '-' + case
    argv = [report['executable'], '-p', '/painter-' + kind + '/' + case]
    completed = subprocess.run(argv, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=240)
    output = completed.stdout
    cases = re.findall(r'^ok \d+ (.+)$', output, re.M)
    errors = re.findall(r'(?:ERROR: AddressSanitizer|runtime error:|SUMMARY: (?:AddressSanitizer|UndefinedBehaviorSanitizer))[^\n]*', output)
    result = dict(name=label, argv=argv, exit_code=completed.returncode, status='PASS' if completed.returncode == 0 and len(cases) == 1 and not errors else 'FAIL', cases=cases, sanitizer_errors=errors, stdout=output, binary_sha256=sha(Path(report['executable'])), build_report_sha256=sha(out / (kind + '-sanitizer-build.json')), sanitizer_options={key: env[key] for key in ('ASAN_OPTIONS', 'UBSAN_OPTIONS')})
    (out / (label + '.json')).write_text(json.dumps(result, indent=2) + '\n')
    (out / (label + '.log')).write_text(output)
    results.append(result)
    print(json.dumps({key: result[key] for key in ('name', 'status', 'exit_code', 'cases')}), flush=True)
    if result['status'] != 'PASS': print(output[-5000:], flush=True); break
for kind, report in reports.items():
    assert sha(Path(report['executable'])) == report['executable_sha256']
    for path, digest in report['sources_sha256'].items(): assert sha(root / path) == digest, path
summary = dict(results=[{key: r[key] for key in ('name', 'status', 'exit_code', 'cases', 'sanitizer_errors', 'binary_sha256', 'build_report_sha256')} for r in results], build_reports={kind: sha(out / (kind + '-sanitizer-build.json')) for kind in reports}, scope='Four focused native GTK lifecycle cases only; source coverage in build reports. Remaining GIMP/dependencies uninstrumented; LeakSanitizer disabled.', changed_during_run=[])
(out / 'sanitizer-summary.json').write_text(json.dumps(summary, indent=2) + '\n')
assert len(results) == 4 and all(r['status'] == 'PASS' for r in results)

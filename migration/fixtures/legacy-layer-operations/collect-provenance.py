"""Collect local build/source evidence before exporting a NEW capture package."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile

base = Path(__file__).resolve().parent
source = base / 'gimp'
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
patch = subprocess.check_output(['git', 'diff', '--binary', 'HEAD'], cwd=source)
with tempfile.TemporaryDirectory(prefix='layer-operations-patch-check-', dir=base) as temp:
    root = Path(temp)
    changed = subprocess.check_output(['git', 'diff', '--name-only', 'HEAD'], cwd=source).decode().splitlines()
    for name in changed:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(subprocess.check_output(['git', 'show', 'HEAD:' + name], cwd=source))
    patch_path = root / 'overlay.patch'
    patch_path.write_bytes(patch)
    result = subprocess.run(['git', 'apply', '--check', str(patch_path)], cwd=root, capture_output=True, text=True)
    (base / 'patch-check.log').write_text(f'git apply --check on pinned source files: exit={result.returncode}\n' + result.stdout + result.stderr)
    result.check_returncode()
archives = []
for name in ['app/core/libappcore.a', 'app/xcf/libappxcf.a', 'app/paint/libapppaint.a', 'app/base/libappbase.a']:
    prior = base.parent / 'legacy-composite-20261015/gimp' / name
    archives.append({'path': name, 'capture_sha256': sha(source / name),
                     'prior_build_sha256': sha(prior) if prior.exists() else None,
                     'same': sha(source / name) == sha(prior) if prior.exists() else None})
(base / 'archive-provenance.json').write_text(json.dumps(archives, indent=2) + '\n')
# Run in Bash to source the exact sibling loader/dependency environment.
command = 'source "$1/../legacy-filter-positive-20261015/build/env.sh"; ldd "$1/gimp/app/tests/.libs/test-xcf"'
result = subprocess.run(['bash', '-c', command, 'collect-provenance', str(base)], capture_output=True, text=True)
(base / 'dependencies.txt').write_text(result.stdout + result.stderr)
result.check_returncode()

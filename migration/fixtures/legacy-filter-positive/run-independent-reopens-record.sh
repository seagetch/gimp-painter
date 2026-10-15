#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
set +u
source "$base/build/env.sh"
set -u
mkdir -p "$base/independent-positive/profile" "$base/independent-negative/profile"
python3 - "$base" <<'PY'
import sys
from pathlib import Path
b=Path(sys.argv[1]); s=(b/'fixture-sources/independent-reopen-template.scm').read_text()
s=s.replace('FILE_PATH',str(b/'capture-builtin/fixtures/filter-brightness-contrast.xcf')).replace('EXPECTED_NAME','brightness filter')
(b/'independent-positive/reopen.scm').write_text(s)
f=b.parent/'recovery-ui-20261015/source/migration/fixtures/legacy-runtime/filter-edge.xcf'
(b/'independent-negative/reopen.scm').write_text(f'(gimp-file-load RUN-NONINTERACTIVE "{f}" "{f}")\n(gimp-quit 0)\n')
PY
for mode in positive negative; do
 set +e
 GIMP2_DIRECTORY="$base/independent-$mode/profile" timeout 60 "$base/build/prefix/bin/gimp-2.8" --no-interface --no-shm --batch-interpreter=plug-in-script-fu-eval -b "(load \"$base/independent-$mode/reopen.scm\")" > "$base/independent-$mode/reopen.log" 2>&1
 status=$?
 set -e
 printf '%s\n' "$status" > "$base/independent-$mode/exit-code"
done
sha256sum "$base/build/prefix/bin/gimp-2.8" > "$base/independent-original-binary.sha256"

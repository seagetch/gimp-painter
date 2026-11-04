#!/usr/bin/env bash
# DRAFT routing wrapper only. The accepted donor's original306 helper/driver
# remain unchanged. No use on the restricted local executor.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
candidate=$(realpath "${1:?candidate checkout required}")
pins=$(realpath "${2:?externally sealed runtime pins required}")
out=$(realpath -m "${3:?artifact directory required}")
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: authorized ordinary Actions only' >&2; exit 2; }
accepted=$(python3 -B - "$here" "$pins" <<'PY'
import json,sys
sys.path.insert(0,sys.argv[1])
from prepare_brush_baseline import commits
print(commits(json.load(open(sys.argv[2])))[0])
PY
)
donor=$(mktemp -d "${RUNNER_TEMP:?}/painter-common306-donor.XXXXXXXX")
git -C "$donor" init
git -C "$donor" remote add origin https://github.com/seagetch/gimp-painter.git
git -C "$donor" fetch --no-tags --depth=1 origin "$accepted"
git -C "$donor" checkout --detach FETCH_HEAD
test "$(stat -c %u "$donor")" = "$(id -u)"
test "$(git -C "$donor" rev-parse HEAD^{tree})" = 8634eeef49c81aaa5c25192cb76d749ff680683f
git -C "$donor" fetch --no-tags --depth=1 origin d4ce384f708090facbf6b08edc38b4ddcf84cff0
mkdir -p "$out"
python3 -B "$here/verify_common_donor.py" --candidate "$candidate" --donor "$donor" \
  --pins "$pins" --output "$out/common306-donor-identity.json"
# All actual baseline overlay/build/RED/integrity checks remain the donor's
# unchanged implementation. A source compatibility report is not acceptance.
cd "$donor"
exec bash tools/run-painter-ui-ci.sh "$out" baseline-carrier-red

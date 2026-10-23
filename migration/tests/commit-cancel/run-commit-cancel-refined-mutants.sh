set -euo pipefail
source <native>/env.sh
exec 9><shared>/gimp-painter-build.lock;flock 9
S=<native>/source
B=<native>/build-port
E=<evidence>
python3 <evidence>/run-commit-cancel-mutations.py --source "$S" --build "$B" --output "$E" >"$E/refined-mutations.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --num-processes 1 --print-errorlogs --logbase commit-cancel-restored gimp-painter-surface >"$E/restored.log" 2>&1

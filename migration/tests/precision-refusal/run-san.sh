set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=<checkout>
S=<native>/source
B=<native>/build-port
E=<evidence>
cp "$R/migration/tests/build_precision_refusal_sanitizers.py" "$S/migration/tests/"
python3 "$S/migration/tests/build_precision_refusal_sanitizers.py" "$B" --report "$E/sanitizer-build.json" >"$E/sanitizer-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" python3 "$E/run-san.py" "$S" "$B" "$E" >"$E/sanitizer.log" 2>&1

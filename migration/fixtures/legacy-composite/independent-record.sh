#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
set +u
source "$base/../legacy-filter-positive-20261015/build/env.sh"
set -u
for scene in anime watercolor; do
 for state in initial edited; do
 out="$base/evidence-nonuniform/$scene/independent-$state"; mkdir -p "$out/profile"
 file="$base/evidence-nonuniform/$scene/$scene-$state.xcf"
 cat > "$out/reopen.scm" <<SCM
(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "$file" "$file")))
       (layers (gimp-image-get-layers image))
       (group (vector-ref (cadr layers) 0)))
 (if (not (= (car layers) 2)) (error "wrong root count"))
 (if (not (string=? (car (gimp-item-get-name group)) "New group")) (error "wrong group name"))
 (gimp-message "INDEPENDENT_ORIGINAL_COMPOSITE_REOPEN_PASS scene=$scene stage=$state"))
(gimp-quit 0)
SCM
 set +e
 GIMP2_DIRECTORY="$out/profile" timeout 60 "$base/../legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8" --no-interface --no-shm --batch-interpreter=plug-in-script-fu-eval -b "(load \"$out/reopen.scm\")" > "$out/reopen.log" 2>&1
 status=$?
 set -e
 echo "$status" > "$out/exit-code"
 done
done

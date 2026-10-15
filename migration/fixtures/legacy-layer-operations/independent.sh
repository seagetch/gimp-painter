#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
for scene in clone filter; do
 for state in baseline duplicate-group delete-source edit-restored delete-group-redo duplicate-filter delete-filter-redo; do
 file="$base/captures/$scene/$state.xcf"; [ -f "$file" ] || continue
 out="$base/captures/$scene/independent-$state"; [ -f "$out/exit-code" ] && continue; mkdir -p "$out/profile"
 cat > "$out/reopen.scm" <<SCM
(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "$file" "$file"))) (layers (gimp-image-get-layers image)))
 (if (not (= (car (gimp-image-width image)) 96)) (error "wrong width"))
 (if (not (= (car (gimp-image-height image)) 80)) (error "wrong height"))
 (gimp-message (string-append "INDEPENDENT_ORIGINAL_REOPEN_PASS scene=$scene step=$state roots=" (number->string (car layers)))))
(gimp-quit 0)
SCM
 set +e
 HOME="$out/profile" GIMP2_DIRECTORY="$out/profile" timeout 30 "$base/../legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8" --no-interface --no-shm --batch-interpreter=plug-in-script-fu-eval -b "(load \"$out/reopen.scm\")" > "$out/reopen.log" 2>&1
 status=$?
 set -e
 echo "$status" > "$out/exit-code"
 done
done
sha256sum "$base/../legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8" > "$base/original-binary-after.sha256"

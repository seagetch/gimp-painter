#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
for name in pen textured_ink watercolor_expressive; do
 out="$base/captures/$name";file="$out/final.xcf";mkdir -p "$base/independent-profile-$name"
 cat > "$out/independent.scm" <<SCM
(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "$file" "$file"))) (layers (gimp-image-get-layers image)))
 (if (not (= (car layers) 1)) (error "wrong layer count"))
 (if (not (= (car (gimp-image-width image)) 96)) (error "wrong width"))
 (if (not (= (car (gimp-image-height image)) 80)) (error "wrong height"))
 (if (not (string=? (car (gimp-item-get-name (vector-ref (cadr layers) 0))) "original brush output")) (error "wrong layer name"))
 (gimp-message "INDEPENDENT_ORIGINAL_STROKE_REOPEN_PASS brush=$name"))
(gimp-quit 0)
SCM
 GIMP2_DIRECTORY="$base/independent-profile-$name" timeout 60 "$base/../legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8" --no-interface --no-shm --batch-interpreter=plug-in-script-fu-eval -b "(load \"$out/independent.scm\")" > "$out/independent.log" 2>&1
 echo $? > "$out/independent-exit-code"
 grep INDEPENDENT_ORIGINAL_STROKE_REOPEN_PASS "$out/independent.log"
done

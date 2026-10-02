(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-indexed.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-indexed.png"))) (layer (car (gimp-image-get-active-layer image))))
 (gimp-message (string-append "INDEXED_PROBE=plug-in-gauss TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (gimp-message "INDEXED_UNEXPECTED_SUCCESS=plug-in-gauss") (gimp-image-delete image))

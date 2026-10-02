(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-indexed.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-indexed.png"))) (layer (car (gimp-image-get-active-layer image))))
 (gimp-message (string-append "INDEXED_PROBE=plug-in-edge TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2 2 0)
 (gimp-message "INDEXED_UNEXPECTED_SUCCESS=plug-in-edge") (gimp-image-delete image))

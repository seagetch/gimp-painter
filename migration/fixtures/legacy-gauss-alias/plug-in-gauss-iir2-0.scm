(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (gimp-message "ALIAS_REJECT_START=plug-in-gauss-iir2-0")
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 0 0)
 (gimp-message "ALIAS_REJECT_UNEXPECTED=plug-in-gauss-iir2-0") (gimp-image-delete image))

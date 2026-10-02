(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/loaded-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/loaded-rgb.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=rgb TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/loaded-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/loaded-gray.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=gray TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/loaded-tile-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/loaded-tile-rgb.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=tile-rgb TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-0.75-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-0.75-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-0.75-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-0.75-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-0.75-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-0.75-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-2.5-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-2.5-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-2.5-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-2.5-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-2.5-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-2.5-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-25-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-25-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-25-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-25-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir-25-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir-25-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 25 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-25-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-25-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir2-25-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 2.5 7.25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-2.5-7.25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-2.5-7.25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir2-2.5-7.25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 0 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-0-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-0-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir2-0-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 0.25 0.75)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-0.25-0.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-0.25-0.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir2-0.25-0.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-1-5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-iir2-1-5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-iir2-1-5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-0.75-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-0.75-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-0.75-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-0.75-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-0.75-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-0.75-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-2.5-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-2.5-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-2.5-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-2.5-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-2.5-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-2.5-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-25-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-25-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-25-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-25-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle-25-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle-25-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 25 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-25-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-25-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle2-25-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 2.5 7.25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-2.5-7.25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-2.5-7.25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle2-2.5-7.25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 0 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-0-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-0-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle2-0-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 0.25 0.75)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-0.25-0.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-0.25-0.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle2-0.25-0.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-1-5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/rgb-gauss-rle2-1-5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-gauss-rle2-1-5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-0.75-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-0.75-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-0.75-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-0.75-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 0.75 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-0.75-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-0.75-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-2.5-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-2.5-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-2.5-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-2.5-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-2.5-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-2.5-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-25-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-25-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-25-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-25-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 25 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir-25-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir-25-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 25 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-25-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-25-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir2-25-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 2.5 7.25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-2.5-7.25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-2.5-7.25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir2-2.5-7.25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 0 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-0-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-0-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir2-0-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 0.25 0.75)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-0.25-0.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-0.25-0.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir2-0.25-0.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-1-5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-iir2-1-5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-iir2-1-5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-0.75-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-0.75-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-0.75-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-0.75-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 0.75 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-0.75-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-0.75-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-2.5-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-2.5-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-2.5-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-2.5-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-2.5-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-2.5-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-25-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-1-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-1-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-25-1-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-0-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-0-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-25-0-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-0-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-0-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-25-0-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 25 2 -3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-2--3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle-25-2--3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle-25-2--3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 25 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-25-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-25-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle2-25-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 2.5 7.25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-2.5-7.25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-2.5-7.25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle2-2.5-7.25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 0 25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-0-25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-0-25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle2-0-25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 0.25 0.75)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-0.25-0.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-0.25-0.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle2-0.25-0.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-gray.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-1-5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/gray-gauss-rle2-1-5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-gauss-rle2-1-5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir RUN-NONINTERACTIVE image layer 2.5 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-iir-2.5-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-iir-2.5-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=tile-rgb-gauss-iir-2.5-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 2.5 7.25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-iir2-2.5-7.25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-iir2-2.5-7.25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=tile-rgb-gauss-iir2-2.5-7.25") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle RUN-NONINTERACTIVE image layer 2.5 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-rle-2.5-1-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-rle-2.5-1-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=tile-rgb-gauss-rle-2.5-1-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/input-tile-rgb.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 2.5 7.25)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-rle2-2.5-7.25.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-alias/tile-rgb-gauss-rle2-2.5-7.25.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=tile-rgb-gauss-rle2-2.5-7.25") (gimp-image-delete image))

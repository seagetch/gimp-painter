; Genuine pinned legacy PDB capture; channel-native Gray/Gray-alpha PNGs.

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-5x4.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=5x4 TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-1x1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=1x1 TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-1x5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=1x5 TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-5x1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=5x1 TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-67x66.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=67x66 TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-opaque.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=opaque TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-lowalpha.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=lowalpha TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-transparent.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=transparent TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-noalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/loaded-noalpha.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "GRAY_INPUT_LOADED=noalpha TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))
 (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m0-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m0-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m0-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m0-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m0-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m0-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m0-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m0-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m0-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m1-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m1-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m1-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m1-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m1-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m1-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m1-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m1-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m1-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m2-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m2-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m2-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m2-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m2-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m2-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m2-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m2-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m2-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m3-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m3-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m3-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m3-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m3-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m3-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m3-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m3-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m3-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 4)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m4-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m4-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m4-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 4)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m4-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m4-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m4-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 4)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m4-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m4-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m4-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m5-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m5-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m5-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m5-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m5-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m5-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m5-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x4-m5-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x4-m5-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h2.5-v7.25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h2.5-v7.25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h0-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h0-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h0-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h0-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h0-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h0-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h1-v5-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h1-v5-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h1-v5-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h1-v5-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x4-h1-v5-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x4-h1-v5-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m0-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m0-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x1-m0-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m0-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m0-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x1-m0-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m0-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m0-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x1-m0-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m5-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m5-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x1-m5-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m5-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m5-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x1-m5-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m5-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x1-m5-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x1-m5-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x1-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x1-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x1-h0.25-v0.75-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x1-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x1-h0.25-v0.75-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m0-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m0-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x5-m0-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m0-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m0-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x5-m0-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m0-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m0-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x5-m0-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m5-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m5-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x5-m5-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m5-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m5-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x5-m5-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m5-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-1x5-m5-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-1x5-m5-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x5-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x5-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x5-h0.25-v0.75-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-1x5-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-1x5-h0.25-v0.75-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m0-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m0-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x1-m0-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m0-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m0-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x1-m0-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m0-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m0-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x1-m0-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m5-w1-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m5-w1-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x1-m5-w1-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 2 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m5-w2-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m5-w2-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x1-m5-w2-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 1.75 3 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m5-w3-a1.75.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-5x1-m5-w3-a1.75.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-5x1-m5-w3-a1.75") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x1-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x1-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x1-h0.25-v0.75-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-5x1-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-5x1-h0.25-v0.75-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 1 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m0-w1-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m0-w1-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-67x66-m0-w1-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m0-w3-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m0-w3-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-67x66-m0-w3-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 1 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m5-w1-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m5-w1-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-67x66-m5-w1-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 3 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m5-w3-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-67x66-m5-w3-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-67x66-m5-w3-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-67x66-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-67x66-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-67x66-h2.5-v7.25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-67x66-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-67x66-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-67x66-h2.5-v7.25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-opaque-m0-w2-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-opaque-m0-w2-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-opaque-m0-w2-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-opaque-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-opaque-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-opaque-h2-v2-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-opaque-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-opaque-h2-v2-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-lowalpha-m0-w2-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-lowalpha-m0-w2-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-lowalpha-m0-w2-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-lowalpha-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-lowalpha-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-lowalpha-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-lowalpha-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-lowalpha-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-lowalpha-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-transparent-m0-w2-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-transparent-m0-w2-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-transparent-m0-w2-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-transparent-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-transparent-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-transparent-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-transparent-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-transparent-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-transparent-h25-v25-m1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-noalpha-m0-w2-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-noalpha-m0-w2-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-noalpha-m0-w2-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-edge RUN-NONINTERACTIVE image layer 2.0 2 5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-noalpha-m5-w2-a2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/edge-noalpha-m5-w2-a2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=edge-noalpha-m5-w2-a2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-noalpha-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-noalpha-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-noalpha-h25-v25-m0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/input-noalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-noalpha-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gray-filter/gauss-noalpha-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "GRAY_CASE_DONE=gauss-noalpha-h25-v25-m1") (gimp-image-delete image))

; Real pinned legacy plug-in-gauss PDB capture, unselected RGBA8.

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-1x1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-1x1.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-1x5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-1x5.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-5x1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-5x1.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-5x4.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-5x4.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-67x66.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-67x66.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-constant.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-constant.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-lowalpha.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-lowalpha.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-opaque.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-opaque.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/loaded-transparent.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_INPUT_LOADED=input-transparent.rgba"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h2.5-v7.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h2.5-v7.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h0-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h0-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v0-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v0-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h25-v0-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v0-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h25-v0-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h25-v0-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h0.25-v0.75-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h0.25-v0.75-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h1-v5-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h1-v5-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h1-v5-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h1-v5-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h1-v5-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h1-v5-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v3-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v3-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h2-v3-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x4.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v3-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x4-h2-v3-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x4-h2-v3-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h2.5-v7.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h2.5-v7.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h0-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h0-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v0-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v0-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h25-v0-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v0-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h25-v0-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h25-v0-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h0.25-v0.75-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h0.25-v0.75-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h1-v5-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h1-v5-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h1-v5-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h1-v5-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h1-v5-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h1-v5-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v3-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v3-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h2-v3-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v3-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x1-h2-v3-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x1-h2-v3-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h2.5-v7.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h2.5-v7.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h0-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h0-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v0-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v0-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h25-v0-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v0-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h25-v0-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h25-v0-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h0.25-v0.75-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h0.25-v0.75-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h1-v5-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h1-v5-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h1-v5-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h1-v5-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h1-v5-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h1-v5-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v3-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v3-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h2-v3-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-1x5.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v3-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-1x5-h2-v3-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-1x5-h2-v3-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h2.5-v7.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h2.5-v7.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h0-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h0-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v0-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v0-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h25-v0-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v0-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h25-v0-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h25-v0-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h0.25-v0.75-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h0.25-v0.75-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h1-v5-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h1-v5-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h1-v5-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h1-v5-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h1-v5-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h1-v5-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v3-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v3-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h2-v3-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-5x1.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v3-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-5x1-h2-v3-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-5x1-h2-v3-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2.5-v7.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2.5-v7.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h2.5-v7.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2.5 7.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2.5-v7.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2.5-v7.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h2.5-v7.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h0-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h0-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v0-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v0-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h25-v0-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 0 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v0-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h25-v0-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h25-v0-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0.25-v0.75-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0.25-v0.75-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h0.25-v0.75-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.25 0.75 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0.25-v0.75-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h0.25-v0.75-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h0.25-v0.75-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h1-v5-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h1-v5-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h1-v5-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 1 5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h1-v5-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h1-v5-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h1-v5-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v3-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v3-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h2-v3-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-67x66.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v3-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-67x66-h2-v3-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-67x66-h2-v3-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-constant-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-constant-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-constant-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-constant-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h0.75-v3.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h0.75-v3.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-constant-h0.75-v3.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-constant.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h0.75-v3.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-constant-h0.75-v3.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-constant-h0.75-v3.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-opaque-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-opaque-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-opaque-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-opaque-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h0.75-v3.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h0.75-v3.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-opaque-h0.75-v3.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-opaque.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h0.75-v3.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-opaque-h0.75-v3.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-opaque-h0.75-v3.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-transparent-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-transparent-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-transparent-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-transparent-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h0.75-v3.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h0.75-v3.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-transparent-h0.75-v3.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-transparent.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h0.75-v3.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-transparent-h0.75-v3.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-transparent-h0.75-v3.25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h25-v25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h25-v25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-lowalpha-h25-v25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 25 25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h25-v25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h25-v25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-lowalpha-h25-v25-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h2-v2-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h2-v2-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-lowalpha-h2-v2-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 2 2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h2-v2-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h2-v2-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-lowalpha-h2-v2-m1"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h0.75-v3.25-m0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h0.75-v3.25-m0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-lowalpha-h0.75-v3.25-m0"))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/input-lowalpha.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 0.75 3.25 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h0.75-v3.25-m1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss/gauss-lowalpha-h0.75-v3.25-m1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-image-delete image) (gimp-message "GAUSS_CASE_DONE=gauss-lowalpha-h0.75-v3.25-m1"))

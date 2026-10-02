(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-rgb-odd.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=rgb-odd TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-gray-odd.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=gray-odd TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-rgb-even.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=rgb-even TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/loaded-gray-even.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message (string-append "ALIAS_INPUT=gray-even TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-iir2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-iir2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--1.1-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--1.1-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--1.1-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--2-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--2-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--2-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--5-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--5-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--5-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--17-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--17-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--17-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--2-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--2-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-3--2-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--5-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--5-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-3--5-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--17-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--17-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-3--17-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-rle2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-rle2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--1.1-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--1.1-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--1.1-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--2-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--2-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--2-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--5-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--5-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--5-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--17-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss--17-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss--17-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--2-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--2-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-3--2-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--5-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--5-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-3--5-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--17-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-odd-gauss-3--17-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-odd-gauss-3--17-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-iir2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-iir2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--1.1-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--1.1-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--1.1-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--2-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--2-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--2-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--5-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--5-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--5-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--17-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--17-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--17-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--2-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--2-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-3--2-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--5-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--5-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-3--5-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--17-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--17-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-3--17-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-rle2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-rle2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--1.1-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--1.1-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--1.1-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--2-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--2-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--2-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--5-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--5-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--5-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--17-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss--17-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss--17-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--2-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--2-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-3--2-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--5-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--5-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-3--5-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-odd.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--17-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-odd-gauss-3--17-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-odd-gauss-3--17-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-iir2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-iir2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--1.1-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--1.1-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--1.1-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--2-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--2-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--2-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--5-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--5-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--5-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--17-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--17-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--17-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--2-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--2-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-3--2-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--5-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--5-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-3--5-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--17-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--17-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-3--17-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-rle2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-rle2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--1.1-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--1.1-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--1.1-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--2-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--2-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--2-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--5-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--5-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--5-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--17-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss--17-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss--17-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--2-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--2-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-3--2-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--5-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--5-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-3--5-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-rgb-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--17-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/rgb-even-gauss-3--17-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=rgb-even-gauss-3--17-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-iir2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-iir2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-iir2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--1.1-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--1.1-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--1.1-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--2-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--2-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--2-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--5-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--5-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--5-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--17-3-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--17-3-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--17-3-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--2-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--2-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-3--2-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--5-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--5-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-3--5-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 0)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--17-0.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--17-0.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-3--17-0") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -1.1 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--1.1-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--1.1-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2--1.1-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -2 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--2-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--2-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2--2-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -5 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--5-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--5-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2--5-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer -17 3)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--17-3.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2--17-3.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2--17-3") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -2)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2-3--2.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2-3--2.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2-3--2") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -5)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2-3--5.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2-3--5.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2-3--5") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss-rle2 RUN-NONINTERACTIVE image layer 3 -17)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2-3--17.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-rle2-3--17.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-rle2-3--17") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -1.1 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--1.1-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--1.1-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--1.1-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -2 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--2-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--2-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--2-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -5 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--5-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--5-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--5-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer -17 3 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--17-3-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss--17-3-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss--17-3-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -2 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--2-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--2-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-3--2-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -5 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--5-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--5-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-3--5-1") (gimp-image-delete image))

(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/input-gray-even.png"))) (layer (car (gimp-image-get-active-layer image))))
 (plug-in-gauss RUN-NONINTERACTIVE image layer 3 -17 1)
 (file-png-save2 RUN-NONINTERACTIVE image layer "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--17-1.png" "/workspace/scratch/5b5281e79681/gimp-painter/migration/fixtures/legacy-gauss-negative/gray-even-gauss-3--17-1.png" 0 9 0 0 0 0 0 0 1)
 (gimp-message "ALIAS_DONE=gray-even-gauss-3--17-1") (gimp-image-delete image))

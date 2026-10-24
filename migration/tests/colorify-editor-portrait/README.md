# Colorify visible-dialog portrait follow-up

One actual GTK creation/pixel case PASS, Xvfb root 1080×1920, associated GimpDisplay with explicit oncanvas visibility/config assertions. The fixture now presents, moves and raises the actual dialog before capture. Before shows the four entries 0.5, 0.25, 1, 0 over white; after shows the resulting purple canvas. Independently checks every pixel (127,63,254,255), including the old luminance 254 behavior. Programmatic GTK entry/response signals, not a mouse-gesture test.

Production and prior seven normal/seven scoped sanitizer results at a2eb200d4f are unchanged. This additional one-case rendering follow-up is not another sanitizer run. The old source-audit correction 9fcd27bf35 remains applicable; no full old generic-editor or old-runtime parity claim. Missing icon/theme warnings remain visible in raw output.

Private five-TU normal link against immutable exact-55bf aggregate dependencies, three owned editor/harness inputs replacing their archive definitions; full linked-input and runtime-library hashes in build.json. Only screenshot fixture changed from a2eb; no clean source/object writes. Actual test binary and runtime libraries unchanged after execution. Archive contains the exact six selected owned/compiled source inputs; linked dependency identities are hashes, not a full rebuild claim.

Validate: `python3 migration/tests/colorify-editor-portrait/validate.py --check-owned`.

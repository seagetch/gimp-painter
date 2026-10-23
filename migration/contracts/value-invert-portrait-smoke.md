# Installed Value Invert portrait GUI smoke

Task `34.005/value-invert-portrait-smoke` is accepted for the bounded Linux
installed-application observation below. The package, legacy UI-equivalence,
full aggregate and whole-port parent gates remain open.

## Exact application and environment

The tested public commit is `8cade86785af938a254216448a2a5aab3a464cd6`, tree
`4b224b5437f60a6014f8825214598facfdd92e2e`. GIMP data is pinned to
`a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`. A fresh Debian 13 amd64 environment
locally extracted all 560 SHA-256-locked packages from the signed snapshot.
The complete default build finished all 3,460 steps and installed successfully.
The executable audit checked 250 ELF files without a missing default target or
permission repair; two explicitly optional targets were unbuilt.

The actual installed GIMP 3.0.9 executable ran with a new writable profile under
`tools/run-painter-xvfb.py`, at 1080 by 1920 pixels and depth 24. The loopback-only
TCP display accepted the authenticated client and rejected an empty authority.
The normal application was controlled through XTest keyboard/mouse events;
screenshots are raw full-screen XGetImage PNGs, without compositing or edits.

## Observed workflow

1. Open the synthetic 720 by 1080 U8 RGB study and enable View > Painter Canvas
   Controls. Custom tool/layer tiles, color controls and Paintbrush options are
   visible in the portrait workspace.
2. Use the Painter layer popup > New Filter Layer and choose Value Invert
   (Painter compatibility). A separate layer in Replace (Painter legacy) mode
   visibly transforms the study. Hiding that layer restores the source image.
3. Save an XCF through the normal application, close the image, and reopen the
   saved file through ordinary Open. Both source and independent filter layers
   are retained. The same visible canvas rectangle, 340,340 pixels, is exactly
   equal before saving and after reopening.
4. Reopen Filter Settings. The retained procedure is `plug-in-vinvert`, with
   three `gint` context values `[1, 1, 0]`; status is up to date. The selector is
   editable. Selecting Maximum/Minimum RGB and then cancelling leaves Value
   Invert selected when the editor is opened again. No new parameter value was
   committed; Value Invert has no additional operation-specific argument.
5. Quit normally using Ctrl+Q, exit zero. The saved XCF bytes remain unchanged;
   the Xvfb listener is closed and the shared build lock is released.

This route is the independent Painter FilterLayer, not the ordinary Colors
menu's GEGL Value Invert effect. The visible rectangle comparison does not
replace the separate native full-buffer or genuine old-writer semantic tests.

## Integrity and retained evidence

`migration/tests/value-invert-portrait-smoke.json` is the unmodified sanitized
run receipt, SHA-256
`8b1c5a8e33525790c391097b3cb607584299b9affd2a3597ae4e3f0debb47166`.
All 13,760 tracked source/submodule input files have byte-identical pre/post
manifests. The 276 sealed installed bin/lib files are unchanged; this is not a
claim that every installed data asset is individually sealed. Build/installed
application hashes are separately recorded. Install removes build RUNPATH;
their ELF Build ID and `.text` hash match. The preserved inspection attempt that
omitted an objcopy output path was refused as Text file busy, and whole-file
pre/post hashes confirm no binary change.

The durable evidence package is `GIMP-portrait-Value-Invert-GUI-evidence.zip`,
5,380,645 bytes, SHA-256
`983c95661ea95df464f3c343f89ac7654fe2dfcab937c6ea1f2c13754e55de1a`.
It retains build commands/logs, complete source/runtime manifests, GUI event
records, twenty original screenshots, synthetic input, saved XCF and terminal
checks. The reopened result and editable definition were also delivered as
native image attachments. The saved XCF SHA-256 is
`5179ade729930cf3c5f2639a433b1ecd51e357625a4c8ac8e26653a3a85eb129`.

Warnings are not suppressed or promoted to clean execution: the build retained
832 compiler-warning lines, and the GUI retained 112 unsupported-font messages,
D-Bus unavailability and 44 dconf warnings. There were no CRITICAL or
segmentation-fault log mentions. This smoke does not establish fresh aggregate,
sanitizer, LeakSanitizer, other-OS, tablet, exhaustive shutdown, full legacy UI
equivalence or release acceptance. The evidence-only publication commit was not
the executable tested above.

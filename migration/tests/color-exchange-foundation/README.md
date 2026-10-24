# Color Exchange exact old-C foundation

Missing route `plug-in-exchange`, pinned old source `afa43fae3e920210146abed514f136fd49f671b5:plug-ins/common/color-exchange.c`.

The old noninteractive procedure takes exactly12 arguments: run-mode INT32, image, drawable, then nine INT8 channels: from-red/green/blue, to-red/green/blue, red/green/blue-threshold. Only RGB drawables are accepted. Interactive and last-values modes substitute foreground color; those UI/PDB modes are not reproduced here.

This fixture compiles literal old `exchange()` and literal old GimpRGB set/get/add/subtract/clamp bodies, with bounded in-memory host stubs for drawable rows, progress and shadow dispatch. It does NOT build/run the old plug-in, old GTK, old PDB, selection-weighted shadow merge or any held diagnostic. Function extraction is checked against exact Git source bytes. GPL notices/provenance remain attached through original source and fixture licenses.

The independent oracle is integer distance: all three inclusive `abs(pixel-from)<=threshold` conditions must hold. Matching channels become `min(255,to+abs(pixel-from))`; the absolute delta is essential and is not signed color translation. Nonmatches and alpha remain exact. The old floating normalized-channel conversion yields precisely clamped integer from±threshold over all65,536 byte pairs.

Normal and ASan/UBSan each pass17,387,044 assertions/scenes:16,777,216 membership checks;589,824 directed RGB channel boundary/absolute-delta/saturation cases;20,004 random RGB/RGBA ROI, preview, empty cases. Host instrumentation checks row coordinates, read/write counts, source/alpha preservation, unchanged outside ROI, and shadow/preview dispatch. Five causal old-code mutations are rejected: exclusive low/high bounds, signed delta, overwritten alpha, omitted saturation. These are synthetic stimuli, not historic user images.

This foundation does not provide a production route, high-precision policy, cancellation scheduler, native selection merge, FilterLayer lifecycle, saved-argument binding, plug-in crash isolation or UI parity. Those remain explicit integration work. No generic GEGL color-exchange equivalence is claimed.

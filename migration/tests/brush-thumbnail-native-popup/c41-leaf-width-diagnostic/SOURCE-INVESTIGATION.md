# Run57 remaining layout investigation

Historical pre-implementation source assessment; C41 implementation review follows separately.

Status: source investigation only. No further repository edits or publication.
Frozen C40 remains tree ff80c80aee94f23a55156d68291ea7c707e14cc3 and repairs only
the independent canvas-mask support pin and its missing integration coverage.

## Verified result

Published C39 is ebafc05949389229b9dacbba35a5217523be229e. Run57 is terminal,
four successful jobs and four failed jobs. The independent mask gate fails closed
before its old/fixed/full107 observations. The primary, non-debugger sanitizer
case14 still takes 12,282,541us, stable0, longest dispatch12,278,760us. Other20
sanitizer leaf assertions pass but leaks remain199751–202391 bytes.

The separate debugger diagnostic captures the original image window resize at
11,328,136us and overlapping layout at12,066,919us. Its complete18-event phase
capture contains activation0→1, unchanged flags128, no global icon-theme event,
and no original-window style/state-flags event. This does NOT establish absence
of style work in descendants; those descendants were not observed by this probe.

All eight samples include ASan slow allocation-stack unwinding. Samples2,3,4,6
include rsvg_handle_new_from_stream_sync; sample5 includes GdkPixbuf stream loading.
Samples7–8 include Pango/cell-renderer or GLib allocation under recursive preferred
width measurement. Samples2–8 hit the96-frame cap. These identify work but not its
complete widget/call ownership or exclusive duration. Debugger overhead is not an
explanation of the independent primary failure. The early-theme change removed
the previously observed late notification in this run but did not repair readiness.

## Source-supported next step

GTK3.24.49 is the recorded dependency. Its gtkiconhelper.c size path may load an
icon surface when it has no known pixel/forced size. This matches the SVG sizing
samples; it does not prove an incorrect icon implementation or a safe optimization.
Source: https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkiconhelper.c

Do not change pixel sizes, prewarm the window, reduce unwind depth, hide widgets,
relax the3s predicate, or suppress leaks on this evidence. A low-risk next candidate
can instead add bounded, diagnostic-only leaf-width timing, with explicit missing
coverage: GtkImage preferred width, GtkCellRendererText preferred width, and
GtkCellRendererPixbuf legacy get_size. The independent source review identifies
several constraints before implementation:

- GtkCellRendererPixbuf uses legacy get_size. Existing GimpCellRendererButton
  and GimpCellRendererPopup inherit copied slots; wrapping only the parent after
  construction misses them. Its forced-size icon helper often avoids SVG decoding
  during sizing, so GtkImage is the stronger lazy-SVG candidate.
- A wrapper must forward the saved exact original once and restore every changed
  class slot. Enumerate supported concrete classes, reject unexpected overrides,
  and expose omitted/subclass coverage rather than silently assuming full coverage.
  Late-created subclasses can inherit a wrapper and retain it after parent restore;
  audit those class slots too. Use fixed per-class originals, never dynamic-type
  lookup or public sizing API redispatch from within a wrapper.
- Do not wrap height slots: GTK compares parent/subclass height vfunc identities
  for baseline dispatch, so a forwarding wrapper can still change behavior.
- GTK performs ensure_resize/ensure_style before the width vfunc. Leaf timings
  exclude that preceding work, and low totals cannot rule out CSS/icon workloads.
- Use fixed numeric class/window roles, fixed counters, explicit nested accounting,
  no callback allocation/logging, and no arbitrary object names, paths or addresses.
- Preserve the unchanged primary binary,3s predicate, protected26, leak settings,
  bounded96-frame collector, exact owned-child cleanup, and diagnostic-only status.

This is a proposed next diagnostic, not implemented or native-tested. No production
repair is justified by the current widget attribution. Keep C40 separate from it.

## Exact evidence

- brush-c39-run57/run57-terminal-observation-summary.json:
  5ef101b0391c0aa65d36fe834679973d63dc08920d2bd8fb83902e22de4e381b
- brush-c39-run57/sanitizer/readiness-owner-diagnostic/sample.json:
  f04f5e06f76b8e170ffff22e7fbe0ed0ef8928e870831fb4b156ec9dd7723988
- brush-c39-run57/sanitizer/brush-sanitizer-case14-results.json:
  400f43799f8c734d21823db29b7e85209948f5709bc4737dbb91f3337aac1618
No local native GIMP, ptrace or babl diagnostic was run. No source, seal, threshold,
or sanitizer settings were changed during this investigation.

# Run57 remaining layout cost: independent source review

Status: investigation only. Frozen `gimp-c40` was not edited. No native GIMP/GTK test, debugger, ptrace, babl operation, or network write was performed. Repository WBS and diagnostic instructions were read. This report does not justify a production repair. Keep C40's mask-provider support-pin repair separate from any later diagnostic proposal.

## Established evidence

- Primary `brush-c39-run57/sanitizer/brush-sanitizer-case14-results.json` fails the original first geometry wait at **12,282,541 us**, stable=0, with one dispatch of 12,278,760 us. This is independent of the later debugger observation, so debugger pauses cannot explain away the primary failure.
- Separate `sanitizer/readiness-owner-diagnostic/sample.json` observes the original mapped GimpImageWindow's check-resize at 11,328,136 us and frame-layout at 12,066,919 us. These overlap, not additive costs. They are debugger-inflated wall spans, not primary callback timing. `probe.inc:33–49` uses monotonic time; `collect.py:139–149` interrupts execution eight times and reads stacks without excluding stopped time.
- All eight stack samples are in ASan slow allocation-stack unwinding. Samples 2–6 contain GdkPixbuf loading; 2, 3, 4 and 6 explicitly contain librsvg, with XML parsing in 3, 4 and 6. Sample 7 contains Pango attribute copying and cell-renderer preferred width. Sample 8 contains GLib allocation under preferred-width recursion. These demonstrate actual workload but not its quantitative distribution. Samples 2–8 hit the 96-frame cap and omit the owner/wait; same-wait identity is not proven by each stack.
- The complete 18-event phase trace has one surviving original image window, activation 0→1, unchanged state flags=128, and no observed original-window style/state or default-theme event. It excludes the previously observed late theme event in this interval, not descendant-only CSS work or an earlier invalidation. The old all-toplevel owner snapshot remains clipped.

## Source pathways

`app/display/gimpimagewindow.c:367–383` and its parent `app/widgets/gimpwindow.c:69–89` do not override check-resize or preferred-size methods. The long owner span therefore encompasses inherited GTK sizing of the child tree, not a bespoke GIMP check-resize function.

The original window contains the menu/header, nested boxes/panes, image notebook, and dock columns (`gimpimagewindow.c:455–641`). The native-oncanvas session fixture includes layer/channel/path/tool-options/device/history/image and brush/pattern/font/document dockables. A window-wide preferred-size traversal can reach many icon and cell-renderer leaves even without a new top-level style notification.

The concrete GIMP style handler measures statusbar and menubar (`gimpimagewindow.c:984–1027`), but the new trace does not support blaming that handler. Allocation callbacks such as notebook orientation (`:3040–3053`) and painter home readiness (`:1658–1681`) are possible downstream work, not demonstrated causes. The latter queues reconciliation rather than directly rebuilding the tree. Do not patch these speculatively.

The failure precedes the popup click: `test-painter-brush-thumbnail-popup.inc:421–425` calls the geometry wait first. Thus it is not evidence that constructing the popup body or its brush editor costs 12 seconds.

## Recommended next diagnostic

A narrow, diagnostic-TU-only leaf timing probe is feasible and better justified than another production change:

1. GtkImage **get_preferred_width only**.
2. GtkCellRendererText **get_preferred_width only**.
3. GtkCellRendererPixbuf **get_size**, as a separate sizing category.

GtkImage width reaches its content-size callback and icon helper. GTK's icon helper can load the surface when size is otherwise unknown, matching the SVG stacks. [GtkImage source](https://raw.githubusercontent.com/GNOME/gtk/gtk-3-24/gtk/gtkimage.c), [GTK 3.24.49 icon helper](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkiconhelper.c).

Text width builds/measures a Pango layout, matching sample 7. [GtkCellRendererText source](https://raw.githubusercontent.com/GNOME/gtk/gtk-3-24/gtk/gtkcellrenderertext.c).

GTK 3.24.49 Pixbuf overrides legacy get_size; inherited preferred-size fallbacks redispatch there. Named pixbuf-renderer icons normally use forced dimensions, so this is not an assumed SVG hotspot. [Pixbuf implementation](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkcellrendererpixbuf.c), [fallback implementation](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkcellrenderer.c).

The two branch-source references above are guidance, not proof of byte identity with Debian's pinned 3.24.49-3 binary. Verify the exact package source before implementation approval.

### Required safeguards and interpretation

- Save each declaring class's original typed pointer before installing anything. Each dedicated wrapper must forward that exact pointer once with identical arguments. Never redispatch through a public size API or select an original using the receiver's dynamic class: either can recurse or call the wrong override.
- Global class mutation is not instance instrumentation. Existing derived classes have copied slots; later derived classes can inherit wrappers. Concrete coverage holes include GimpCellRendererButton (`gimpcellrendererbutton.c:48–86`) and GimpCellRendererPopup (`gimpcellrendererpopup.cpp:33`). Use explicit exact-type coverage and a bounded class/restore audit. Restore late descendant copies too, or fail the diagnostic and terminate its owned child safely. Do not claim complete coverage from three base classes.
- No class-height wrappers. GTK's baseline-support selection compares parent/subclass height-function pointers; changing one can alter dispatch even if its wrapper forwards unchanged. GTK also performs ensure-resize/style before invoking the width vfunc, so low leaf totals do not exclude costly style work. [GTK 3.24.49 sizing dispatch, lines 87–164](https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtksizerequest.c).
- Record bounded static class/owner ordinals, counts, total/max durations, nesting and unmatched state. Attribute renderer calls using the supplied host widget. No names, text, icon lookup, property allocation, logging, redraw, size query or cache warming inside callbacks. Do not retain observed widgets.
- Keep nested spans non-additive; use aggregate outermost accounting. If practical, a paired main-thread CPU clock can separate execution from stopped/descheduled wall time, but its difference is not exclusively debugger overhead. Current evidence cannot retrospectively subtract pauses.
- Before publication, prove exact forwarding/results, reentrancy, inherited and late-derived dispatch, restore/cleanup, bounded overflow, and all unchanged primary/protected-source hashes. Native confirmation remains required.

A precise source-hook alternative would bracket GTK's actual sizing/icon-helper functions in an exact separately sealed dependency build. That has cleaner dispatch semantics but materially expands dependency-build scope. GIMP statusbar/menubar source hooks are smaller but would not cover the presently observed generic GTK traversal. Prefer the narrow width/get_size probe only if its class-lifetime audit passes; otherwise use an explicitly reviewed source-hook design rather than accept ambiguous instrumentation.

Neither path may change the 3-second deadline, stable-frame assertion, leak policy, protected26, acceptance eligibility, or C40 pin-only scope.

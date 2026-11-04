# Focused GimpFrame label-style idempotence proposal

## Evidence, not a complete latency claim

Run33 first sampled stack is inside the first private-target Brush geometry wait,
then gimp_frame_apply_margins:185 -> gimp_frame_get_label_spacing:220 -> GTK style
lookup and nested widget-path construction. The later capped stacks show nested
preferred-width requests, XML/librsvg loading and slow sanitizer allocation
unwinding. These are not proof of infinite recursion or repeated GSource calls.

Both trees can contain the sampled class:
- private toolbar: app/tools/gimp-tools.c horizontal factory ->
  gimppainterpainttoolbar.c paint_toolbar_shared -> paint_toolbar_dynamics;
  Fade/Color GimpFrames are constructed at353/369 inside a GtkPopover; popup
  contents are shown eagerly by paint_toolbar_popup before the Brush click;
- ordinary dock: gimppaintoptions-gui.c builds analogous frames at352/392.
No sampled pointer/window identity establishes which tree this frame belongs to.

## Source-supported redundant invalidation

libgimpwidgets/gimpframe.c style_updated clears the indent cache, updates the
label, then applies margins. Its label update unconditionally invokes
libgimpwidgets/gimpwidgetsutils.c gimp_label_set_attributes, creating a complete
single full-range bold list and setting it even when already equal.

GTK3.24.49 gtk_label_set_attributes always notifies attributes, clears its cached
layout and queues resize. GTK margin-start/top setters already return for equal
values, so adding margin-value guards would not fix redundant invalidation.
Sources:
- https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtklabel.c (2166–2182)
- https://raw.githubusercontent.com/GNOME/gtk/3.24.49/gtk/gtkwidget.c (14093–14114,14198–14208)

## Narrow production change

Only gimpframe.c changes. Construct the same full-range singleton bold list and
call gtk_label_set_attributes only if it differs in contents/ranges. Differing
lists are still fully replaced, including removal of extra attributes. No global
utility behavior changes. Existing label-bold behavior is intentionally retained:
the old implementation reads this style property but always uses bold; correcting
that separate behavior is not bundled here.

Font/indent cache invalidation, style lookup, label replacement, alignment and
margin recomputation remain unchanged. There is no deadline, warmup, sanitizer,
frame timing, hidden-widget or application startup change.

pango_attr_list_equal exists since1.46; repository minimum is1.50. Its comparison
checks list cardinality, attribute values and ranges; the desired list has exactly
one attribute, so the documented general duplicate-list matching caveat does not
permit a multi-attribute current list to compare equal here.
- https://docs.gtk.org/Pango/method.AttrList.equal.html
- https://raw.githubusercontent.com/GNOME/pango/main/pango/pango-attributes.c

## Seven native regression cases, not yet executed

1. Three unchanged style-updated emissions: zero attributes notifications,
   identical retained attribute-list and cached-label-layout objects.
2. Different weight plus another attribute: exactly one replacement/notification,
   complete full-range bold list, layout invalidated, then idempotent thereafter.
3. Same bold weight plus extra attribute: extra attribute is still removed.
4. Partial-range bold: replaced with full-range bold.
5. Equal contents in a distinct externally supplied list: unchanged after style.
6. Replacement GtkBin-wrapped label: receives bold, then is idempotent.
7. Actual CSS changes from explicit12px/spacing7 to24px/spacing19: expected font
   indent and margin-top recomputed at both stages. No ambient-font assumption.

Held refs prevent allocator reuse from hiding list/layout replacement. Tests emit
style-updated synchronously to isolate this production setter behavior; this is
not a substitute for the unchanged native Brush acceptance run or frame deadline.

## Old-RED / new-GREEN execution plan

- Old production frame file is exact C14 gimpframe.c SHA256
  d158f0367d7c83540c49d4b997e1f3a99466766f5d7c0fb913d1989b10d47997.
- Build the SAME regression source against exact old production in a separately
  sealed control build; do not mutate the acceptance candidate/build in place.
  Existing isolated old-provider machinery may be reused only with verified
  source/binary/provider identity, otherwise use a separate C14 worktree.
- Run only /gimp-frame-style/unchanged on the old provider. Expected RED is the
  specific zero-notification assertion failing (old setter emits notifications).
  Compile/link errors, crashes elsewhere, display absence77 or timeouts are NOT
  successful RED evidence.
- New provider must pass all EXACT7 named tests, exit0, no skip/77, under existing
  hosted display and strict sanitizer settings. A registered Meson test alone may
  accept77 as skip; the hosted evidence gate must explicitly reject it.
- Then run the original unchanged Brush acceptance, including private-target
  case14. The targeted optimization is not declared to cure the whole delay.

Native GTK development metadata is unavailable locally (native-prerequisite.log).
No compilation, GTK execution, old-RED or new-GREEN result is claimed. No tools
were installed, denied ptrace reproduction was not retried, no remote changes or
main/protected artifact edits were made. meson.append is integration guidance,
not an edit to the frozen candidate.

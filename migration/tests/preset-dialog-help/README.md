# One-field help successor semantic audit
Original layer-preset-gui.cpp:197 sets actual dockable help_id="". Current native
DOCKABLE registration previously changed it to GIMP_HELP_DOCK; restored empty.
No preference GtkStack page key changes: preferences-dialog.c uses its explicit
stable gimp-prefs-folders-layer-presets identity independently.

Native gimp_dockable_new requires help_id != NULL, accepts empty, duplicates it,
and gimp_help_set_help_data stores its nonNULL pointer in GIMP_HELP_ID qdata.
Context help lookup tests pointer truth, not strlen, so empty stops ancestor-topic
fallback. Dockbook help calls gimp_standard_help_func with current dockable's
stored string; only no selected dockable uses GIMP_HELP_DOCK. gimp_help_show
normalizes zero-length help_id to NULL in its zeroed IdleHelp before scheduling.
Thus this restores absence of a specific topic, not a disabled help action and
not existence of any installed page. The separate dialog action retains original
gimp-layer-preset-dialog. No browser/plugin launch or installed document claim.

Causal UI assertions check actual registered entry, constructed dockable getter
and nonNULL empty widget qdata through factory/action open-close-reopen. Existing
actual UI cases exercise unchanged rendering and navigation. This metadata-only
change does not alter visible layout, so no fabricated new help-page image.

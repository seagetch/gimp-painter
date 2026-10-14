# Rows 19–20: checked owning editor-dropdown operations

Public base 69e5994e7f297d7b21b4643eb4fb2dd5161f7d23. Legacy pin
 afa43fae3e920210146abed514f136fd49f671b5, app/widgets/gimpeditor-cxx.cpp:89–178
and :182–269. The original generic and action C providers remain the sole
implementations; their source and signatures are unchanged in this subset.

EditorRef owns a checked native GimpEditor reference. Its two named methods call
the corresponding C provider once and retain the returned already-parented,
nonfloating transfer-none primary button. They never adopt the parent's reference
or create a second model/store. Callback data remains arbitrary borrowed gpointer.
The ignored list_id, primary/arrow roles, generic callbacks, forced normal primary
shape for toggle actions, alternate ordering and arrow sensitivity remain native.

Action alternatives are alternating gchar*/const gchar* names and GdkModifierType
masks. The C++14 method checks those exact decayed types and appends a typed NULL
name sentinel. It rejects malformed pairs and caller-supplied sentinel values.
Implicitly convertible classes are rejected: user-defined conversions do not
happen when forwarding their objects through C ellipsis. Input strings remain
borrowed for the synchronous native construction call.

A narrow shared native action-button repair conditionally escapes nullable main
and alternate action tooltips. Four baseline tests independently reproduced the
GLib fatal diagnostic for NULL metadata in dropdown and ordinary-action routes.
The repair changes no action identity, ordering, activation or sensitivity rules.

These are GUI-thread operations on live GTK editors. Owning an object reference
does not keep it operational after gtk_widget_destroy. Tests distinguish disposal
weak notifications from remaining strong storage references. An already-destroyed
editor's action route returns empty after its manager is cleared; generic
reconstruction on such an editor and arbitrary off-thread GTK calls are outside
this live-widget contract. No general GTK thread-safety or resurrection claim.

This completes only the two callable-provider named-operation routes. The two
legacy gimpitemtreeview.c callers (:426 and :462) were inactive #else branches;
current ordinary production UI does not call these providers. No new toolbar or
full historical GTK layout/visual ABI compatibility is claimed. The other 74
inventory rows, separate 31 hooks, type/handle catalog, overall 141/661 and task
08.018 remain unchanged. Row 50 live identity, ImageGenerator prerequisites and
the blocked perspective guard remain outside this subset.

## Final acceptance

78 native and 78 ASan+UBSan cases passed: 56 editor-dropdown C++14 cases and 22
actual-C11 provider/control cases. All original 33 and 2 cases are preserved.
Every selected process and X server exits normally, without skip or timeout.
The four NULL-tooltip regressions first fail independently against the unchanged
baseline, then pass through repaired dropdown and ordinary-action paths.

375 reviewed C11/C++14 public-header probes pass. The new handle separately
passes double-include compilation, four valid usage probes and eight malformed-
pack rejection probes. Source gate, 48 policy cases, 90-contract/76-route checks,
strict evidence verification and 12 corrupted-report controls pass. Independent
production and test-source review found no remaining blocking issue.

Reproduce using the established isolated native and address,undefined builds:

    meson compile -C BUILD -j2 editor-dropdown editor-dropdown-preset-control
    meson test -C BUILD --no-rebuild --num-processes 1 --print-errorlogs editor-dropdown editor-dropdown-preset-control

No unrelated MyPaint acceptance suite was repeated. See report.json for source,
binary and raw evidence hashes, baseline failures and explicit scope limits.

# MyPaint preset startup and Reset disposition

Base: 79d36312c27a6a8c866d17a87beb53fae05127ff.
Legacy source: afa43fae3e920210146abed514f136fd49f671b5.

The exact directly affected legacy rows remain
`gimp_mypaint_options_get_current_brush` and `gimp_mypaint_brush_duplicate`.
This closes the evidenced loss of the selected preset's full working settings
and stored icon in the startup-to-draft-to-Save flow. It does not restore those
old borrowed-object exports or declare either entire row complete.

Legacy selection duplicates the selected brush into the working draft
(app/paint/gimpmypaintoptions.cpp:389-403); the getter returns that draft
(:533-535). Duplicate copies settings, curves, text, group, parent metadata and
stored icon (app/core/gimpmypaintbrush.cpp:350-359,781-799). The name-edit and Save
callers operate on the working draft, and Save duplicates it before writing
(app/tools/gimpmypaintoptions-gui.cpp:165-195). History owns the outgoing draft
(app/paint/gimpmypaintoptions-history.cpp:62-72), so its icon snapshot is distinct
from the selected source's current icon.

Legacy GimpToolOptions Reset changes scalar/bool/text properties of that existing
draft; it does not replace the private icon, mappings or unrelated metadata.
The modern synthetic painter-settings property's full-document default was an
additional destructive reset operation. NULL now means no serialized property
override, while direct C set_json(NULL) still fails and valid full JSON remains
an exact replacement. Serialization reads the actual non-NULL full draft, so the
NULL default does not omit intended JSON. Explicit iconless JSON stays iconless.

Native startup loads factories before GUI restore. gimp_tools_restore resets
every tool, and previously left the Painter model reduced to generic defaults.
The already-selected resource is a deliberate native same-pointer no-op, so
opening the tool did not reload it. A dedicated startup-only C boundary now
reseeds selected settings after this reset without selecting or adding history;
later saved tool-options deserialization wins. User Reset retains its separate
scalar-reset behavior. Existing named shape/paper bidirectional binding behavior
is unchanged and remains the limit described in report.json.

The original normal-app failure had 43 charcoal settings and three mapped
settings in the source, but only one setting and no mappings after Save. The
candidate preserves the complete original JSON except the user's name/opacity
edits and the added embedded PNG representation. Decoded saved icon pixels equal
the unchanged original sidecar. These facts supplement, not replace, native and
sanitizer controls for custom draft icons, intentional clearing, serialization,
copy/history, source independence and failed Save retention.

Stored icons and rendered stroke previews are separate. Legacy stored-icon
preview scaling was disabled at gimpmypaintbrush.cpp:587-622; its active preview
renders a stroke. This change does not claim old/current preview pixel parity.
Generic native copy still shares mutable GdkPixbuf storage; that independent
legacy duplicate ownership obligation remains open.

The original 76 C entries and 31 core hooks remain separate. Completion counts
are unchanged; 08.018 is open.

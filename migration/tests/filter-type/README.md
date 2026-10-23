# Original WBS 08.003: FilterLayer native GType lifecycle

The existing native G_DEFINE_TYPE_WITH_CODE derives FilterLayer directly from
GimpLayer and implements GimpPickable and GimpProgress. The C instance/class embed
their native parents; a C boolean records inert construction. Instance init adds
FilterSlot to the common BindingStore inside the exception boundary. Constructed
chains the inherited native implementation, validates the slot and activates
the store. Native factory setup uses the same slot for image/topology observers.
This is the same ownership and admission contract used by CloneLayer.

Dispose closes the store before chaining to the native parent. FilterImpl close
cancels/detaches scheduler work, closes pending owner dispatch and connections,
and releases preparation state. Store storage survives close until native object
destruction, where its qdata notifier deletes the C++ state. Workers own detached
job data and close does not wait for them. Parent cleanup releases drawable/item
resources. No second Impl registry or old GIMP runtime is involved.

The added C test checks direct parent, actual registered instance/class sizes,
both interfaces, native factory type, inherited name/size/opacity, a GEGL buffer
and zero executor starts. A native C descendant chains its finalize override to
the real parent. Normal last-unref and repeated g_object_run_dispose each lead
to exactly one finalize and removal of the item ID. Before final unref, repeated
dispose leaves finalize count zero, scheduler state CLOSED, Progress inactive,
and definition mutation rejected with the closed error. Weak notification is
kept distinct from finalization. The image property required by GimpItem is set.

The new case and 12 selected native regressions pass with 21 unchanged source
seals. These include C/C++ layout and typed handles, partial duplicate failure,
Progress ABI, ordinary processing and completed-buffer-only access, removal and
Undo, copied definitions, weak object/array arguments, preparation close, actual
image close during a running worker, and retained handles after image shutdown.
The latter cases observe actual owner-thread destruction using an unmodified
qdata notifier, drain tracked jobs to zero and check absence of late updates.
This is a selected 13-case run, not the complete Filter suite or a sanitizer run.

No production lifecycle defect was found; the new test supplies the missing
direct native type and repeated-dispose/finalize assertions. Both exact legacy
whole-file blobs are preserved and independently matched. Their two assigned
08.003 obligations concern the type skeleton; other behavior in those files
retains its independent WBS obligations. Earlier test reports keep their original
source seals, including the startup snapshot before this fixture was added.
No current cross-platform, complete Filter implementation or release is claimed.

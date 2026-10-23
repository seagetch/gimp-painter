# Native Undo scalar and preferences metadata review

The unchanged adapter gate found ten unreviewed fingerprints (14 calls) and two
superseded preferences call fingerprints in committed171d467c21. These are not
permission to register a new C++ implementation outside BindingStore.

- Eight Undo fingerprints (12 calls) store/read only boolean pointer constants.
  `publishing` prevents nested publication and is cleared on rejection or entry
  into common publication. `published` prevents reuse of the consumed entry;
  it does not mean the entry survives notifications (use the explicit outcome).
  `pop-active` rejects nested transitions and is cleared after reverse guard
  finish, before releasing the retained image. There is no allocated payload or
  destroy callback in these keys. Typed stroke state remains in BindingStore.
- Two preferences fingerprints replace upstream field spellings. Their values
  are static property-name literals or NULL, not pointers to an automatic
  descriptor. Both `paths[]` and `layer_preset_path` are static const. The native
  reset callback synchronously reads these strings. No new owner/registry.

Exact paths, normalized calls and multiplicity are approved; no blanket native
C exemption and no checker semantic changes. Controls reject an extra call,
changed key and missing review for every new fingerprint. Two superseded exact
call-token approvals are removed, not left as stale exceptions.

Runtime evidence is explicitly reused, hash-bound, not rerun or relabeled:
Filter policy's prepared18/Filter3 scoped sanitizer, typed bridge's ordinary
prepared18/guard11 and native16/ASan16, and preset metadata's real GTK/portrait
checkpoint. The source gate uses an exact committed source archive plus only
this policy overlay, excluding all four held candidate paths. It is **not** a
whole-HEAD runtime, old application or remaining cleanup acceptance claim.

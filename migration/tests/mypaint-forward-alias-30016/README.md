# MyPaint options opaque source name — bounded 30.016 child

Original hunk001006 adds the `GimpMypaintOptions` forward name to paint-types.h.
Current operation compatibility already aliases this name to the sole native
`GimpPainterMybrushOptions` in gimpmypaintoptions.h. Adding the matching opaque
native-tag typedef to paint-types.h makes pointer-only consumers compile without
requiring the complete options shim or another model. The existing shim is not
changed. Original hunk metadata is preserved verbatim in original-duty.json.

18 isolated syntax checks cover C11/C++14, opaque-only use and both double-include
orders, with the actual pre-change header and an intentionally wrong-tag mutant.
12 compile successfully and six fail as required: old opaque-only lacks the name,
and the wrong tag conflicts with the real native operation alias. All423 existing
public C/C++ header probes and reviewed-route checks pass; no extra route or
immutable inventory rewrite is needed because paint-types.h is in the closure.
The initial fixture lacked the shim's relative native-header search directory;
that harness failure is retained separately, not called a production failure.

This is pointer/source-name compatibility only. The old `_GimpMypaintOptions`
struct tag, binary layout, runtime GType, constructor and property semantics are
not restored or claimed. Those duties remain under08.018 and original30.016;
this small child does not close either parent or change their acceptance text.
No runtime, sanitizer, drawing or new UI test is claimed for a forward typedef.
Header build uses the clean private checkpoint plus prior committed owned
changes, not the entire concurrently modified branch. Compilation has noRTTI.

Run `python3 migration/tests/mypaint-forward-alias-30016/validate.py --check-source`.

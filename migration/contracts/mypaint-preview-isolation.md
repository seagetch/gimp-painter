# Private preview resource snapshots

`GimpResources::Purpose::Preview` resolves the same preview-specific named
resources as before, then owns deep native duplicates of a built-in brush,
generated brush or brush pipe (including every child mask) and paper. Copies are
made at construction or explicit replacement, before the first lazy dab. Source
dirty signals cannot change a running preview, and source resources can finalize
while its private snapshots remain usable. Ordinary Stroke resource ownership,
dirty invalidation and native selection are unchanged.

Every new preview brush snapshot resets its private pipe indices to zero and
uses a private `GRand` seeded with 12345. Native `gimp_brush_select_brush` still
evaluates all eight modes. For each random dimension only, the private RNG
chooses the index and that private dimension is temporarily constant during the
native call; RAII restores the original mode. No original pipe index/current
pointer/use count, child, paper or process-global random stream is changed.
Preview reset is deliberately canonical, independent of how far the real brush
was used previously. It is a preview policy, not a change to active painting.

Unknown virtual brush/paper subclasses are explicitly refused because their
duplicate/selector isolation is not established. Invalid pipes, empty children,
unrepresentable native integer indices and excessive nested pipes are rejected
before native duplication/selection. The current built-in file-backed resource
types remain supported; this does not silently replace an unsupported selector.

Four synthetic native GIMP tests pass normally and under focused 28-source
ASan/UBSan/float-cast-overflow, with RTTI/vptr enabled and LeakSanitizer disabled:

- All eight pipe modes render byte-identically to controlled native selection;
  repeated previews ignore original selection state and leave global RNG intact
- Deep child/paper snapshots survive later source mutation and finalization
- Active Stroke mode still updates the real native pipe with balanced use counts
- Unisolated subclasses, invalid ranks and overflowing indices are rejected

The nine Surface cases and independent old active-session 129-record comparison
remain passing. `mypaint-preview-native.json` and
`mypaint-preview-sanitizers.json` record results and the exact instrumented
source/executable hashes. Remaining GIMP/dependency code is uninstrumented;
private thin archives leave normal production objects untouched. The initial
test-fixture teardown mistakenly unreferenced the application after `gimp_exit`
had already released it; the fixture now follows existing native test teardown.

This proves resource isolation and controlled native selector behavior, not the
old full preview stimulus/output. The shared editor's actual old spiral preview
and independent `get_new_preview` oracle are a separate gate. Active brush-pipe
sequence parity and inactive-hover adapter sequencing also remain separate.

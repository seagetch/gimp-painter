# Original preset action-group entry is a no-op

The pinned legacy `PresetGuiFactory::action_group_entry_point` iterates its
registry with an empty lambda. It does **not** add the modern Apply/Refresh
actions. The original `void preset_factory_gui_action_group_entry_point
(GimpActionGroup *)` now exists at the original include path, preserving that
empty operation for NULL, empty/nonempty, repeated and disposed group calls.
It does not dereference the argument or create a legacy singleton. Existing
`layer_presets_actions_setup` remains the separate native functionality.

ActionGroupRef uses the shared ObjectRef and lazy native TypeTraits, with
adopt/retain/copy/reset and a named method directly invoking the sole C entry.
No additional group, state store or implementation backend is created. Native
weak finalization and wrong-type rejection are tested.

## Evidence

* Clean source from committed baseline in build-policy.json, pinned gimp-data,
  fresh independent build/profile/prefix and read-only shared dependencies.
  No blocked candidate sources/objects reused. O2, debug=false, assertions on;
  compiler command audit excludes debug and assert-disable flags.
* Actual baseline linker expected-red: original symbol absent. Owned provider
  overlay then passes both native groups in the original 60-second budget.
* Both groups pass ASan/UBSan/float-cast with five selected units instrumented;
  no-RTTI ABI preserved, vptr and LSan off, dependencies uninstrumented.
* Native behavioral mutation replacing the empty operation with native action
  registration fails the unchanged empty-group assertion. Mutation uses a
  private source/object/thin archive/executable, not modified production files.
* 419 C11/C++14 header probes and additive-route negative controls pass.

Separate existing clean GUI regression is not evidence of a visual effect from
this no-op API: perspective24 passes with real oncanvas portrait capture, while
layer-presets initially passed4 groups then failed its gesture fixture hit-test
before event dispatch. That unmodified failure is retained here; portrait fixture
repair/remaining UI acceptance is a separate task, not hidden by this closure.

Original dialogs-actions and preference entry routes remain future work. No
historical C++ Interface, dynamic registry lifecycle, binary class/subclass ABI,
or complete legacy GUI parity is claimed. Parent08.018 remains open.

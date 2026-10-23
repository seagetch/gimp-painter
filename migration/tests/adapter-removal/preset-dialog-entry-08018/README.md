# Original preset dialog-action entry: one native descriptor

`preset_factory_gui_dialogs_actions_entry_point(GimpActionGroup *)` is restored
with its exact original void C signature. It selects the existing
`dialogs_dockable_actions` layer-preset descriptor and invokes the existing
native string-action registration provider once, with its real dockable callback.
The ordinary complete dialogs setup and its order are untouched. There is no
second descriptor, dynamic singleton registry, action backend or C++ placement.
ActionGroupRef's named operation calls the sole C provider directly.

An owner lease spans native action-added observers. Native C tests exercise
last-owner release and disposal during registration. NULL is diagnosed before
native dereference; native duplicate registration retains the existing action.
This does not assert general post-dispose action-group reinitialization semantics.

## Compatibility and evidence boundaries

Native label/short label/help metadata intentionally stays unchanged and differs
from legacy, so the original dialog row remains PARTIAL for historical semantics.
The legacy dynamic registry, C++ Interface, old binary/subclass layout, and final
preferences entry are not restored by this task. The previous original empty
action-group operation stays empty, not an alias to modern Apply/Refresh actions.

* Clean normal4 and selected6-unit ASan/UBSan/float-cast4 pass. Assertions remain
  enabled; no-RTTI ABI retained, vptr/LSan off, dependencies uninstrumented.
* Actual baseline link fails for the missing original dialog symbol, then passes
  with only the owned provider overlay. A native wrong-dialog descriptor mutation
  fails the unchanged type/identity assertion. An initial mutation selecting a
  keyboard-accelerated brush descriptor failed an unrelated headless GTK
  prerequisite and is explicitly excluded, not counted as behavioral evidence.
* All419 C11/C++14 probes and additional-route controls pass.
* Clean UI17 (all prior16 plus original registration/activation/open/close) and
  perspective24 pass on authenticated1080x1920x24 with oncanvas enabled.
* The new image is the real app after the original C entry and native GAction
  activation produced a visible preset dock, with native close/weak destruction
  verified. It is not a pointer-click screenshot; the prior16 queued gesture
  cases remain separate passing coverage. Window1040x1800 is evidence display,
  not a geometry workaround before interactions.

The prior no-op evidence is immutable. The successor validator replays its
original receipt/archive **without** matching old sources to the newer headers;
then it binds this new combined no-op/dialog test receipt to current sources.
No old receipt hash is resealed to make a changed implementation appear tested.

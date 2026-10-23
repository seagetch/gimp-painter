# Borrowed registry iteration review

The retirement loop borrows the global registered tool-info list. This is not a
general guarantee against arbitrary injected callbacks mutating that list.
Inspection of all 22 non-test `tool_info_list` references found registration adds
only in `gimp_tools_register` (startup) and disposal/clear only in Gimp teardown
(`app/core/gimp.c`). Normal reset/deserialization changes `tool_item_list` and
child group containers, not the registered tool-info list. `gimp_tools_exit`
also only traverses the registry; it does not remove its members.

The concrete synchronous paths caused by child removal were traced:

- `gimp_tool_group_child_remove` clears the parent and updates active selection.
- Parent change emits `ancestry-changed`; `tool_manager_tool_ancestry_changed`
  updates the active group and its signal binding, not the registry.
- `tool_manager_group_active_tool_changed` updates the context's selected tool.
- `gimp_tool_button_tool_remove` removes a menu entry and updates the button;
  `gimp_tool_button_active_tool_changed` updates the button.
- Canvas container/group observers schedule a generation-checked idle rebuild;
  they do not remove registered tool infos.

No reachable native observer that mutates the registry was found. Therefore this
bounded repair retains existing registry-iteration semantics instead of adding a
speculative mutation behavior or weakening tests. Parent lifetime is different:
old UI references can genuinely be dropped during synchronous notifications, so
the retiring parent is explicitly leased. The actual registered UI observers run
in the 24-case native and focused sanitizer receipts. No arbitrary callback-driven
registry mutation or application-destruction safety is claimed by these receipts.

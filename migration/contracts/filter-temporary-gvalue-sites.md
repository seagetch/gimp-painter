# Filter PDB temporary GValue sites (15.001/pdb-temporary-gvalue)

This is an ownership/source reconciliation, not completion of the real
15.001 prerequisite or arbitrary legacy PDB execution. Original criterion:
`変換用一時値を全経路で unset し、引数配列との所有権を分ける`.
No old runtime, old Edge/string runtime, or new runtime test is claimed here.

## Complete initialized temporary census in the pinned helper

Source: `afa43fae:app/pdb/pdb-cxx-utils.hpp` (entire file inspected).
There are exactly two local initialized conversion GValues, both used only to
print diagnostic strings. Neither is part of the saved argument array.

| Original site | Original duty / defect | Current mapping |
|---|---|---|
| `ProcedureRunnerImpl::setup_args`,177–181,`v_str` | Default-value-to-string debug transform; initialized string never unset | Diagnostic conversion removed. Checked admitted preparation stages scalars in `std::array<Value,...>`, makes one scoped `Value` destination per slot, and copies into independently owned `ArgsRef`. |
| `ProcedureRunnerImpl::get_arg`,263–267,`b` | Argument-to-string debug transform; uninitialized shell then init; string never unset; returned value is a different borrowed-by-value argument | No diagnostic temporary and no by-value owning alias returned. `gimp_filter_layer_dup_args` materializes an independent owned native array. `gimp_filter_layer_get_procedure_args` deep-copies initialized entries into a GArray with per-element unset callback and RAII cleanup of the intermediate native array. |

`set_arg` at275–279 transforms directly into an existing array element: it has
no initialized conversion temporary and is not a third site. `get_args` copies
the native array; its old intermediate-container leak is mapped independently
in `adapter-removal/filter-entry-handles-08018/README.md`, not confused with the
two diagnostic values above.

## Current ownership paths

- `app/painter/resources.hpp::Value`: zero-initialized shell; destructor calls
  reset; reset empties the owned shell before unsetting its former payload.
  Copy is independent `g_value_copy`; moves transfer payload. Normal return and
  C++ exception unwinding share the same destructor, including failed transform
  and partial preparation. `ArgsRef` owns its native array separately.
- `app/core/gimpfilterlayer.cpp::prepare_legacy_scalar_route`: all consumed
  caller scalars are staged before any potentially reentrant transform; each
  temporary destination is scoped; append copies its value into the output.
  A thrown validation/transform error destroys already-created staged values,
  destination and partial result. Caller array remains caller-owned.
- `gimp_filter_layer_get_procedure_args`: `ArgsRef values` owns materialization;
  `ArrayRef result` owns partially initialized zero-filled entries, and its
  clear callback unsets only initialized values. Success transfers the output
  array, not either intermediate owner.
- `gimp_filter_arguments_snapshot_value` is an explicitly caller-owned output
  GValue, not an internal temporary. The caller must provide an empty shell and
  unset a successful output. `snapshot_peek_value` is a const borrowed view;
  neither transfers the snapshot's saved payload.

## Existing independent evidence (not rerun in this audit)

`app/painter/tests/test-resources.cpp::value_ownership` covers stack/heap shells,
borrow versus copy, object finalization counts, reset idempotence, independent
strings and counted boxed copy/free ownership. Filter C-entry fixture
`c_owned_values_and_model_identity` covers12 rich slots, independent free order,
caller/result mutations and unchanged saved identity; see
`migration/tests/adapter-removal/filter-entry-handles-08018/`.
Checked preparation validation, partial conversion failure and callback
replacement/closure receipts are documented in
`migration/contracts/filter-legacy-procedure-entry.md` and its additive
Small Tiles/Retinex contracts. These are bounded admitted routes, not an assertion
that arbitrary old procedure execution or default/context semantics were ported.

No additional implementation is inferred necessary merely because old debug
conversions are absent. The task remains subject to its unchanged prerequisites
`15.001, 01.007/gvalue`; the broader section16/17 procedure compatibility duties
and paused runtime scopes remain separately represented.

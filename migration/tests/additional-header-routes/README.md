# Additive reviewed C-header routes

`migration/inventory/additional-c-header-routes.json` extends the registered
C11/C++14 header compile probes without editing the historical inventories.
The original 103 routes, 102 public targets, legacy assignments, 11 C++ mappings,
21 private headers (19 without HTTP), 63 generated headers and baseline source
identities retain their existing checks. `public_targets` retains its historical
meaning; `additional_public_targets` and `additional_reviewed_routes` identify
the separately reviewed extension. Historical probe names are unchanged.

Every extension is explicit, has a review description, and compiles twice-included
in both languages. `kind: new` admits only genuinely new production app `.h`
files. `kind: changed-baseline` admits only previously unregistered baseline
headers and must repeat the exact `baseline_blob` from the historical inventory.
An additional route cannot relabel a baseline header as new or replace an
existing historical compile route. Missing routes still fail the source scan.

`context: core` uses the existing type prelude with its outer C-linkage adapter
removed. `context: standalone` supplies no prelude. Ordered `prerequisites` may
name only existing historically covered headers or explicitly registered
extension targets. They cannot silently expand the reviewed closure.

The current routes cover the new popup renderer header and the changed native
tree public/private headers. The private layout is tested after its public
header. The old inventories, source duties and historical evidence reports are
unchanged. The dropdown candidate's headers are not registered until that
source is integrated and reviewed.

The inventory is a Meson configure input, and its headers are custom-target
inputs and source-hashed manifest inputs. Run the registered tests:

```sh
meson test -C BUILD painter-additional-header-routes painter-public-header-compilation --print-errorlogs
```

The first test exercises isolated positive/negative route controls and unchanged
historical inventory seals. The second builds and verifies the actual C/C++
archives using Meson's selected toolchain. Source or metadata drift invalidates
the recorded manifest. These probes establish compilation only; they do not
claim runtime behavior, C/C++ link ABI, platform coverage or original 08.017
behavioral acceptance.

## Tool-options route repair (2026-10-15)

A fresh native configure at `6e6f2568e7593089c4f3c93e4613354c1f915001`
correctly rejected changed baseline tool-option headers outside the reviewed
closure. Child `04.003/tool-options-header-routes` explicitly registers
`gimpalignoptions.h`, `gimprectangleoptions.h`, and `gimpvectoroptions.h` with
their original baseline blob identities and the normal core/tool/GTK prelude.
Their added horizontal constructors, action/metadata/history state and path
availability declarations require no extra prerequisite header. The generator
and historical inventories are unchanged. A regression removes each route in
turn and requires the fail-closed source scan to reject it. As with other
routes, double-include compilation is not a cross-language link ABI claim.

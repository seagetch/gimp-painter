# Channel representation absence and independent alpha

Bounded12.015/field-owner-checkpoint, still DOING. Sixteen subsets of legacy
OPACITY/COLOR and modern FLOAT_OPACITY/FLOAT_COLOR are opened, saved and reopened.
The fixture replaces absent recognized tags with distinct unknown tags of the
same size, preserving payloads and all object offsets; it does not truncate data.
Expected defaults are black/opaque, with modern fields overriding their byte
counterparts when present. Byte RGB/alpha expectations come from actual writer
payloads. Native resave introduces float opacity, so the second round explicitly
expects its representable float value rather than pretending double wire storage.

The genuine red fails subset5: OPACITY94/255 followed by COLOR changes alpha from
0.36862745098039218 to0.36862745881080627. The generic channel RGBA getter converts
through float. The unchanged old loader sets double alpha94/255; its COLOR setter
changes onlyRGB. Those exact source files are pinned here (source algebra, not
old application execution). The narrow native loader fix reads existing alpha
in explicit sRGB-double format for COLOR/FLOAT_COLOR; no generic getter or
interactive channel operation changes. Original RGB2e-7/alpha1e-12 bounds stay.

The new16×2 case passes normal and focused ASan/UBSan. All three relevant channel
groups (six valid order permutations,40 declared-length conditions,16 absence
subsets) additionally pass in separate exact-path normal and sanitizer processes,
one mandatory TAP case each, under60s. A mistaken component-prefix selection ran
zero tests; its receipt is preserved and rejected, not counted as evidence.
The previous full98+98 checkpoint remains immutable; this is not a full current
99-case run. Instrumentation41 units plus40 RTTI-only; dependencies/whole app and
LSan are not covered. Source seal limitations are explicit in report.json.

A clean committed021603382c source overlay containing only this loader change
passes the persistence gate:1378 records,873 candidates,16 routes,55 rejection
controls. Paused UI candidate changes are excluded. The original185-field ledger
is unchanged; malformed truncation, all owners/graphs/platforms remain open.
No new UI or physical input claim.

# Empty-canvas grab cleanup: ownership diagnostic (not a fix)

The already committed compression lease fix is independent. Original event-compression-order remains open.

Host is sealed `55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94`, with unchanged product libraries. Its `gimpdisplayshell-grab.c` is byte-identical to the current committed provider. Only the private fixture is new. Actual authenticated Xvfb root is asserted 1080×1920; oncanvas UI explicitly enabled/asserted on both real, mapped Gimp shells. This is synthetic queued input, not physical tablet or Windows evidence.

## Causal sequence
1. First real shell receives native button press and acquires pointer seat.
2. Its image is detached through `gimp_display_set_image`; shell/canvas remain mapped and its grab bookkeeping remains populated.
3. Queue its initiating button release with `gdk_event_put`.
4. Before dispatch, a distinct real second shell successfully acquires the same seat via the existing native pointer-grab helper.
5. Dispatch through GDK and GTK. An emission observer confirms the old release reaches the first actual canvas once.
6. Baseline preserves the actual second grab (`gdk_display_device_is_grabbed` true), but old bookkeeping is stale. No old broken-grab event preceded release.
7. Fixture-only naive variant invokes the existing old-shell pointer-ungrab at that same GTK release. It makes the actual second grab disappear, failing the public boolean query. It does not modify product source.

`baseline.json`: exit0; `naive.json`: exit250, assertion `gdk_display_device_is_grabbed(gdkdisplay,device)` should be TRUE. Actual linked input hashes were unchanged. `diagnostic-scope.json` corrects the inherited generic wording of the first baseline receipt: there are NO candidate provider overrides in this link.

## Boundary
Adding unconditional seat-ungrab to empty-canvas release, image disconnect, or disposal is unsafe. Merely processing queued broken-grab notifications is not sufficient for this ordering. The public boolean query proves this diagnostic but cannot identify current ownership in production. `gdk_device_grab_info_libgtk_only` is explicitly not an application ownership API. `GdkEventGrabBroken` carries old/new windows but no ownership generation/timestamp. A broader ownership design requires review; no such fix has been attempted here.

No claim is made that this bounded synthetic sequence exhausts backend ownership behavior. Missing theme/application icons and session-bus warnings remain in receipts.

## Timestamp boundary controls
Already prepared X11 controls were run before packaging: older release timestamp than replacement grab preserves the new grab; equal millisecond timestamp loses it. These use actual X11 server timestamps and deprecated public device-ungrab in fixture only. Equality is a real weakness even on X11, not just a cross-backend concern. Source audit shows Wayland marks latest grab ended without time comparison, and Win32 virtual ungrab ignores time and calls ReleaseCapture. Neither backend was executed. No shell grab acquisition timestamp currently exists; only the three borrowed seat/device fields. No product timestamp workaround was introduced.

Original acceptance remains unchanged/open: 旧compress_motionとnext_event経路を現行入力へ接続し、pen up・key・crossing・device切替が欠落せずイベント所有が一回解放される. This diagnostic does not complete lifecycle cleanup, dependency coverage, GTK internal free-count, physical tablet history, or cross-platform proof. No sanitizer was run for this diagnostic-only package.

Receipt versioning: baseline/naive use preserved v1 fixture and their own binary/runtime hashes. Timestamp controls use the final fixture and final compile/link manifest. No final link manifest is claimed for the older v1 binary; its runtime receipt pins the executed binary and dependencies.

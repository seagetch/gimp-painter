# Separate explicit-dispose renderer lifetime residual

A private standard paint-options instance initialized from the registered tool context is displayed by the exact independent-toolbar factory (the public dispatcher correctly rejects noncanonical options). With Texture and its real stock Pattern child open, forced `g_object_run_dispose(options)`, child cancellation and owner teardown crash both actual300 GtkPopover baseline and this native-popup candidate (SIGSEGV, runner245).

Matching fatal-signal-preserving diagnostic traces reach `gimp_view_renderer_set_context` (`app/widgets/gimpviewrenderer.c:297`), renderer dispose188 and view dispose205. The context disposal clears `context->gimp` before parent disposal; the same baseline fault remains a separate renderer/context lifetime follow-up. This is not labeled unsupported and is not a new native-popup regression or a passing acceptance case. No global renderer fix is included here.

An initial test invented immediate popup closure on forced dispose, but old source has no such policy. That assertion was removed rather than claimed as legacy behavior. Ordinary last-unref differs: controls/renderers retain their model/context. The final17 gate covers caller release, direct host destruction, owner unmap, real nested child teardown, reopening and eventual model weak finalization.

#!/usr/bin/env python3
"""Copy exact test sources to a separate diagnostic TU; fail closed on drift."""
import pathlib, sys

def once(text, old, new):
    if text.count(old) != 1: raise ValueError('Diagnostic overlay anchor is not unique')
    return text.replace(old, new)

# Exact substitutions in the isolated generated copy only. No primary edits.
BRUSH_PHASES = (
    ('brush_popup_private_target_release (void)\n{\n',
     'brush_popup_private_target_release (void)\n{\n  phase_observe_begin ();\n  phase_observe_mark (1);\n'),
    ('  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);',
     '  phase_observe_mark (2);\n  gimp_context_copy_properties (GIMP_CONTEXT (tool->tool_options), GIMP_CONTEXT (options), GIMP_CONTEXT_PROP_MASK_ALL);\n  phase_observe_mark (3);'),
    ('  GtkWidget *toolbar = factory (options); g_assert_nonnull (toolbar);',
     '  phase_observe_mark (4);\n  GtkWidget *toolbar = factory (options); g_assert_nonnull (toolbar);\n  phase_observe_mark (5);'),
    ('  gtk_container_add (GTK_CONTAINER (window), toolbar); gtk_widget_show_all (window); drain ();',
     '  gtk_container_add (GTK_CONTAINER (window), toolbar);\n  phase_observe_mark (6);\n  gtk_widget_show_all (window);\n  phase_observe_mark (7);\n  phase_observe_mark (8);\n  drain ();\n  phase_observe_mark (9);'),
)

def generate(source, output):
    source, output = pathlib.Path(source), pathlib.Path(output)
    main = (source/'app/tests/test-painter-compact-options.c').read_text()
    helper = (source/'app/tests/test-painter-transform-popup.inc').read_text()
    main = once(main, '#include "test-painter-transform-popup.inc"', '#include "readiness-owner-transform.inc"')
    main = once(main, '#include "test-painter-brush-thumbnail-popup.inc"', '#include "readiness-owner-brush.inc"')
    brush = (source/'app/tests/test-painter-brush-thumbnail-popup.inc').read_text()
    for old, new in BRUSH_PHASES:
        brush = once(brush, old, new)
    helper = '#include "readiness-owner-phase.inc"\n#include "readiness-owner-image.inc"\n#include "readiness-owner-icon.inc"\n#include "readiness-owner-leaf.inc"\n#include "readiness-owner-probe.inc"\n' + helper
    helper = once(helper, '  TransformPopupLayout layout = {0}; layout.button = button;',
                  '  phase_observe_mark (10);\n  owner_observe_begin (button, window);\n  image_observe_begin ();\n  icon_observe_begin ();\n  leaf_observe_begin (button, window);\n  TransformPopupLayout layout = {0}; layout.button = button;')
    helper = once(helper, '    g_main_context_iteration (NULL, FALSE);\n    longest_iteration',
                  '    owner_observe_dispatch (TRUE);\n    g_main_context_iteration (NULL, FALSE);\n    owner_observe_dispatch (FALSE);\n    longest_iteration')
    helper = once(helper, '  g_assert_cmpuint (layout.stable, >=, 3);',
                  '  phase_observe_mark (11);\n  phase_observe_mark (12);\n  phase_observe_finish ();\n  icon_observe_finish ();\n  image_observe_finish ();\n  leaf_observe_finish ();\n  owner_observe_finish ();\n  g_assert_cmpuint (layout.stable, >=, 3);')
    output.mkdir(parents=True, exist_ok=True)
    for name, text in [('readiness-owner-main.c',main),('readiness-owner-transform.inc',helper),
                       ('readiness-owner-probe.inc',(source/'tools/brush-ci/readiness-owner/probe.inc').read_text()),
                       ('readiness-owner-brush.inc',brush),
                       ('readiness-owner-phase.inc',(source/'tools/brush-ci/readiness-owner/phase.inc').read_text()),
                       ('readiness-owner-leaf.inc',(source/'tools/brush-ci/readiness-owner/leaf.inc').read_text()),
                       ('readiness-owner-image.inc',(source/'tools/brush-ci/readiness-owner/image.inc').read_text()),
                       ('readiness-owner-icon.inc',(source/'tools/brush-ci/readiness-owner/icon.inc').read_text())]:
        (output/name).write_text(text)

if __name__ == '__main__': generate(*sys.argv[1:])

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TEST_PAINTER_WIDGET_EXCEPTION_FAULT_H
#define TEST_PAINTER_WIDGET_EXCEPTION_FAULT_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum
{
  PAINTER_WIDGET_EXCEPTION_STANDARD = 1,
  PAINTER_WIDGET_EXCEPTION_UNKNOWN
} PainterWidgetException;

#define PAINTER_WIDGET_EXCEPTION_MESSAGE "Injected widget boundary failure"

/* Test-only, owner-specific, one-shot fault. No GTK callback may be dispatched
 * between arming and the direct C facade call under test. */
void     painter_widget_exception_fault_arm      (GObject                *owner,
                                                  PainterWidgetException  exception);
gboolean painter_widget_exception_fault_consumed (void);

G_END_DECLS

#endif

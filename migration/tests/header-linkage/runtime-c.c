/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "prelude.h"
#include "painter/gimp-painter-binding.h"
#include "painter/gimp-painter-error.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "display/gimpcanvasitem.h"
#include "probe-api.h"

void
painter_check_cpp_boundary_from_c (void)
{
  GObject *owner = g_object_new (G_TYPE_OBJECT, NULL);
  GError *error = NULL;
  g_assert_true (gimp_painter_binding_close (owner, &error));
  g_assert_no_error (error);
  g_object_unref (owner);
  g_assert_false (gimp_painter_binding_close (NULL, &error));
  g_assert_error (error, gimp_painter_error_quark (),
                  GIMP_PAINTER_ERROR_WRONG_TYPE);
  g_clear_error (&error);
}

void
painter_c_layout (size_t values[8])
{
  values[0] = sizeof (GimpContext);
  values[1] = offsetof (GimpContext, template);
  values[2] = sizeof (GimpDrawable);
  values[3] = offsetof (GimpDrawable, private);
  values[4] = sizeof (GimpCanvasItem);
  values[5] = offsetof (GimpCanvasItem, private);
  values[6] = sizeof (GimpContextClass);
  values[7] = offsetof (GimpContextClass, template_changed);
}

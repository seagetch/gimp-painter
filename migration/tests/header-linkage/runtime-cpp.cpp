/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "prelude.h"
#include "core/gimpcontext.h"
#include "core/gimpcoords.h"
#include "core/gimpdrawable.h"
#include "core/gimpobject.h"
#include "display/gimpcanvasitem.h"
#include "display/gimppainternavigation.h"
#include "probe-api.h"
#include "navigation-calls.inc"
#include <stdio.h>

int
main (void)
{
  for (size_t i = 0; i < painter_reference_count; ++i)
    {
      g_assert_true (painter_c_references[i] != nullptr);
      g_assert_true (painter_cpp_references[i] != nullptr);
      g_assert_true (painter_c_references[i] == painter_cpp_references[i]);
    }
  painter_check_navigation ();
  GimpCoords a = {};
  GimpCoords b = {};
  GimpCoords mixed = {};
  a.x = 2.0; a.y = 4.0; b.x = 6.0; b.y = 8.0;
  gimp_coords_mix (0.25, &a, 0.75, &b, &mixed);
  g_assert_cmpfloat (mixed.x, ==, 5.0);
  g_assert_cmpfloat (mixed.y, ==, 7.0);
  g_assert_true (g_type_is_a (gimp_object_get_type (), G_TYPE_OBJECT));
  painter_check_cpp_boundary_from_c ();
  size_t c_layout[8];
  painter_c_layout (c_layout);
  const size_t cpp_layout[8] = {
    sizeof (GimpContext), offsetof (GimpContext, template_object),
    sizeof (GimpDrawable), offsetof (GimpDrawable, priv),
    sizeof (GimpCanvasItem), offsetof (GimpCanvasItem, priv),
    sizeof (GimpContextClass), offsetof (GimpContextClass, template_changed)
  };
  for (size_t i = 0; i < G_N_ELEMENTS (c_layout); ++i)
    g_assert_cmpuint (c_layout[i], ==, cpp_layout[i]);
  printf ("PASS: %zu equal C/C++ reference addresses; navigation 7; "
          "coords/object calls; C-to-C++ success/error; layout 8\n",
          painter_reference_count);
  return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "core-types.h"
#include "gimpundopopguard.h"
G_DEFINE_INTERFACE (GimpUndoPopGuard, gimp_undo_pop_guard, GIMP_TYPE_UNDO)
static void gimp_undo_pop_guard_default_init (GimpUndoPopGuardInterface *iface) {}
gboolean
gimp_undo_pop_guard_prepare (GimpUndo *undo, GimpUndoMode mode)
{
  GimpUndoPopGuardInterface *iface;
  g_return_val_if_fail (GIMP_IS_UNDO (undo), FALSE);
  if (! GIMP_IS_UNDO_POP_GUARD (undo)) return TRUE;
  iface = GIMP_UNDO_POP_GUARD_GET_IFACE (undo);
  return iface->prepare && iface->finish && iface->prepare (undo, mode);
}
void
gimp_undo_pop_guard_finish (GimpUndo *undo, GimpUndoMode mode)
{
  GimpUndoPopGuardInterface *iface;
  g_return_if_fail (GIMP_IS_UNDO_POP_GUARD (undo));
  iface = GIMP_UNDO_POP_GUARD_GET_IFACE (undo);
  if (iface->finish) iface->finish (undo, mode);
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_UNDO_POP_GUARD_H
#define GIMP_UNDO_POP_GUARD_H
#include "gimpundo.h"
G_BEGIN_DECLS
#define GIMP_TYPE_UNDO_POP_GUARD (gimp_undo_pop_guard_get_type ())
#define GIMP_IS_UNDO_POP_GUARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_UNDO_POP_GUARD))
#define GIMP_UNDO_POP_GUARD_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GIMP_TYPE_UNDO_POP_GUARD, GimpUndoPopGuardInterface))
typedef struct _GimpUndoPopGuard GimpUndoPopGuard;
typedef struct _GimpUndoPopGuardInterface GimpUndoPopGuardInterface;
struct _GimpUndoPopGuardInterface
{
  GTypeInterface parent_iface;
  /* Opt-in native contract: prepare pins/locks state before stack mutation.
   * FALSE must leave no lease held. Every TRUE gets exactly one finish, in
   * reverse preparation order, including rejection of a sibling. No throwing
   * C++ callback is permitted through this C interface. */
  gboolean (*prepare) (GimpUndo *undo, GimpUndoMode mode);
  void (*finish) (GimpUndo *undo, GimpUndoMode mode);
};
GType gimp_undo_pop_guard_get_type (void) G_GNUC_CONST;
gboolean gimp_undo_pop_guard_prepare (GimpUndo *undo, GimpUndoMode mode);
void gimp_undo_pop_guard_finish (GimpUndo *undo, GimpUndoMode mode);
G_END_DECLS
#endif

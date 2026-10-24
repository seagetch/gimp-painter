/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __GIMP_IMAGE__UNDO_H__
#define __GIMP_IMAGE__UNDO_H__

G_BEGIN_DECLS


gboolean        gimp_image_undo_is_enabled      (GimpImage     *image);
gboolean        gimp_image_undo_enable          (GimpImage     *image);
gboolean        gimp_image_undo_disable         (GimpImage     *image);
gboolean        gimp_image_undo_freeze          (GimpImage     *image);
gboolean        gimp_image_undo_thaw            (GimpImage     *image);

/* Internal observer query: true throughout native Undo/Redo preflight and pop.
 * Distinguishes an Undo request from finishable normal save preflight. */
gboolean        gimp_image_undo_is_pop_active   (GimpImage     *image);

gboolean        gimp_image_undo                 (GimpImage     *image);
gboolean        gimp_image_redo                 (GimpImage     *image);

gboolean        gimp_image_strong_undo          (GimpImage     *image);
gboolean        gimp_image_strong_redo          (GimpImage     *image);

GimpUndoStack * gimp_image_get_undo_stack       (GimpImage     *image);
GimpUndoStack * gimp_image_get_redo_stack       (GimpImage     *image);

void            gimp_image_undo_free            (GimpImage     *image);

gint            gimp_image_get_undo_group_count (GimpImage     *image);
gboolean        gimp_image_undo_group_start     (GimpImage     *image,
                                                 GimpUndoType   undo_type,
                                                 const gchar   *name);
gboolean        gimp_image_undo_group_end       (GimpImage     *image);

GimpUndo      * gimp_image_undo_push            (GimpImage     *image,
                                                 GType          object_type,
                                                 GimpUndoType   undo_type,
                                                 const gchar   *name,
                                                 GimpDirtyMask  dirty_mask,
                                                 ...) G_GNUC_NULL_TERMINATED;

GimpUndo      * gimp_image_undo_can_compress    (GimpImage     *image,
                                                 GType          object_type,
                                                 GimpUndoType   undo_type);


/* Transfers one reference after valid image/type/owner/error/not-published
 * preconditions. Initialization failure or frozen history consumes it and
 * preserves dirty/history/redo state. Returns borrowed current entry or NULL
 * if rejected/immediately expired. Invalid arguments remain caller-owned. */
GimpUndo *      gimp_image_undo_push_prepared    (GimpImage     *image,
                                               GimpUndo      *undo,
                                               GError       **error);

/* Same transfer contract, with publication independent of entry survival.
 * TRUE means native stack insertion completed, even if observers immediately
 * expired the entry/image. FALSE is rejection before insertion. On FALSE only
 * helper-owned preflight failure is side-effect-free; arbitrary callbacks are
 * never rolled back. surviving_entry must be non-NULL and is borrowed/nullable. */
gboolean       gimp_image_undo_publish_prepared (GimpImage     *image,
                                               GimpUndo      *undo,
                                               GimpUndo     **surviving_entry,
                                               GError       **error);

G_END_DECLS

#endif /* __GIMP_IMAGE__UNDO_H__ */

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILE_SAVE_DEFERRED_H
#define GIMP_FILE_SAVE_DEFERRED_H
G_BEGIN_DECLS
/* Completion is delivered only while the originating UI context is alive.
 * destroy always releases completion_data, including cancellation/owner loss.
 * A queued request owns its exact file, procedure and save/export arguments. */
/* Internal synchronous attempt with an explicit BUSY result. */
gboolean file_save_dialog_save_image_pending (GimpProgress *progress,
                                              Gimp *gimp, GimpImage *image, GFile *file,
                                              GimpPlugInProcedure *procedure,
                                              GimpRunMode run_mode,
                                              gboolean change_saved_state,
                                              gboolean export_backward,
                                              gboolean export_forward,
                                              gboolean xcf_compression,
                                              gboolean verbose_cancel,
                                              gboolean *pending_paint);
typedef void (*FileSaveCompletion) (gboolean success, gpointer completion_data);
void file_save_dialog_save_image_async (GObject *owner,
                                        GimpProgress *progress,
                                        Gimp *gimp, GimpImage *image, GFile *file,
                                        GimpPlugInProcedure *procedure,
                                        GimpRunMode run_mode,
                                        gboolean change_saved_state,
                                        gboolean export_backward,
                                        gboolean export_forward,
                                        gboolean xcf_compression,
                                        gboolean verbose_cancel,
                                        FileSaveCompletion completion,
                                        gpointer completion_data,
                                        GDestroyNotify destroy);
G_END_DECLS
#endif

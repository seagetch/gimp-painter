/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpwidgets/gimpwidgets.h"
#include "actions-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimppaintermybrusheditor.h"
#include "mypaint-brush-editor-actions.h"
#include "gimp-intl.h"

static void
save_command (GimpAction *action, GVariant *value, gpointer data)
{
  GError *error = NULL;
  GimpContext *context;
  if (!GIMP_IS_PAINTER_MYBRUSH_EDITOR (data)) return;
  gimp_painter_mybrush_editor_save_command (GIMP_PAINTER_MYBRUSH_EDITOR (data),
      g_strcmp0 (gimp_action_get_name (action), "mypaint-brush-editor-save-as") == 0,
      &error);
  if (error)
    {
      context = gimp_action_get_context (action);
      if (context && context->gimp)
        gimp_message_literal (context->gimp, G_OBJECT (data), GIMP_MESSAGE_ERROR,
                               error->message);
      g_clear_error (&error);
    }
}
static const GimpActionEntry entries[] = {
  { "mypaint-brush-editor-save", GIMP_ICON_DOCUMENT_SAVE,
    NC_("mypaint-brush-editor-action", "_Save"), NULL, { NULL },
    NC_("mypaint-brush-editor-action", "Save the shared Painter brush draft"),
    NULL, NULL },
  { "mypaint-brush-editor-save-as", GIMP_ICON_DOCUMENT_SAVE_AS,
    NC_("mypaint-brush-editor-action", "Save _As…"), NULL, { NULL },
    NC_("mypaint-brush-editor-action", "Save the shared draft as a separate Painter brush"),
    NULL, NULL }
};
void mypaint_brush_editor_actions_setup (GimpActionGroup *group)
{
  gimp_action_group_add_actions (group, "mypaint-brush-editor-action", entries,
                                 G_N_ELEMENTS (entries));
  /* Factory caches do not own callback data. A retained action must disconnect
   * when its editor dies, rather than keeping a borrowed pointer callable. */
  if (GIMP_IS_PAINTER_MYBRUSH_EDITOR (group->user_data))
    for (guint i = 0; i < G_N_ELEMENTS (entries); ++i)
      g_signal_connect_object (gimp_action_group_get_action (group, entries[i].name),
                               "activate", G_CALLBACK (save_command),
                               G_OBJECT (group->user_data), 0);
}
void mypaint_brush_editor_actions_update (GimpActionGroup *group, gpointer data)
{
  gboolean enabled = GIMP_IS_PAINTER_MYBRUSH_EDITOR (data);
  gimp_action_group_set_action_sensitive (group, "mypaint-brush-editor-save", enabled, NULL);
  gimp_action_group_set_action_sensitive (group, "mypaint-brush-editor-save-as", enabled, NULL);
}

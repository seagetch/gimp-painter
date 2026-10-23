/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "config/gimpdialogconfig.h"
#include "dialogs/dialogs.h"
#include "dialogs/welcome-dialog.h"
#include "widgets/gimpdialogfactory.h"

Gimp *gimp_test_application_owned_start (const gchar *, GApplication **, gboolean);
void gimp_test_application_owned_release (Gimp *, GApplication *);

static Gimp *gimp;
static guint mode;
static gboolean finalized[6];
static gboolean destroyed;
static guint responses;
static GtkWidget *retained;

static void
finalized_cb (gpointer data, GObject *object)
{
  *((gboolean *) data) = TRUE;
}

static void
destroyed_cb (GtkWidget *widget, gpointer data)
{
  destroyed = TRUE;
}

static gboolean
response_hook (GSignalInvocationHint *hint, guint n_values,
               const GValue *values, gpointer data)
{
  if (g_value_get_object (&values[0]) == data)
    responses++;
  return TRUE;
}

static GtkWidget *
find_button (GtkWidget *widget, const gchar *label)
{
  GtkWidget *found = NULL;
  if (GTK_IS_BUTTON (widget) &&
      g_strcmp0 (gtk_button_get_label (GTK_BUTTON (widget)), label) == 0)
    return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (GList *iter = children; iter && ! found; iter = iter->next)
        found = find_button (iter->data, label);
      g_list_free (children);
    }
  return found;
}

static void
activated (GApplication *application, gpointer data)
{
  GtkWidget *dialog = NULL;
  gulong response_hook_id = 0;
  guint response_signal = g_signal_lookup ("response", GTK_TYPE_DIALOG);

  if (mode)
    {
      if (mode == 2)
        dialog = gimp_dialog_factory_dialog_new (
          gimp_dialog_factory_get_singleton (),
          gdk_display_get_primary_monitor (gdk_display_get_default ()),
          NULL, NULL, "gimp-welcome-dialog", -1, FALSE);
      else
        dialog = welcome_dialog_create (gimp, TRUE);
      g_assert_nonnull (dialog);
      g_signal_connect (dialog, "destroy", G_CALLBACK (destroyed_cb), NULL);
      response_hook_id = g_signal_add_emission_hook (response_signal, 0,
                                                     response_hook, dialog, NULL);
      for (guint i = 0; i < 2; i++)
        {
          GimpDialogConfig *config = g_object_get_data (
            G_OBJECT (dialog), i ? "config-orig" : "config-copy");
          g_assert_nonnull (config);
          g_object_weak_ref (G_OBJECT (config), finalized_cb, &finalized[i * 3]);
          g_object_weak_ref (G_OBJECT (config->fill_options), finalized_cb,
                             &finalized[i * 3 + 1]);
          g_object_weak_ref (G_OBJECT (config->stroke_options), finalized_cb,
                             &finalized[i * 3 + 2]);
        }
      if (mode >= 3)
        retained = g_object_ref (dialog);
      if (mode == 4)
        gtk_widget_destroy (dialog);
      if (mode == 5 || mode == 6)
        {
          GList *before = gtk_window_list_toplevels ();
          GtkWidget *button = find_button (dialog, mode == 5 ? "C_reate" : "_Open");
          GtkWidget *child = NULL;
          GList *after;

          g_assert_nonnull (button);
          gtk_widget_show (dialog);
          gtk_button_clicked (GTK_BUTTON (button));
          after = gtk_window_list_toplevels ();
          for (GList *iter = after; iter; iter = iter->next)
            if (! g_list_find (before, iter->data) && GTK_IS_DIALOG (iter->data))
              child = iter->data;
          g_assert_nonnull (child);
          g_object_ref (child);
          g_list_free (before);
          g_list_free (after);
          g_assert_false (gtk_widget_get_visible (dialog));
          gtk_widget_destroy (dialog);
          /* The child can outlive Welcome. Neither a late response nor its
           * destruction may re-show or dereference the disposed parent.
           */
          gtk_dialog_response (GTK_DIALOG (child), GTK_RESPONSE_CANCEL);
          gtk_widget_destroy (child);
          g_assert_false (gtk_widget_get_visible (dialog));
          g_object_unref (child);
        }
      if (mode == 7)
        {
          GFile *rc = NULL;
          gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
          g_object_get (gimp->edit_config, "user-gimprc", &rc, NULL);
          g_assert_true (g_file_query_exists (rc, NULL));
          g_object_unref (rc);
          g_assert_true (destroyed);
        }
    }

  gimp_exit (gimp, FALSE);
  if (mode)
    {
      g_assert_true (destroyed);
      for (guint i = 0; i < G_N_ELEMENTS (finalized); i++)
        { g_test_message ("config/context witness %u: %d", i, finalized[i]);
          g_assert_true (finalized[i]); }
      g_assert_cmpuint (responses, ==, mode == 7 ? 1 : 0);
      g_signal_remove_emission_hook (response_signal, response_hook_id);
    }
  /* The ordinary GUI exit already ran this; repeated shutdown must be safe. */
  welcome_dialog_exit ();
  welcome_dialog_exit ();
  g_clear_object (&retained);
}

static void
remove_profile (GFile *file)
{
  GError *error = NULL;
  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) ==
      G_FILE_TYPE_DIRECTORY)
    {
      GFileEnumerator *children = g_file_enumerate_children (
        file, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
        NULL, &error);
      GFileInfo *info;
      g_assert_no_error (error);
      while ((info = g_file_enumerator_next_file (children, NULL, &error)))
        {
          GFile *child = g_file_get_child (file, g_file_info_get_name (info));
          remove_profile (child);
          g_object_unref (child);
          g_object_unref (info);
        }
      g_assert_no_error (error);
      g_object_unref (children);
    }
  g_assert_true (g_file_delete (file, NULL, &error));
  g_assert_no_error (error);
}

static void
lifecycle (gconstpointer data)
{
  GApplication *application;
  GFile *profile;
  GError *error = NULL;
  gchar *directory;

  if (! g_test_subprocess ())
    {
      g_test_trap_subprocess (NULL, 60 * G_USEC_PER_SEC, 0);
      g_test_trap_assert_passed ();
      return;
    }
  mode = GPOINTER_TO_UINT (data);
  directory = g_dir_make_tmp ("welcome-shutdown-XXXXXX", &error);
  g_assert_no_error (error);
  g_setenv ("GIMP3_DIRECTORY", directory, TRUE);
  g_unsetenv ("GIMP_PAINTER_HTTP_ENABLE");
  gimp = gimp_test_application_owned_start (directory, &application, TRUE);
  g_signal_connect_after (application, "activate", G_CALLBACK (activated), NULL);
  g_assert_cmpint (g_application_run (application, 0, NULL), ==, 0);
  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);
  gimp_test_application_owned_release (gimp, application);
  g_object_run_dispose (G_OBJECT (gimp));
  g_object_unref (gimp);
  profile = g_file_new_for_path (directory);
  remove_profile (profile);
  g_object_unref (profile);
  g_free (directory);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_data_func ("/welcome-shutdown/absent", GUINT_TO_POINTER (0), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/direct", GUINT_TO_POINTER (1), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/factory", GUINT_TO_POINTER (2), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/retained", GUINT_TO_POINTER (3), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/already-destroyed-retained", GUINT_TO_POINTER (4), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/new-child-outlives-welcome", GUINT_TO_POINTER (5), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/open-child-outlives-welcome", GUINT_TO_POINTER (6), lifecycle);
  g_test_add_data_func ("/welcome-shutdown/normal-close", GUINT_TO_POINTER (7), lifecycle);
  return g_test_run ();
}

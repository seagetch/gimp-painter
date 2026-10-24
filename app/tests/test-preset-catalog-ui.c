/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpprefsbox.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpaction.h"
#include "display/gimpdisplay.h"
#include "display/gimppaintercanvasui.h"
#include "dialogs/preferences-dialog.h"
#include "presets/preset-factory-gui.h"
#include "presets/gimppresetdescriptor.h"
#include "gimpcoreapp.h"
#include "gimp-features.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static gboolean fail_lease_alloc;
gpointer __real_g_try_malloc0 (gsize);
gpointer __wrap_g_try_malloc0 (gsize);
gpointer __wrap_g_try_malloc0 (gsize n)
{ if(fail_lease_alloc){fail_lease_alloc=FALSE;return NULL;}return __real_g_try_malloc0(n); }

static guint publication_fault, copies_created, copies_released, widgets_released;
static gboolean publication_data_attached;
static gpointer observed_dialog;
static void copy_released (gpointer data, GObject *object)
{ (void)data;(void)object;copies_released++; }
static void widget_released (gpointer data, GObject *object)
{ (void)data;(void)object;widgets_released++; }
gpointer __real_gimp_config_duplicate (GimpConfig *config);
gpointer __wrap_gimp_config_duplicate (GimpConfig *config);
gpointer __wrap_gimp_config_duplicate (GimpConfig *config)
{
  gpointer copy=__real_gimp_config_duplicate(config);
  if(publication_fault && G_OBJECT(config)==G_OBJECT(gimp->edit_config))
    { copies_created++;g_object_weak_ref(copy,copy_released,NULL); }
  return copy;
}
gboolean __real_gimp_preset_gui_end (GimpPresetGuiLease *,gboolean);
gboolean __wrap_gimp_preset_gui_end (GimpPresetGuiLease *,gboolean);
gboolean __wrap_gimp_preset_gui_end (GimpPresetGuiLease *lease,gboolean published)
{
  if(publication_fault && published)
    {
      GList *windows=gtk_window_list_toplevels();GtkWidget *dialog=NULL;guint mode=publication_fault;
      for(GList *l=windows;l;l=l->next)
        if(g_strcmp0(gtk_window_get_role(GTK_WINDOW(l->data)),"gimp-preferences")==0)dialog=l->data;
      g_list_free(windows);g_assert_nonnull(dialog);observed_dialog=dialog;
      g_object_add_weak_pointer(G_OBJECT(dialog),&observed_dialog);g_object_weak_ref(G_OBJECT(dialog),widget_released,NULL);
      publication_data_attached=g_object_get_data(G_OBJECT(dialog),"gimp")==gimp &&
        g_object_get_data(G_OBJECT(dialog),"config-copy") && g_object_get_data(G_OBJECT(dialog),"config-orig");
      publication_fault=0;
      if(mode==2)gimp_features_close(gimp);
      return __real_gimp_preset_gui_end(lease,mode==2 ? TRUE : FALSE);
    }
  return __real_gimp_preset_gui_end(lease,published);
}
typedef struct { const gchar *text; gboolean found; } LabelSearch;
static gboolean tree_label (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  LabelSearch *search = data;
  for (gint column = 0; column < gtk_tree_model_get_n_columns (model); ++column)
    if (gtk_tree_model_get_column_type (model, column) == G_TYPE_STRING)
      {
        gchar *text = NULL; gtk_tree_model_get (model, iter, column, &text, -1);
        if (g_strcmp0 (text, search->text) == 0) search->found = TRUE;
        g_free (text);
      }
  return search->found;
}
static gboolean has_label (GtkWidget *widget, const char *text)
{
  if (GTK_IS_TREE_VIEW (widget))
    {
      LabelSearch search = {text, FALSE};
      GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
      if (model) gtk_tree_model_foreach (model, tree_label, &search);
      if (search.found) return TRUE;
    }
  if (GTK_IS_LABEL (widget) && g_strcmp0 (gtk_label_get_text (GTK_LABEL (widget)), text) == 0) return TRUE;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget)); gboolean found = FALSE;
      for (GList *l = children; l && !found; l = l->next) found = has_label (l->data, text);
      g_list_free (children); return found;
    }
  return FALSE;
}
static GtkWidget *find_prefs_widget (GtkWidget *widget, gboolean box)
{
  if ((box && GIMP_IS_PREFS_BOX (widget)) ||
      (!box && g_strcmp0 (g_object_get_data (G_OBJECT (widget), "path"),
                         "tool-preset-path") == 0)) return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      GtkWidget *found = NULL;
      for (GList *l = children; l && !found; l = l->next)
        found = find_prefs_widget (l->data, box);
      g_list_free (children); return found;
    }
  return NULL;
}
typedef struct { const gchar *label; GtkTreePath *path; } PrefsPathSearch;
static gboolean find_prefs_path (GtkTreeModel *model, GtkTreePath *path,
                                 GtkTreeIter *iter, gpointer data)
{
  PrefsPathSearch *search = data;
  LabelSearch label = {search->label, FALSE};
  tree_label (model, path, iter, &label);
  if (label.found) search->path = gtk_tree_path_copy (path);
  return label.found;
}
static void selected_gui (void)
{
  GtkWidget *dialog, *box, *tree, *reset, *window;
  GimpDisplay *display;
  PrefsPathSearch search = {"Catalog selected folders", NULL};
  GimpDialogFactoryEntry *entry = gimp_dialog_factory_find_entry(gimp_dialog_factory_get_singleton(),"gimp-layer-preset-list");
  GimpActionGroup *group;
  GimpPresetDescriptor *d;
  GError *error=NULL;
  const gchar *capture=g_getenv("GIMP_TEST_CATALOG_SCREENSHOT");
  g_assert_nonnull(entry);g_assert_cmpstr(entry->name,==,"Catalog selected presets");
  g_assert_cmpstr(entry->help_id,==,"catalog-selected-help");
  gimp_set_focused_once(gimp);
  gimp_test_utils_create_image(gimp,640,480);gimp_test_run_mainloop_until_idle();
  display=gimp_context_get_display(gimp->user_context);g_assert_nonnull(display);
  g_assert_true(gimp_painter_canvas_ui_get_visible(gimp_display_get_shell(display)));
  group=gimp_action_group_new(gimp,"catalog-selected-actions","Catalog",NULL,display,NULL);
  preset_factory_gui_dialogs_actions_entry_point(group);
  g_assert_cmpstr(gimp_action_get_label(gimp_action_group_get_action(group,"dialogs-layer-presets")),==,"Catalog selected action");
  dialog=preferences_dialog_create(gimp);g_assert_nonnull(dialog);g_object_ref_sink(dialog);
  g_assert_cmpstr(g_object_get_data(G_OBJECT(dialog),"preset-descriptor-help"),==,"catalog-selected-preferences-help");
  box=find_prefs_widget(dialog,TRUE);g_assert_nonnull(box);
  tree=gimp_prefs_box_get_tree_view(GIMP_PREFS_BOX(box));
  gtk_tree_model_foreach(gtk_tree_view_get_model(GTK_TREE_VIEW(tree)),find_prefs_path,&search);g_assert_nonnull(search.path);
  gtk_tree_view_expand_to_path(GTK_TREE_VIEW(tree),search.path);gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree),search.path,NULL,FALSE);gtk_tree_path_free(search.path);
  gtk_widget_show(dialog);gimp_test_run_mainloop_until_idle();
  reset=find_prefs_widget(dialog,FALSE);g_assert_nonnull(reset);g_assert_true(gtk_widget_get_mapped(reset));
  g_assert_cmpstr(g_object_get_data(G_OBJECT(reset),"path-writable"),==,"tool-preset-path-writable");
  g_assert_true(has_label(dialog,"Catalog selected folders"));
  g_assert_cmpstr(gimp_prefs_box_get_current_help_id(GIMP_PREFS_BOX(box)),==,"gimp-prefs-folders-layer-presets");
  d=gimp_preset_catalog_snapshot(gimp);g_assert_false(gimp_preset_catalog_register(gimp,d,&error));g_assert_nonnull(error);g_clear_error(&error);gimp_preset_descriptor_unref(d);
  window=gtk_widget_get_toplevel(GTK_WIDGET(gimp_display_get_shell(display)));
  gtk_window_resize(GTK_WINDOW(window),1040,1800);gtk_window_move(GTK_WINDOW(window),10,20);gtk_window_present(GTK_WINDOW(window));
  gtk_window_move(GTK_WINDOW(dialog),10,700);gtk_window_present(GTK_WINDOW(dialog));
  {gint64 end=g_get_monotonic_time()+G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<end){g_main_context_iteration(NULL,FALSE);g_usleep(10000);}}
  if(capture){GdkWindow *root=gdk_get_default_root_window();GdkPixbuf *pixels;
    g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
    pixels=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(pixels);g_assert_true(gdk_pixbuf_save(pixels,capture,"png",&error,NULL));g_assert_no_error(error);g_object_unref(pixels);}
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);gtk_widget_destroy(dialog);g_object_unref(dialog);g_object_unref(group);
}
static void optional_dialog_allocation_failure (void)
{
  GtkWidget *dialog;
  fail_lease_alloc=TRUE;dialog=preferences_dialog_create(gimp);
  g_assert_false(fail_lease_alloc);g_assert_null(dialog);
  dialog=preferences_dialog_create(gimp);g_assert_nonnull(dialog);g_object_ref_sink(dialog);
  g_assert_true(has_label(dialog,"Catalog selected folders"));
  gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);gtk_widget_destroy(dialog);g_object_unref(dialog);
}
static void publication_failure (gboolean prior_autosave,gboolean close_owner)
{
  GtkWidget *dialog;
  copies_created=copies_released=widgets_released=0;observed_dialog=NULL;publication_data_attached=FALSE;
  gimp_rc_set_autosave(GIMP_RC(gimp->edit_config),prior_autosave);
  publication_fault=close_owner ? 2 : 1;
  dialog=preferences_dialog_create(gimp);g_assert_null(dialog);g_assert_cmpuint(publication_fault,==,0);
  g_assert_cmpint(GIMP_RC(gimp->edit_config)->autosave,==,prior_autosave);
  g_assert_true(publication_data_attached);g_assert_cmpuint(copies_created,==,2);
  g_assert_cmpuint(copies_released,==,2);g_assert_cmpuint(widgets_released,==,1);g_assert_null(observed_dialog);
  /* A failed constructor must not leave callbacks into destroyed list boxes. */
  g_object_notify(G_OBJECT(gimp->config),"theme");g_object_notify(G_OBJECT(gimp->config),"icon-theme");
  if(!close_owner)
    {
      dialog=preferences_dialog_create(gimp);g_assert_nonnull(dialog);g_object_ref_sink(dialog);
      g_assert_true(has_label(dialog,"Catalog selected folders"));
      gtk_dialog_response(GTK_DIALOG(dialog),GTK_RESPONSE_CANCEL);gtk_widget_destroy(dialog);g_object_unref(dialog);
    }
  else g_assert_null(preferences_dialog_create(gimp));
}
static void publication_failure_enabled (void) { publication_failure(TRUE,FALSE); }
static void publication_failure_disabled (void) { publication_failure(FALSE,FALSE); }
static void publication_owner_close (void) { publication_failure(TRUE,TRUE); }
int main(int argc,char **argv)
{
  GimpPresetDescriptor *base,*selected;GimpPresetDescriptorValues v;GError *error=NULL;gint result;
  g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  base=gimp_preset_catalog_snapshot(gimp);v=*gimp_preset_descriptor_values(base);
  v.tree_label=v.label="Catalog selected folders";v.path_property_name="tool-preset-path";v.writable_property_name="tool-preset-path-writable";
  v.dialog_label="Catalog selected presets";v.dialog_help="catalog-selected-help";v.action_label="Catalog selected action";
  v.help_data="catalog-selected-preferences-help";v.icon="folders-tool-presets";
  selected=gimp_preset_descriptor_new(&v,&error);g_assert_no_error(error);g_assert_true(gimp_preset_catalog_register(gimp,selected,&error));g_assert_no_error(error);
  gimp_preset_descriptor_unref(selected);gimp_preset_descriptor_unref(base);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  g_test_add_func("/preset-catalog-ui/selected-metadata-path-page",selected_gui);
  g_test_add_func("/preset-catalog-ui/optional-dialog-allocation-failure",optional_dialog_allocation_failure);
  g_test_add_func("/preset-catalog-ui/publication-failure-enabled",publication_failure_enabled);
  g_test_add_func("/preset-catalog-ui/publication-failure-disabled",publication_failure_disabled);
  g_test_add_func("/preset-catalog-ui/publication-owner-close",publication_owner_close);
  g_application_run(gimp->app,0,NULL);result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(gimp->app);g_clear_object(&gimp->app);return result;
}

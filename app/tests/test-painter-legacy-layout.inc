/* Source-matched legacy layout acceptance, exercised through native widgets. */
#include "display/gimpimagewindow.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpwindowstrategy.h"
#include "widgets/gimpdialogfactory.h"
#include "core/gimp-gui.h"

static void legacy_settle (void)
{
  for (guint i=0;i<80;i++) { drain (); g_usleep (5000); }
}

static void legacy_tabs_and_collection (void)
{
  gboolean saved=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&saved,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  GimpDisplayShell *a=new_shell();
  GimpImageWindow *w=gimp_display_shell_get_window(a);
  GtkWidget *notebook=gtk_widget_get_parent(GTK_WIDGET(a));
  g_assert_true(GTK_IS_NOTEBOOK(notebook));
  GimpDockColumns *left=gimp_image_window_get_left_docks(w);
  GimpDockColumns *right=gimp_image_window_get_right_docks(w);
  /* A populated collection makes orientation and whole-collection transfer
   * assertions meaningful even when the test profile starts with empty docks. */
  GimpDock *fixture_docks[2];
  for(guint i=0;i<2;i++) {
    GtkWidget *book=NULL;
    gimp_dock_columns_prepare_dockbook(right,-1,&book);
    fixture_docks[i]=g_object_ref(gimp_dockbook_get_dock(GIMP_DOCKBOOK(book)));
    g_assert_nonnull(gimp_dockbook_add_from_dialog_factory(GIMP_DOCKBOOK(book),"gimp-buffer-list"));
  }
  const gint dimensions[][2]={{1200,800},{600,1300},{1000,1000},{1200,800}};
  for(guint i=0;i<G_N_ELEMENTS(dimensions);i++) {
    gtk_window_resize(GTK_WINDOW(w),dimensions[i][0],dimensions[i][1]);legacy_settle();
    gint width=gtk_widget_get_allocated_width(notebook),height=gtk_widget_get_allocated_height(notebook);
    g_assert_cmpint(gtk_notebook_get_tab_pos(GTK_NOTEBOOK(notebook)),==,width>height?GTK_POS_LEFT:GTK_POS_TOP);
    g_assert_false(gtk_widget_get_visible(GTK_WIDGET(left)));
    g_assert_true(gtk_widget_is_ancestor(GTK_WIDGET(right),a->canvas));
    gint window_width=gtk_widget_get_allocated_width(GTK_WIDGET(w));
    gint window_height=gtk_widget_get_allocated_height(GTK_WIDGET(w));
    if(dimensions[i][0]==dimensions[i][1])g_assert_cmpint(window_width,==,window_height);
    gboolean portrait=window_width<=window_height;
    for(GList *l=gimp_dock_columns_get_docks(right);l;l=l->next)
      g_assert_cmpint(gtk_orientable_get_orientation(GTK_ORIENTABLE(l->data)),==,portrait?GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL);
  }
  /* A second real view transfers the same collection, never clones Dockables. */
  GimpDisplayShell *b=new_shell();legacy_settle();
  g_assert_true(gimp_display_shell_get_window(b)==w);
  gimp_image_window_set_active_shell(w,b);legacy_settle();
  g_assert_true(gtk_widget_is_ancestor(GTK_WIDGET(right),b->canvas));
  gimp_image_window_set_active_shell(w,a);legacy_settle();
  g_assert_true(gtk_widget_is_ancestor(GTK_WIDGET(right),a->canvas));
  for(guint i=0;i<2;i++) {
    g_assert_true(gtk_widget_is_ancestor(GTK_WIDGET(fixture_docks[i]),GTK_WIDGET(right)));
    gimp_dock_columns_remove_dock(right,fixture_docks[i]);
    gtk_widget_destroy(GTK_WIDGET(fixture_docks[i]));g_object_unref(fixture_docks[i]);
  }
  close_shell(b);close_shell(a);
  g_object_set(gimp->config,"painter-canvas-ui",saved,NULL);drain();
}

static void legacy_destroy_open_target (GimpDock *dock,GimpDockbook *book,gpointer data)
{
  (void)book;(*(guint *)data)++;gtk_widget_destroy(GTK_WIDGET(dock));
}
static void legacy_watch_open_target (GimpDockColumns *columns,GimpDock *dock,gpointer data)
{
  (void)columns;
  g_signal_connect(dock,"book-added",G_CALLBACK(legacy_destroy_open_target),data);
}

static void legacy_options_repeated_open (void)
{
  gboolean saved=FALSE;g_object_get(gimp->config,"painter-canvas-ui",&saved,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  GimpDisplayShell *shell=new_shell();GimpImageWindow *w=gimp_display_shell_get_window(shell);
  GimpDockColumns *right=gimp_image_window_get_right_docks(w);
  GimpDockColumns *left=gimp_image_window_get_left_docks(w);
  GimpDialogFactory *factory=gimp_dialog_factory_get_singleton();
  GtkWidget *existing=gimp_dialog_factory_find_widget(factory,"gimp-tool-options");
  /* The GUI test profile may restore the singleton in a deliberately floating
   * window. An Open action must leave that explicit floating placement alone. */
  if(existing && !gtk_widget_is_ancestor(existing,GTK_WIDGET(left)) &&
                 !gtk_widget_is_ancestor(existing,GTK_WIDGET(right))) {
    GtkWidget *parent=gtk_widget_get_parent(existing);
    GtkWidget *opened=gimp_window_strategy_show_dockable_dialog(
      GIMP_WINDOW_STRATEGY(gimp_get_window_strategy(gimp)),gimp,factory,
      gimp_widget_get_monitor(GTK_WIDGET(w)),"gimp-tool-options");
    g_assert_true(opened==existing);g_assert_true(gtk_widget_get_parent(opened)==parent);
  }
  /* Set up the restored hidden-left layout this regression covers explicitly,
   * retaining the native singleton while moving it into a two-page book. */
  GtkWidget *book=NULL;
  gimp_dock_columns_prepare_dockbook(left,-1,&book);
  g_object_ref(book);
  GtkWidget *sibling=gimp_dockbook_add_from_dialog_factory(GIMP_DOCKBOOK(book),"gimp-buffer-list");
  g_assert_nonnull(sibling);
  GtkWidget *first=gimp_dockbook_add_from_dialog_factory(GIMP_DOCKBOOK(book),"gimp-tool-options");
  g_assert_nonnull(first);
  if(gtk_widget_get_parent(first)!=book) {
    g_object_ref(first);gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(first)),first);
    gtk_notebook_append_page(GTK_NOTEBOOK(book),first,NULL);g_object_unref(first);
  }
  g_assert_true(gtk_widget_is_ancestor(first,GTK_WIDGET(left)));
  g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(book)),==,2);
  for(guint i=0;i<3;i++) {
    GtkWidget *opened=gimp_window_strategy_show_dockable_dialog(
      GIMP_WINDOW_STRATEGY(gimp_get_window_strategy(gimp)),gimp,factory,
      gimp_widget_get_monitor(GTK_WIDGET(w)),"gimp-tool-options");
    legacy_settle();g_assert_nonnull(opened);g_assert_true(opened==first);
    g_assert_true(gtk_widget_get_parent(opened)==book);
    g_assert_true(gtk_widget_get_parent(sibling)==book);
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(book)),==,2);
    g_assert_true(gtk_widget_is_ancestor(opened,GTK_WIDGET(right)));
    g_assert_false(gtk_widget_get_visible(GTK_WIDGET(left)));
  }
  /* A destination can close synchronously from book-added. The Open call
   * must not show the destroyed dock or recreate the destroyed singleton. */
  GimpDock *old=gimp_dockbook_get_dock(GIMP_DOCKBOOK(book));
  gimp_dock_remove_book(old,GIMP_DOCKBOOK(book));
  GtkWidget *placeholder=NULL;
  gimp_dock_columns_prepare_dockbook(left,-1,&placeholder);
  GimpDock *left_dock=gimp_dockbook_get_dock(GIMP_DOCKBOOK(placeholder));
  gimp_dock_add_book(left_dock,GIMP_DOCKBOOK(book),-1);
  gimp_dock_remove_book(left_dock,GIMP_DOCKBOOK(placeholder));
  guint closes=0;
  GList *targets=gimp_dock_columns_get_docks(right);
  gulong close_handler;
  GObject *emitter;
  if(targets) {
    emitter=g_object_ref(targets->data);
    close_handler=g_signal_connect(emitter,"book-added",G_CALLBACK(legacy_destroy_open_target),&closes);
  } else {
    emitter=G_OBJECT(g_object_ref(right));
    close_handler=g_signal_connect(emitter,"dock-added",G_CALLBACK(legacy_watch_open_target),&closes);
  }
  GtkWidget *closed=gimp_window_strategy_show_dockable_dialog(
    GIMP_WINDOW_STRATEGY(gimp_get_window_strategy(gimp)),gimp,factory,
    gimp_widget_get_monitor(GTK_WIDGET(w)),"gimp-tool-options");
  g_assert_null(closed);g_assert_cmpuint(closes,==,1);
  g_assert_null(gimp_dialog_factory_find_widget(factory,"gimp-tool-options"));
  if(g_signal_handler_is_connected(emitter,close_handler))g_signal_handler_disconnect(emitter,close_handler);
  g_object_unref(emitter);g_object_unref(book);
  close_shell(shell);g_object_set(gimp->config,"painter-canvas-ui",saved,NULL);drain();
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
#include "widgets/popupper.h"
#include "widgets/popupper.h"
#include "core/gimpcontext.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
/* Fault driver is compiled as C: no C++ stack may receive a provider exception. */
GtkWidget *popover_c_factory (GtkWidget *view);
GtkWidget *popover_c_raw_new (void);
GtkWidget *popover_c_factory (GtkWidget *view) { return gimp_popover_new (view); }
GtkWidget *popover_c_raw_new (void) { return GTK_WIDGET (g_object_new (GIMP_TYPE_POPOVER, "type", GTK_WINDOW_POPUP, NULL)); }
static Gimp *gimp;
void popover_cpp_handles (void);
void popover_cpp_faults (void);
static void drain (void) { gimp_test_run_mainloop_until_idle (); }
static void original_factory (void)
{
  GtkWidget *view = gtk_entry_new ();
  GtkWidget *popup = gimp_popover_new (view);
  GtkWidget *frame;
  gpointer weak = popup;
  g_assert_nonnull (popup); g_assert_true (GIMP_IS_POPOVER (popup));
  g_assert_cmpstr (G_OBJECT_TYPE_NAME (popup), ==, "GimpPopover2");
  g_assert_cmpuint (g_type_parent (G_OBJECT_TYPE (popup)), ==, GTK_TYPE_WINDOW);
  g_assert_false (g_object_is_floating (popup));
  g_assert_false (g_object_is_floating (view));
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (popup)), ==, GTK_WINDOW_POPUP);
  g_assert_false (gtk_window_get_resizable (GTK_WINDOW (popup)));
  frame = gtk_bin_get_child (GTK_BIN (popup)); g_assert_true (GTK_IS_FRAME (frame));
  g_assert_cmpint (gtk_frame_get_shadow_type (GTK_FRAME (frame)), ==, GTK_SHADOW_OUT);
  g_assert_true (gtk_bin_get_child (GTK_BIN (frame)) == view);
  g_assert_true (gtk_widget_get_visible (view));
  g_assert_true (gtk_window_get_focus (GTK_WINDOW (popup)) == view);
  g_object_add_weak_pointer (G_OBJECT (popup), &weak);
  gtk_widget_destroy (popup); g_assert_null (weak);
}
static void invalid_views (void)
{
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *view = gtk_label_new ("already owned");
  g_object_ref_sink (box); gtk_container_add (GTK_CONTAINER (box), view);
  g_assert_null (gimp_popover_new (NULL)); g_assert_null (gimp_popover_new (window));
  g_assert_null (gimp_popover_new (view));
  g_assert_true (gtk_widget_get_parent (view) == box);
  gtk_widget_destroy (window); gtk_widget_destroy (box); g_object_unref (box);
}
static void mapped_grabs (void)
{
  GtkWidget *popup = gimp_popover_new (gtk_entry_new ());
  for (guint i=0;i<4;++i) {
    gtk_widget_show (popup); drain (); g_assert_true (gtk_grab_get_current () == popup);
    gtk_widget_hide (popup); drain (); g_assert_true (gtk_grab_get_current () != popup);
  }
  gtk_widget_show (popup); drain (); gtk_widget_destroy (popup);
  g_assert_null (gtk_grab_get_current ());
}
static void count_signal (GtkWidget *widget, gpointer data) { (void) widget; ++*(guint *) data; }
static void signal_actions (void)
{
  const gchar *names[] = { "cancel", "confirm" };
  for (guint i=0;i<2;++i) {
    GtkWidget *popup=gimp_popover_new (gtk_entry_new ()); gpointer weak=popup; guint count=0;
    GSignalQuery query;
    g_signal_query (g_signal_lookup (names[i], GIMP_TYPE_POPOVER), &query);
    g_assert_true ((query.signal_flags & (G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION)) == (G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION));
    g_object_add_weak_pointer (G_OBJECT (popup), &weak);
    g_signal_connect (popup, names[i], G_CALLBACK (count_signal), &count);
    gtk_widget_show (popup); drain (); g_signal_emit_by_name (popup, names[i]);
    g_assert_cmpuint (count, ==, 1); g_assert_null (weak); g_assert_null (gtk_grab_get_current ());
  }
}
static void veto (GtkWidget *widget, gpointer data)
{ ++*(guint *) data; g_signal_stop_emission_by_name (widget, "cancel"); }
static void signal_veto (void)
{
  GtkWidget *popup=gimp_popover_new (gtk_entry_new ()); guint count=0;
  gulong id=g_signal_connect (popup, "cancel", G_CALLBACK (veto), &count);
  gtk_widget_show (popup); drain (); g_signal_emit_by_name (popup, "cancel");
  g_assert_cmpuint (count, ==, 1); g_assert_true (gtk_grab_get_current () == popup);
  g_signal_handler_disconnect (popup,id); gtk_widget_destroy (popup);
}
static void key_bindings (void)
{
  GtkWidget *popup=gimp_popover_new (gtk_entry_new ()); guint cancel=0, confirm=0;
  GdkEventKey key={0}; gpointer weak=popup;
  g_signal_connect (popup,"cancel",G_CALLBACK(count_signal),&cancel);
  g_signal_connect (popup,"confirm",G_CALLBACK(count_signal),&confirm);
  gtk_widget_show (popup); drain (); key.type=GDK_KEY_PRESS;
  key.window=gtk_widget_get_window(popup); key.keyval=GDK_KEY_Return;
  GTK_WIDGET_GET_CLASS(popup)->key_press_event(popup,&key);
  key.keyval=GDK_KEY_space; GTK_WIDGET_GET_CLASS(popup)->key_press_event(popup,&key);
  g_assert_cmpuint(confirm,==,0); g_assert_cmpuint(cancel,==,0);
  g_object_add_weak_pointer(G_OBJECT(popup),&weak);
  key.keyval=GDK_KEY_Escape; g_assert_true(GTK_WIDGET_GET_CLASS(popup)->key_press_event(popup,&key));
  g_assert_cmpuint(cancel,==,1); g_assert_null(weak);
}
static void button_contract (void)
{
  for (guint i=0;i<5;++i) {
    GtkWidget *popup=gimp_popover_new(gtk_entry_new()); GtkAllocation a; GdkEventButton e={0};
    guint cancel=0; gpointer weak=popup;
    gtk_widget_show(popup); drain(); gtk_widget_get_allocation(popup,&a);
    g_signal_connect(popup,"cancel",G_CALLBACK(count_signal),&cancel);
    g_object_add_weak_pointer(G_OBJECT(popup),&weak);
    e.type=GDK_BUTTON_PRESS; e.window=gtk_widget_get_window(popup); e.x=1;e.y=1;
    if(i==1) { e.x=a.width;e.y=a.height; }
    if(i==2) e.x=-0.5;
    if(i==3) e.x=a.width+0.5;
    if(i==4) e.window=NULL;
    g_assert_cmpint(GTK_WIDGET_GET_CLASS(popup)->button_press_event(popup,&e),==,(i==2||i==3));
    g_assert_cmpuint(cancel,==,(i==2||i==3));
    if(weak) gtk_widget_destroy(popup);
    g_assert_null(weak);
  }
}
static void button_targets (void)
{
  GtkWidget *entry=gtk_entry_new(), *popup=gimp_popover_new(entry), *other=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GdkEventButton event={0}; gpointer weak=popup; guint cancel=0;
  gtk_widget_show(other); gtk_widget_show(popup); drain();
  g_signal_connect(popup,"cancel",G_CALLBACK(count_signal),&cancel);
  event.type=GDK_BUTTON_PRESS;event.window=gtk_widget_get_window(entry);event.x=1;event.y=1;
  g_assert_false(GTK_WIDGET_GET_CLASS(popup)->button_press_event(popup,&event));
  g_assert_cmpuint(cancel,==,0);
  g_object_add_weak_pointer(G_OBJECT(popup),&weak);event.window=gtk_widget_get_window(other);
  g_assert_true(GTK_WIDGET_GET_CLASS(popup)->button_press_event(popup,&event));
  g_assert_cmpuint(cancel,==,1);g_assert_null(weak);gtk_widget_destroy(other);
}
static void destroy_on_map (GtkWidget *widget, gpointer data)
{ ++*(guint*)data;gtk_widget_destroy(widget); }
static void map_reentry (void)
{
  GtkWidget *popup=gimp_popover_new(gtk_entry_new());gpointer weak=popup;guint calls=0;
  g_object_add_weak_pointer(G_OBJECT(popup),&weak);
  g_signal_connect(popup,"map",G_CALLBACK(destroy_on_map),&calls);
  gtk_widget_show(popup);drain();g_assert_cmpuint(calls,==,1);g_assert_null(weak);g_assert_null(gtk_grab_get_current());
}
static void destroy_cancel (GtkWidget *popup, gpointer data)
{ ++*(guint*)data; gtk_widget_destroy(popup); }
static void cancel_reentry (void)
{
  GtkWidget *popup=gimp_popover_new(gtk_entry_new()); gpointer weak=popup; guint calls=0;
  g_object_add_weak_pointer(G_OBJECT(popup),&weak);
  g_signal_connect(popup,"cancel",G_CALLBACK(destroy_cancel),&calls);
  gtk_widget_show(popup); drain(); g_signal_emit_by_name(popup,"cancel");
  g_assert_cmpuint(calls,==,1); g_assert_null(weak); g_assert_null(gtk_grab_get_current());
}
static void destroy_parent (GtkWidget *view, GtkWidget *old, gpointer data)
{
  (void)old;
  if(gtk_widget_get_parent(view)) { ++*(guint*)data; gtk_widget_destroy(gtk_widget_get_toplevel(view)); }
}
static void insertion_reentry (void)
{
  GtkWidget *view=gtk_entry_new(); guint calls=0; gpointer weak=view;
  g_object_add_weak_pointer(G_OBJECT(view),&weak);
  g_signal_connect(view,"parent-set",G_CALLBACK(destroy_parent),&calls);
  g_assert_null(gimp_popover_new(view)); g_assert_cmpuint(calls,==,1); g_assert_null(weak);
}
static void actual_ui (void)
{
  GimpDisplay *display; GimpDisplayShell *shell; GtkWidget *main_window,*popup,*box,*label,*entry;
  const gchar *path=g_getenv("GIMP_TEST_POPOVER_SCREENSHOT"); gboolean enabled=FALSE;
  gimp_test_utils_create_image(gimp,128,128); drain();
  display=gimp_context_get_display(gimp_get_user_context(gimp)); shell=gimp_display_get_shell(display);
  g_object_get(gimp->config,"painter-canvas-ui",&enabled,NULL); g_assert_true(enabled);
  g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  main_window=gtk_widget_get_toplevel(GTK_WIDGET(shell)); gtk_window_resize(GTK_WINDOW(main_window),1040,1800);
  box=gtk_box_new(GTK_ORIENTATION_VERTICAL,12); gtk_container_set_border_width(GTK_CONTAINER(box),24);
  label=gtk_label_new("Original popup-window provider — GimpPopover2"); entry=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry),"Escape cancels • explicit confirm closes");
  gtk_box_pack_start(GTK_BOX(box),label,FALSE,FALSE,0); gtk_box_pack_start(GTK_BOX(box),entry,FALSE,FALSE,0);
  gtk_widget_show_all(box); popup=gimp_popover_new(box);
  gtk_window_set_transient_for(GTK_WINDOW(popup),GTK_WINDOW(main_window)); gtk_window_move(GTK_WINDOW(popup),260,650);
  gtk_widget_show(popup);
  for(guint i=0;i<20;++i){ drain();g_usleep(10000); }
  g_assert_true(gtk_grab_get_current()==popup);
  if(path){ GError *error=NULL; GdkWindow *root=gdk_get_default_root_window(); GdkPixbuf *pixels;
    g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
    pixels=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(pixels);
    g_assert_true(gdk_pixbuf_save(pixels,path,"png",&error,NULL));g_assert_no_error(error);g_object_unref(pixels);
  }
  g_signal_emit_by_name(popup,"confirm"); g_assert_null(gtk_grab_get_current());
}
int main(int argc,char **argv)
{
  gint result;g_test_init(&argc,&argv,NULL);
  if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  g_test_add_func("/native-popover/original-factory",original_factory);
  g_test_add_func("/native-popover/invalid-views",invalid_views);
  g_test_add_func("/native-popover/mapped-grabs",mapped_grabs);
  g_test_add_func("/native-popover/signal-actions",signal_actions);
  g_test_add_func("/native-popover/signal-veto",signal_veto);
  g_test_add_func("/native-popover/key-bindings",key_bindings);
  g_test_add_func("/native-popover/button-contract",button_contract);
  g_test_add_func("/native-popover/button-targets",button_targets);
  g_test_add_func("/native-popover/map-reentry",map_reentry);
  g_test_add_func("/native-popover/cancel-reentry",cancel_reentry);
  g_test_add_func("/native-popover/insertion-reentry",insertion_reentry);
  g_test_add_func("/native-popover/typed-handles",popover_cpp_handles);
  g_test_add_func("/native-popover/factory-faults",popover_cpp_faults);
  g_test_add_func("/native-popover/actual-ui",actual_ui);
  g_application_run(gimp->app,0,NULL);result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}

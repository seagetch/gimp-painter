#include <gtk/gtk.h>
int main(int argc,char **argv){gtk_init(&argc,&argv);
 GtkWidget *off=gtk_offscreen_window_new(), *ob=gtk_button_new_with_label("offscreen");gtk_container_add(GTK_CONTAINER(off),ob);gtk_widget_show_all(off);
 while(gtk_events_pending()) { gtk_main_iteration(); }
 
 g_assert_cmpint(gdk_window_get_window_type(gtk_widget_get_window(off)),==,GDK_WINDOW_OFFSCREEN);
 gtk_grab_add(ob);g_assert_null(gtk_grab_get_current());g_print("PASS offscreen grab remains rejected\n");
 GtkWidget *w=gtk_window_new(GTK_WINDOW_TOPLEVEL),*b=gtk_button_new_with_label("normal");gtk_container_add(GTK_CONTAINER(w),b);gtk_widget_show_all(w);
 while(gtk_events_pending()) { gtk_main_iteration(); }
 gtk_grab_add(b);g_assert_true(gtk_grab_get_current()==b);gtk_grab_remove(b);g_assert_null(gtk_grab_get_current());g_print("PASS mapped normal grab acquire/release\n");
 GtkWidget *m=gtk_menu_new();gtk_grab_add(m);g_assert_true(gtk_grab_get_current()==m);gtk_grab_remove(m);g_object_ref_sink(m);g_object_unref(m);g_print("PASS unrealized menu null-window grab acquire/release\n");gtk_widget_destroy(w);gtk_widget_destroy(off);return 0;}

/* Independent GTK3/ATK dependency reproducer. No GIMP headers or code.
 * Every diagnostic is forwarded unchanged to the normal GLib handler.
 */
#include <gtk/gtk.h>
#include <atk/atk.h>
#include <string.h>

static GtkWidget *window;
static GtkWidget *item;
static GtkWidget *dialog;
static guint dialogs, gdk_count, gtk_count, other_count;
static gboolean expect_known_defect;
static gboolean passed;

static void
count_log (const gchar    *domain,
           GLogLevelFlags level,
           const gchar   *message,
           gpointer       unused)
{
  (void) unused;
  if (strstr (message, "gdk_window_get_window_type"))
    ++gdk_count;
  else if (strstr (message, "gtk_accessible_get_widget"))
    ++gtk_count;
  else
    ++other_count;
  g_log_default_handler (domain, level, message, NULL);
}

static void
open_dialog (GtkMenuItem *menu,
             gpointer     unused)
{
  (void) menu;
  (void) unused;
  ++dialogs;
  dialog = gtk_dialog_new_with_buttons ("GTK dependency control",
                                       GTK_WINDOW (window), 0,
                                       "Close", GTK_RESPONSE_CLOSE, NULL);
  g_signal_connect_swapped (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), dialog);
  gtk_widget_show (dialog);
}

static gboolean
run_case (gpointer unused)
{
  AtkObject *accessible = gtk_widget_get_accessible (item);
  GtkWidget *toplevel = gtk_widget_get_toplevel (item);
  guint      before_gdk, before_gtk, before_other;
  gboolean   action;

  (void) unused;
  g_print ("MENU toplevel=%s has_window=%d mapped=%d\n",
           G_OBJECT_TYPE_NAME (toplevel),
           gtk_widget_get_window (toplevel) != NULL,
           gtk_widget_get_mapped (item));
  for (AtkObject *parent = accessible; parent;
       parent = atk_object_get_parent (parent))
    g_print ("ANCESTOR type=%s is_gtk_accessible=%d\n",
             G_OBJECT_TYPE_NAME (parent), GTK_IS_ACCESSIBLE (parent));

  g_print ("PHASE hidden ATK action\n");
  action = atk_action_do_action (ATK_ACTION (accessible), 0);
  g_print ("RESULT hidden-atk action=%d dialogs=%u gdk=%u gtk=%u other=%u\n",
           action, dialogs, gdk_count, gtk_count, other_count);
  passed = action && dialogs == 1 && other_count == 0 &&
           gdk_count == (expect_known_defect ? 1u : 0u) &&
           gtk_count == (expect_known_defect ? 1u : 0u);
  if (dialogs == 1)
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);

  before_gdk = gdk_count;
  before_gtk = gtk_count;
  before_other = other_count;
  g_print ("PHASE direct GTK activation control\n");
  gtk_menu_item_activate (GTK_MENU_ITEM (item));
  g_print ("RESULT direct-gtk dialogs=%u added_gdk=%u added_gtk=%u added_other=%u\n",
           dialogs, gdk_count - before_gdk, gtk_count - before_gtk,
           other_count - before_other);
  passed = passed && dialogs == 2 && gdk_count == before_gdk &&
           gtk_count == before_gtk && other_count == before_other;
  if (dialogs == 2)
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CLOSE);
  gtk_widget_destroy (window);
  return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
  GtkWidget *bar, *root, *menu;

  if (argc != 2 || (strcmp (argv[1], "--expect-known-defect") &&
                    strcmp (argv[1], "--check-fixed")))
    {
      g_printerr ("Usage: %s --expect-known-defect|--check-fixed\n", argv[0]);
      return 2;
    }
  expect_known_defect = ! strcmp (argv[1], "--expect-known-defect");
  if (! gtk_init_check (&argc, &argv))
    {
      g_printerr ("GTK display unavailable; no test was run\n");
      return 77;
    }
  g_log_set_handler ("Gtk", G_LOG_LEVEL_CRITICAL, count_log, NULL);
  g_log_set_handler ("Gdk", G_LOG_LEVEL_CRITICAL, count_log, NULL);
  g_print ("GTK runtime=%u.%u.%u mode=%s\n",
           gtk_get_major_version (), gtk_get_minor_version (),
           gtk_get_micro_version (), argv[1]);
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "GTK menu assertion control");
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 160);
  bar = gtk_menu_bar_new ();
  root = gtk_menu_item_new_with_label ("File");
  menu = gtk_menu_new ();
  item = gtk_menu_item_new_with_label ("Open Dialog");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root), menu);
  gtk_menu_shell_append (GTK_MENU_SHELL (bar), root);
  gtk_container_add (GTK_CONTAINER (window), bar);
  g_signal_connect (item, "activate", G_CALLBACK (open_dialog), NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_widget_show_all (window);
  g_idle_add (run_case, NULL);
  gtk_main ();
  passed = passed && dialogs == 2 && other_count == 0 &&
           gdk_count == (expect_known_defect ? 1u : 0u) &&
           gtk_count == (expect_known_defect ? 1u : 0u);
  g_print ("FINAL dialogs=%u gdk=%u gtk=%u other=%u passed=%d\n",
           dialogs, gdk_count, gtk_count, other_count, passed);
  return passed ? 0 : 1;
}

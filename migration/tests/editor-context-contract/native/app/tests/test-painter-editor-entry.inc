/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Ordinary constructor/menu contract only; no acquisition reentry or legacy
 * GimpDataEditor layout/session equivalence is asserted here. */
extern "C" {
#include "menus/menus.h"
#include "widgets/gimpmenufactory.h"
#include "widgets/gimpmypaintbrusheditor.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
}
static void named_editor_constructor ()
{
  prepare ();
  auto *factory = menus_get_global_menu_factory (gimp);
  g_assert_nonnull (factory);
  g_assert_null (gimp_mypaint_brush_editor_new (nullptr, factory));
  auto wrong = ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  g_assert_null (gimp_mypaint_brush_editor_new (reinterpret_cast<GimpContext *> (wrong.get ()), factory));
  g_assert_null (gimp_mypaint_brush_editor_new (gimp->user_context, reinterpret_cast<GimpMenuFactory *> (wrong.get ())));
  auto empty = ObjectRef<GObject>::adopt (G_OBJECT (gimp_menu_factory_new (gimp, global_action_factory)));
  g_assert_null (gimp_mypaint_brush_editor_new (gimp->user_context, GIMP_MENU_FACTORY (empty.get ())));
  auto *plain = gimp_mypaint_brush_editor_new (gimp->user_context, nullptr);
  g_assert_nonnull (plain); g_assert_true (g_object_is_floating (plain));
  g_object_ref_sink (plain); g_assert_null (gimp_editor_get_menu_factory (GIMP_EDITOR (plain)));
  gtk_widget_destroy (plain); g_object_unref (plain);
  auto owner = MypaintBrushEditorRef::create (gimp->user_context, factory);
  g_assert_false (g_object_is_floating (owner.get ()));
  g_assert_true (gimp_editor_get_menu_factory (GIMP_EDITOR (owner.get ())) == factory);
  g_assert_nonnull (gimp_editor_get_ui_manager (GIMP_EDITOR (owner.get ())));
  g_assert_cmpstr (gimp_editor_get_ui_path (GIMP_EDITOR (owner.get ())), ==, "/mypaint_brush-editor-popup");
  Editor other;
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (control (GTK_WIDGET (owner.get ()), "opaque")), .371);
  g_assert_cmpfloat_with_epsilon (other.snapshot ().base_value (BRUSH_OPAQUE), .371, 1e-6);
  gtk_widget_destroy (GTK_WIDGET (owner.get ()));
}
static GimpMenuFactoryEntry *named_editor_factory_entry (GimpMenuFactory *factory)
{
  for (auto *l = gimp_menu_factory_get_registered_menus (factory); l; l = l->next) {
    auto *entry = static_cast<GimpMenuFactoryEntry *> (l->data);
    if (!g_strcmp0 (entry->identifier, "<MypaintBrushEditor>")) return entry;
  }
  return nullptr;
}
static void named_editor_menu_lifetime ()
{
  prepare ();
  auto *factory = menus_get_global_menu_factory (gimp);
  auto *entry = named_editor_factory_entry (factory); g_assert_nonnull (entry);
  auto *widget = gimp_mypaint_brush_editor_new (gimp->user_context, factory);
  g_assert_nonnull (widget); g_object_ref_sink (widget);
  auto *editor = GIMP_PAINTER_MYBRUSH_EDITOR (widget);
  GWeakRef weak; g_weak_ref_init (&weak, G_OBJECT (widget));
  auto manager = ObjectRef<GObject>::retain (G_OBJECT (gimp_editor_get_ui_manager (GIMP_EDITOR (widget))));
  auto group = ObjectRef<GObject>::retain (G_OBJECT (gimp_ui_manager_get_action_group (GIMP_UI_MANAGER (manager.get ()), "mypaint-brush-editor")));
  g_assert_true (GIMP_ACTION_GROUP (group.get ())->user_data == widget);
  auto action = ObjectRef<GObject>::retain (G_OBJECT (gimp_ui_manager_find_action (GIMP_UI_MANAGER (manager.get ()),
    "mypaint-brush-editor", "mypaint-brush-editor-save")));
  g_assert_true (g_hash_table_lookup (entry->managers, widget) == manager.get ());
  gimp_docked_set_context (GIMP_DOCKED (widget), nullptr);
  g_assert_true (g_hash_table_lookup (entry->managers, widget) == manager.get ());
  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_editor_set_context (editor, gimp->user_context, &error));
  g_assert_no_error (error);
  g_assert_true (g_hash_table_lookup (entry->managers, widget) == manager.get ());
  gtk_widget_destroy (widget);
  g_assert_null (g_hash_table_lookup (entry->managers, widget));
  g_assert_null (GIMP_ACTION_GROUP (group.get ())->user_data);
  g_object_run_dispose (G_OBJECT (widget)); // ordinary repeated teardown
  g_object_unref (widget);
  g_assert_null (g_weak_ref_get (&weak)); g_weak_ref_clear (&weak);
  // The externally retained action no longer has an owner callback. This does
  // not exercise acquisition reentry or the held tree-renderer popup path.
  g_assert_cmpuint (g_signal_handler_find (action.get (), G_SIGNAL_MATCH_DATA, 0, 0,
                                          nullptr, nullptr, widget), ==, 0);
  gimp_action_activate (GIMP_ACTION (action.get ()));
}
static GtkWidget *named_editor_prompt ()
{
  auto *windows = gtk_window_list_toplevels ();
  GtkWidget *result = nullptr;
  for (auto *l = windows; l; l = l->next)
    if (GTK_IS_DIALOG (l->data) && gtk_widget_get_visible (GTK_WIDGET (l->data)) &&
        !g_strcmp0 (gtk_window_get_title (GTK_WINDOW (l->data)), "Save Painter Brush As"))
      { g_assert_null (result); result = GTK_WIDGET (l->data); }
  g_list_free (windows); return result;
}
static bool named_menu_has_action (GMenuModel *model, const char *action)
{
  for (int n = 0; n < g_menu_model_get_n_items (model); ++n) {
    gchar *name = nullptr;
    if (g_menu_model_get_item_attribute (model, n, G_MENU_ATTRIBUTE_ACTION, "s", &name)) {
      bool match = !g_strcmp0 (name, action); g_free (name); if (match) return true;
    }
    for (const char *link : {G_MENU_LINK_SECTION, G_MENU_LINK_SUBMENU}) {
      auto child = ObjectRef<GObject>::adopt (G_OBJECT (g_menu_model_get_item_link (model, n, link)));
      if (child && named_menu_has_action (G_MENU_MODEL (child.get ()), action)) return true;
    }
  }
  return false;
}
static void named_editor_menu_save ()
{
  prepare ();
  const char *screenshot = g_getenv ("GIMP_TEST_EDITOR_NAMED_SCREENSHOT");
  GimpDisplayShell *shell = nullptr;
  if (screenshot) {
    gimp_test_utils_create_image (gimp, 640, 480);
    auto *display = gimp_context_get_display (gimp->user_context);
    g_assert_nonnull (display); shell = gimp_display_get_shell (display);
    gimp_painter_canvas_ui_set_visible (shell, TRUE);
    g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
    auto *main_window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
    gtk_window_resize (GTK_WINDOW (main_window), 1040, 1850);
    gtk_window_move (GTK_WINDOW (main_window), 0, 0);
  }
  auto owner = MypaintBrushEditorRef::create (gimp->user_context, menus_get_global_menu_factory (gimp));
  auto *widget = GTK_WIDGET (owner.get ());
  auto window = ObjectRef<GObject>::sink (G_OBJECT (gtk_window_new (GTK_WINDOW_TOPLEVEL)));
  gtk_window_set_default_size (GTK_WINDOW (window.get ()), 1000, screenshot ? 1200 : 1300);
  if (screenshot) gtk_window_move (GTK_WINDOW (window.get ()), 20, 650);
  gtk_window_set_title (GTK_WINDOW (window.get ()), "Native named constructor — shared draft Save / Save As");
  gtk_container_add (GTK_CONTAINER (window.get ()), widget); gtk_widget_show_all (GTK_WIDGET (window.get ()));
  auto *manager = gimp_editor_get_ui_manager (GIMP_EDITOR (widget));
  gimp_ui_manager_update (manager, widget);
  auto menu = ObjectRef<GObject>::adopt (G_OBJECT (gimp_ui_manager_get_model (manager, "/mypaint_brush-editor-popup")));
  g_assert_nonnull (menu.get ());
  g_assert_true (named_menu_has_action (G_MENU_MODEL (menu.get ()), "mypaint-brush-editor.mypaint-brush-editor-save"));
  g_assert_true (named_menu_has_action (G_MENU_MODEL (menu.get ()), "mypaint-brush-editor.mypaint-brush-editor-save-as"));
  auto *save = gimp_ui_manager_find_action (manager, "mypaint-brush-editor", "mypaint-brush-editor-save");
  auto *save_as = gimp_ui_manager_find_action (manager, "mypaint-brush-editor", "mypaint-brush-editor-save-as");
  g_assert_nonnull (save); g_assert_nonnull (save_as);
  GError *error = nullptr;
  auto model = ObjectRef<GimpPainterMybrushOptions>::adopt (gimp_painter_mybrush_options_ref_for_context (gimp->user_context, &error));
  g_assert_no_error (error);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (control (widget, "opaque")), .427);
  gtk_entry_set_text (GTK_ENTRY (control (widget, "painter-brush-name")), "Named entry menu save");
  gimp_action_activate (save);
  auto saved = ObjectRef<GimpPainterMybrush>::retain (gimp_context_get_painter_mybrush (GIMP_CONTEXT (model.get ())));
  g_assert_cmpstr (gimp_object_get_name (saved.get ()), ==, "Named entry menu save");
  g_assert_nonnull (gimp_data_get_file (GIMP_DATA (saved.get ())));
  auto stream = ObjectRef<GObject>::adopt (G_OBJECT (g_file_read (gimp_data_get_file (GIMP_DATA (saved.get ())), nullptr, &error)));
  g_assert_no_error (error);
  g_assert_cmpfloat_with_epsilon (MyPaint::Resource::load (G_INPUT_STREAM (stream.get ())).base_value (BRUSH_OPAQUE), .427, 1e-6);
  gimp_action_activate (save_as);
  auto *dialog = named_editor_prompt (); g_assert_nonnull (dialog);
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  g_assert_null (named_editor_prompt ());
  g_assert_true (saved.get () == gimp_context_get_painter_mybrush (GIMP_CONTEXT (model.get ())));
  gimp_action_activate (save_as); dialog = named_editor_prompt (); g_assert_nonnull (dialog);
  auto *children = gtk_container_get_children (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))));
  g_assert_nonnull (children); g_assert_true (GTK_IS_ENTRY (children->data));
  gtk_entry_set_text (GTK_ENTRY (children->data), "Named entry menu separate"); g_list_free (children);
  if (screenshot) {
    gimp_test_run_temp_mainloop (700);
    gtk_window_move (GTK_WINDOW (window.get ()), 20, 650);
    gtk_window_present (GTK_WINDOW (window.get ()));
    gtk_window_present (GTK_WINDOW (dialog));
    gimp_test_run_temp_mainloop (300);
    gdk_window_raise (gtk_widget_get_window (GTK_WIDGET (window.get ())));
    gdk_window_raise (gtk_widget_get_window (dialog));
    gdk_display_sync (gdk_display_get_default ());
    g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
    auto *root = gdk_get_default_root_window ();
    g_assert_cmpint (gdk_window_get_width (root), ==, 1080);
    g_assert_cmpint (gdk_window_get_height (root), ==, 1920);
    auto pixels = ObjectRef<GObject>::adopt (G_OBJECT (gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920)));
    g_assert_nonnull (pixels.get ());
    g_assert_true (gdk_pixbuf_save (GDK_PIXBUF (pixels.get ()), screenshot, "png", &error, nullptr));
    g_assert_no_error (error);
    g_test_message ("Actual portrait desktop 1080x1920; oncanvas visible; API-activated menu Save As prompt");
  }
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  auto separate = ObjectRef<GimpPainterMybrush>::retain (gimp_context_get_painter_mybrush (GIMP_CONTEXT (model.get ())));
  g_assert_true (separate.get () != saved.get ());
  g_assert_cmpstr (gimp_object_get_name (separate.get ()), ==, "Named entry menu separate");
  g_assert_cmpfloat_with_epsilon (PainterMybrushRef::retain (saved.get ()).snapshot ().base_value (BRUSH_OPAQUE), .427, 1e-6);
  g_assert_true (gimp_painter_mybrush_editor_delete (owner.get (), &error)); g_assert_no_error (error);
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (model.get ()), saved.get ());
  g_assert_true (gimp_painter_mybrush_editor_delete (owner.get (), &error)); g_assert_no_error (error);
  gtk_widget_destroy (GTK_WIDGET (window.get ()));
  if (shell) {
    auto *display = shell->display;
    g_object_unref (gimp_display_get_image (display));
    gimp_display_close (display);
    gimp_test_run_mainloop_until_idle ();
  }
}

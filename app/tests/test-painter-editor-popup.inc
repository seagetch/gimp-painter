/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise the real default-oncanvas entry, never an artificial editor host. */
static void popup_drain () { gimp_test_run_mainloop_until_idle (); }
static GtkWidget *popup_named_window (const char *name)
{
  auto *windows = gtk_window_list_toplevels (); GtkWidget *result = nullptr;
  for (auto *l = windows; l; l = l->next)
    if (gtk_widget_get_visible (GTK_WIDGET (l->data)) && !g_strcmp0 (gtk_widget_get_name (GTK_WIDGET (l->data)), name))
      { g_assert_null (result); result = GTK_WIDGET (l->data); }
  g_list_free (windows); return result;
}
static GtkWidget *popup_window ()
{
  GtkWidget *result = nullptr;
  GList *windows = gtk_window_list_toplevels ();
  for (auto *l = windows; l; l = l->next)
    if (gtk_widget_get_visible (GTK_WIDGET (l->data)) &&
        !g_strcmp0 (gtk_window_get_title (GTK_WINDOW (l->data)), "Painter MyPaint Brush Editor"))
      { g_assert_null (result); result = GTK_WIDGET (l->data); }
  g_list_free (windows); return result;
}
static void popup_screenshot (const char *variable)
{
  if (const char *path = g_getenv (variable)) {
    const auto deadline = g_get_monotonic_time () + 500 * G_TIME_SPAN_MILLISECOND;
    do { popup_drain (); g_usleep (1000); } while (g_get_monotonic_time () < deadline);
    auto *root = gdk_get_default_root_window ();
    g_assert_cmpint (gdk_window_get_width (root), ==, 1080);
    g_assert_cmpint (gdk_window_get_height (root), ==, 1920);
    auto *pixels = gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920);
    GError *error = nullptr; g_assert_nonnull (pixels);
    g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, nullptr));
    g_assert_no_error (error); g_object_unref (pixels);
  }
}
struct CanvasPopup {
  GimpDisplayShell *shell;
  CanvasPopup () {
    prepare ();
    static bool native_resources_ready = false;
    if (!native_resources_ready) {
      auto *shipped = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "gimp-data", "brushes", nullptr);
      auto *writable = g_build_filename (gimp_directory (), "brushes", nullptr);
      g_assert_cmpint (g_mkdir_with_parents (writable, 0700), ==, 0);
      auto *search = g_build_path (G_SEARCHPATH_SEPARATOR_S, shipped, writable, nullptr);
      g_object_set (gimp->config, "brush-path", search, "brush-path-writable", writable, nullptr);
      g_free (search); g_free (shipped); g_free (writable);
      gimp_data_factory_data_refresh (gimp->brush_factory, gimp->user_context);
      auto *brushes = gimp_data_factory_get_container (gimp->brush_factory);
      guint file_backed = 0;
      for (int n = 0; n < gimp_container_get_n_children (brushes); ++n)
        if (gimp_data_get_file (GIMP_DATA (gimp_container_get_child_by_index (brushes, n)))) ++file_backed;
      g_assert_cmpuint (file_backed, >, 0);
      g_test_message ("Native dock factory has %u shipped file-backed brushes", file_backed);
      native_resources_ready = true;
    }
    g_assert_true (gimp->config->painter_canvas_ui);
    gimp_test_utils_create_image (gimp, 640, 480); popup_drain ();
    auto *display = gimp_context_get_display (gimp->user_context);
    g_assert_nonnull (display); shell = gimp_display_get_shell (display);
    g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
    if (g_getenv ("GIMP_TEST_EDITOR_POPUP_SCREENSHOT") || g_getenv ("GIMP_TEST_EDITOR_COMPACT_POPUP_SCREENSHOT")) {
      auto *main_window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
      gtk_window_resize (GTK_WINDOW (main_window), 1040, 1800);
      gtk_window_move (GTK_WINDOW (main_window), 0, 0);
      const auto deadline = g_get_monotonic_time () + 300 * G_TIME_SPAN_MILLISECOND;
      do { popup_drain (); g_usleep (1000); } while (g_get_monotonic_time () < deadline);
    }
    auto *tool = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list, "gimp-painter-mypaint-tool"));
    g_assert_nonnull (tool); gimp_context_set_tool (gimp->user_context, tool); popup_drain ();
  }
  GtkWidget *open () {
    auto *button = control (shell->canvas, "painter-compact-mypaint");
    g_assert_true (gtk_widget_get_visible (button));
    gtk_button_clicked (GTK_BUTTON (button)); popup_drain ();
    auto *host = popup_named_window ("painter-compact-mypaint-popup");
    g_assert_nonnull (host);
    g_assert_true (GTK_IS_WINDOW (host));
    g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (host)), ==, GTK_WINDOW_POPUP);
    g_assert_cmpint (gtk_widget_get_allocated_width (host), >=, 340);
    popup_screenshot ("GIMP_TEST_EDITOR_COMPACT_POPUP_SCREENSHOT");
    gtk_button_clicked (GTK_BUTTON (control (host, "painter-brush-popup"))); popup_drain ();
    auto *window = popup_window (); g_assert_nonnull (window);
    return window;
  }
  ~CanvasPopup () {
    if (!shell) return;
    if (auto *window = popup_window ()) gtk_widget_destroy (window);
    auto *display = shell->display;
    auto *image = gimp_display_get_image (display);
    gimp_display_delete (display); g_object_unref (image); popup_drain ();
  }
};
static void popup_real_entry_type ()
{
  CanvasPopup canvas;
  auto *window = canvas.open ();
  g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (window)), ==, GTK_WINDOW_POPUP);
  g_assert_true (gtk_widget_get_mapped (window));
  g_assert_true (gtk_widget_get_visible (control (window, "opaque")));
  popup_screenshot ("GIMP_TEST_EDITOR_POPUP_SCREENSHOT");
}
static void popup_key (GtkWidget *widget, guint key)
{
  auto *event = gdk_event_new (GDK_KEY_PRESS);
  event->key.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (widget)));
  event->key.send_event = TRUE; event->key.time = GDK_CURRENT_TIME; event->key.keyval = key;
  gdk_event_set_device (event, gdk_seat_get_keyboard (gdk_display_get_default_seat (gtk_widget_get_display (widget))));
  gtk_widget_event (widget, event); gdk_event_free (event); popup_drain ();
}
static void popup_live_keys_and_outside ()
{
  CanvasPopup canvas;
  auto *window = canvas.open ();
  auto keep = ObjectRef<GObject>::retain (G_OBJECT (window));
  GError *error = nullptr;
  auto model = ObjectRef<GimpPainterMybrushOptions>::adopt (gimp_painter_mybrush_options_ref_for_context (gimp->user_context, &error));
  g_assert_no_error (error);
  auto *spin = control (window, "opaque");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), .384);
  g_assert_cmpfloat_with_epsilon (PainterOptionsRef::retain (model.get ()).snapshot ().base_value (BRUSH_OPAQUE), .384, 1e-6);
  gtk_widget_grab_focus (spin); popup_key (window, GDK_KEY_Return);
  g_assert_true (gtk_widget_get_visible (window));
  gtk_window_set_focus (GTK_WINDOW (window), nullptr); // test global Space, not spin text insertion
  popup_key (window, GDK_KEY_space);
  g_assert_true (gtk_widget_get_visible (window));
  auto *focus = gdk_event_new (GDK_FOCUS_CHANGE);
  focus->focus_change.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (window)));
  focus->focus_change.in = FALSE;
  gtk_widget_event (window, focus); gdk_event_free (focus); popup_drain ();
  g_assert_true (gtk_widget_get_visible (window)); // legacy does not cancel on focus loss
  popup_key (window, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_visible (window));
  g_assert_cmpfloat_with_epsilon (PainterOptionsRef::retain (model.get ()).snapshot ().base_value (BRUSH_OPAQUE), .384, 1e-6);
  window = canvas.open (); keep = ObjectRef<GObject>::retain (G_OBJECT (window));
  auto *event = gdk_event_new (GDK_BUTTON_PRESS);
  event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (window)));
  event->button.x = -10; event->button.y = -10; event->button.button = 1;
  gdk_event_set_device (event, gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (window))));
  // Assert the legacy vfunc contract directly: gtk_widget_event itself may
  // report handled when a handler destroys its receiver, regardless of FALSE.
  g_assert_false (GTK_WIDGET_GET_CLASS (window)->button_press_event (window, &event->button));
  gdk_event_free (event); popup_drain ();
  g_assert_false (gtk_widget_get_visible (window));
  g_assert_cmpfloat_with_epsilon (PainterOptionsRef::retain (model.get ()).snapshot ().base_value (BRUSH_OPAQUE), .384, 1e-6);
  // Also retain GTK dispatcher coverage; its return differs after destruction.
  window = canvas.open (); keep = ObjectRef<GObject>::retain (G_OBJECT (window));
  event = gdk_event_new (GDK_BUTTON_PRESS);
  event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (window)));
  event->button.x = -10; event->button.y = -10; event->button.button = 1;
  gdk_event_set_device (event, gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (window))));
  gtk_widget_event (window, event); gdk_event_free (event); popup_drain ();
  g_assert_false (gtk_widget_get_visible (window));
  g_assert_cmpfloat_with_epsilon (PainterOptionsRef::retain (model.get ()).snapshot ().base_value (BRUSH_OPAQUE), .384, 1e-6);
}
static GtkWidget *popup_entry (GtkWidget *widget)
{
  if (GTK_IS_ENTRY (widget)) return widget;
  if (!GTK_IS_CONTAINER (widget)) return nullptr;
  auto *children = gtk_container_get_children (GTK_CONTAINER (widget)); GtkWidget *result = nullptr;
  for (auto *l = children; l && !result; l = l->next) result = popup_entry (GTK_WIDGET (l->data));
  g_list_free (children); return result;
}
static void popup_prompt_click (GtkWidget *dialog, gint response)
{
  g_assert_true (gtk_grab_get_current () == dialog);
  auto *button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), response);
  gint x, y, width, height; gtk_window_get_position (GTK_WINDOW (dialog), &x, &y);
  gtk_window_get_size (GTK_WINDOW (dialog), &width, &height);
  g_test_message ("Nested prompt geometry %d,%d %dx%d; current grab %s", x, y, width, height,
                  gtk_grab_get_current () ? G_OBJECT_TYPE_NAME (gtk_grab_get_current ()) : "none");
  g_assert_nonnull (button); g_assert_true (gtk_widget_get_mapped (button));
  const auto ready = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  auto *event_window = gtk_button_get_event_window (GTK_BUTTON (button));
  while (!gdk_window_is_viewable (event_window) && g_get_monotonic_time () < ready)
    { popup_drain (); g_usleep (1000); }
  g_assert_true (gdk_window_is_viewable (event_window));
  auto *device = gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (button)));
  auto *enter = gdk_event_new (GDK_ENTER_NOTIFY);
  enter->crossing.window = GDK_WINDOW (g_object_ref (event_window));
  enter->crossing.mode = GDK_CROSSING_NORMAL; enter->crossing.detail = GDK_NOTIFY_NONLINEAR;
  enter->crossing.x = 5; enter->crossing.y = 5;
  gdk_event_set_device (enter, device); gtk_main_do_event (enter); gdk_event_free (enter);
  for (auto type : {GDK_BUTTON_PRESS, GDK_BUTTON_RELEASE}) {
    auto *event = gdk_event_new (type);
    event->button.window = GDK_WINDOW (g_object_ref (event_window));
    event->button.x = 5; event->button.y = 5; event->button.button = 1;
    event->button.time = GDK_CURRENT_TIME;
    event->button.state = type == GDK_BUTTON_RELEASE ? GDK_BUTTON1_MASK : 0;
    gdk_event_set_device (event, device); gtk_main_do_event (event); gdk_event_free (event);
    popup_drain ();
  }
  const auto deadline = g_get_monotonic_time () + 200 * G_TIME_SPAN_MILLISECOND;
  do { popup_drain (); g_usleep (1000); } while (g_get_monotonic_time () < deadline);
}
static void popup_nested_save_as ()
{
  CanvasPopup canvas; auto *window = canvas.open ();
  auto keep = ObjectRef<GObject>::retain (G_OBJECT (window));
  gtk_button_clicked (GTK_BUTTON (control (window, "painter-brush-save-as"))); popup_drain ();
  auto *dialog = named_editor_prompt (); g_assert_nonnull (dialog);
  g_assert_true (gtk_window_get_transient_for (GTK_WINDOW (dialog)) == GTK_WINDOW (window));
  g_assert_true (gtk_window_get_modal (GTK_WINDOW (dialog)));
  popup_prompt_click (dialog, GTK_RESPONSE_CANCEL);
  g_assert_null (named_editor_prompt ());
  g_assert_true (gtk_widget_get_visible (window)); g_assert_true (gtk_grab_get_current () == window);
  gtk_button_clicked (GTK_BUTTON (control (window, "painter-brush-save-as"))); popup_drain ();
  dialog = named_editor_prompt (); g_assert_nonnull (dialog);
  auto *entry = popup_entry (gtk_dialog_get_content_area (GTK_DIALOG (dialog))); g_assert_nonnull (entry);
  gtk_entry_set_text (GTK_ENTRY (entry), "Popup Native Save Test");
  popup_prompt_click (dialog, GTK_RESPONSE_OK);
  g_assert_null (named_editor_prompt ()); g_assert_true (gtk_widget_get_visible (window));
  g_assert_true (gtk_grab_get_current () == window);
  GError *error = nullptr;
  auto model = ObjectRef<GimpPainterMybrushOptions>::adopt (gimp_painter_mybrush_options_ref_for_context (gimp->user_context, &error));
  g_assert_no_error (error);
  auto *saved = gimp_context_get_painter_mybrush (GIMP_CONTEXT (model.get ()));
  g_assert_cmpstr (gimp_object_get_name (saved), ==, "Popup Native Save Test");
  g_assert_true (g_file_query_exists (gimp_data_get_file (GIMP_DATA (saved)), nullptr));
  /* Delete through the real editor, so this test leaves no saved resource. */
  auto *editor = gtk_widget_get_ancestor (control (window, "opaque"), GIMP_TYPE_PAINTER_MYBRUSH_EDITOR);
  g_assert_nonnull (editor);
  g_assert_true (gimp_painter_mybrush_editor_delete (GIMP_PAINTER_MYBRUSH_EDITOR (editor), &error)); g_assert_no_error (error);
}
static void popup_parent_close ()
{
  CanvasPopup canvas; auto *window = canvas.open ();
  auto keep = ObjectRef<GObject>::retain (G_OBJECT (window));
  gtk_button_clicked (GTK_BUTTON (control (window, "painter-brush-save-as"))); popup_drain ();
  auto *dialog = named_editor_prompt (); g_assert_nonnull (dialog);
  auto prompt = ObjectRef<GObject>::retain (G_OBJECT (dialog));
  auto *host = popup_named_window ("painter-compact-mypaint-popup"); g_assert_nonnull (host);
  gtk_widget_destroy (host); popup_drain ();
  g_assert_false (gtk_widget_get_visible (window));
  g_assert_false (gtk_widget_get_visible (dialog));
  g_assert_null (named_editor_prompt ());
  /* A fresh actual activation must work after the entire popup chain closes. */
  window = canvas.open (); g_assert_true (gtk_widget_get_mapped (window));
}
static void popup_anchor_construction_reentry ()
{
  CanvasPopup canvas; auto *window = canvas.open ();
  auto *anchor = control (window, "painter-brush-popup");
  auto keep = ObjectRef<GObject>::retain (G_OBJECT (anchor));
  struct Reentry { GtkWidget *anchor; bool fired; } state {anchor, false};
  auto signal = g_signal_lookup ("parent-set", GTK_TYPE_WIDGET);
  auto hook = g_signal_add_emission_hook (signal, 0,
    +[](GSignalInvocationHint *, guint, const GValue *values, gpointer data)->gboolean {
      auto &state = *static_cast<Reentry *> (data);
      auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
      if (!state.fired && !g_strcmp0 (gtk_widget_get_name (widget), "painter-editor-notebook")) {
        state.fired = true; gtk_widget_destroy (state.anchor);
      }
      return TRUE;
    }, &state, nullptr);
  g_assert_cmpuint (hook, >, 0);
  GError *error = nullptr;
  auto *result = gimp_painter_mybrush_editor_popup (gimp->user_context, anchor, &error);
  g_signal_remove_emission_hook (signal, hook);
  g_assert_true (state.fired); g_assert_null (result);
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  // Only the original real-entry popup remains; no orphan was presented.
  g_assert_true (popup_window () == window);
}

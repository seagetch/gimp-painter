/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native Duplicate is a saved-resource action; Save As is a draft action. */
static GtkEntry *duplicate_prompt_entry (GObject *dialog)
{
  GtkEntry *entry = nullptr;
  auto *children = gtk_container_get_children (GTK_CONTAINER (
    gtk_dialog_get_content_area (GTK_DIALOG (dialog))));
  for (auto *p = children; p; p = p->next)
    if (GTK_IS_ENTRY (p->data)) entry = GTK_ENTRY (p->data);
  g_list_free (children); g_assert_nonnull (entry); return entry;
}
static std::string duplicate_file_bytes (GimpPainterMybrush *brush)
{
  gchar *bytes = nullptr; gsize size = 0; GError *error = nullptr;
  g_assert_true (g_file_load_contents (gimp_data_get_file (GIMP_DATA (brush)),
    nullptr, &bytes, &size, nullptr, &error)); g_assert_no_error (error);
  std::string result (bytes, size); g_free (bytes); return result;
}
static void duplicate_assert_disk (GimpPainterMybrush *brush, MyPaint::Resource expected)
{
  GError *error = nullptr;
  auto stream = ObjectRef<GObject>::adopt (G_OBJECT (g_file_read (
    gimp_data_get_file (GIMP_DATA (brush)), nullptr, &error)));
  g_assert_no_error (error);
  expected.set_parent_brush_name (gimp_object_get_name (brush));
  g_assert_true (MyPaint::Resource::load (G_INPUT_STREAM (stream.get ())).encode () == expected.encode ());
}
struct DuplicateFixture {
  Editor editor;
  ObjectRef<GimpPainterMybrush> source;
  MyPaint::Resource saved, draft;
  std::string bytes;
  GimpContainer *container;
  int count;
  bool source_deleted = false;
  DuplicateFixture () {
    container = gimp_data_factory_get_container (gimp->painter_mybrush_factory);
    saved = MyPaint::Resource::decode ("{\"version\":3,\"unknown\":{\"saved\":\"A\"},\"settings\":{}}");
    saved.set_base_value (BRUSH_OPAQUE, .21);
    saved.set_text (BRUSH_TEXTURE_NAME, "saved-A-paper");
    saved.set_curve (BRUSH_SMUDGE, INPUT_PRESSURE, {{0,.1},{1,.3}});
    auto icon = ObjectRef<GObject>::adopt (G_OBJECT (gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5)));
    gdk_pixbuf_fill (GDK_PIXBUF (icon.get ()), 0xa04080ff);
    gchar *png = nullptr; gsize size = 0; GError *error = nullptr;
    g_assert_true (gdk_pixbuf_save_to_buffer (GDK_PIXBUF (icon.get ()), &png, &size, "png", &error, nullptr));
    g_assert_no_error (error);
    String encoded (g_base64_encode (reinterpret_cast<const guchar *> (png), size)); g_free (png);
    saved.set_preview_png_base64 (encoded.get ());
    editor.set (saved); seed_prompt_fixture (editor.model.get (), "Duplicate saved source A");
    source = ObjectRef<GimpPainterMybrush>::retain (gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())));
    auto *tag = gimp_tag_new ("saved-duplicate-tag");
    gimp_tagged_add_tag (GIMP_TAGGED (source.get ()), tag); g_object_unref (tag);
    saved = PainterMybrushRef::retain (source.get ()).snapshot (); bytes = duplicate_file_bytes (source.get ());
    draft = MyPaint::Resource::decode ("{\"version\":3,\"unknown\":{\"draft\":\"B\"},\"settings\":{}}");
    draft.set_base_value (BRUSH_OPAQUE, .83); draft.set_text (BRUSH_TEXTURE_NAME, "draft-B-paper");
    draft.set_curve (BRUSH_SMUDGE, INPUT_PRESSURE, {{0,.7},{.5,.8},{1,.9}});
    gdk_pixbuf_fill (GDK_PIXBUF (icon.get ()), 0x30b070ff);
    g_assert_true (gdk_pixbuf_save_to_buffer (GDK_PIXBUF (icon.get ()), &png, &size, "png", &error, nullptr));
    g_assert_no_error (error); String second (g_base64_encode (reinterpret_cast<const guchar *> (png), size)); g_free (png);
    draft.set_preview_png_base64 (second.get ()); editor.set (draft);
    gtk_entry_set_text (GTK_ENTRY (editor.get ("painter-brush-name")), "Unsaved draft B");
    count = gimp_container_get_n_children (container);
  }
  void unchanged () {
    g_assert_true (PainterMybrushRef::retain (source.get ()).snapshot ().encode () == saved.encode ());
    g_assert_true (duplicate_file_bytes (source.get ()) == bytes);
  }
  void pending () {
    unchanged ();
    g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (editor.model.get ())) == source.get ());
    g_assert_true (editor.snapshot ().encode () == draft.encode ());
    g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (editor.get ("painter-brush-name"))), ==, "Unsaved draft B");
    g_assert_cmpint (gimp_container_get_n_children (container), ==, count);
  }
  void erase (GimpPainterMybrush *brush) {
    GError *error = nullptr;
    g_assert_true (gimp_data_factory_data_delete (gimp->painter_mybrush_factory, GIMP_DATA (brush), TRUE, &error));
    g_assert_no_error (error);
  }
  ~DuplicateFixture () { if (!source_deleted) erase (source.get ()); }
};
static void duplicate_capture (Editor &editor, const char *suffix, const char *title)
{
  const char *prefix = g_getenv ("GIMP_TEST_EDITOR_DUPLICATE_SCREENSHOT");
  if (!prefix) return;
  auto *display = gimp_context_get_display (gimp->user_context);
  if (!display || !gimp_display_get_image (display)) { gimp_test_utils_create_image (gimp, 640, 480); display = gimp_context_get_display (gimp->user_context); }
  g_assert_nonnull (display); auto *shell = gimp_display_get_shell (display);
  /* Integrated product default must expose the actual canvas UI without an override. */
  gboolean configured = FALSE;
  g_object_get (gimp->config, "painter-canvas-ui", &configured, nullptr);
  g_assert_true (configured);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  auto *main_window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  gtk_window_resize (GTK_WINDOW (main_window), 1040, 640);
  gtk_window_move (GTK_WINDOW (main_window), 0, 0);
  gtk_window_resize (GTK_WINDOW (editor.window), 990, 1180);
  gtk_window_move (GTK_WINDOW (editor.window), 25, 650);
  gtk_window_set_title (GTK_WINDOW (editor.window), title);
  gtk_window_present (GTK_WINDOW (editor.window)); editor.preview ();
  gimp_test_run_temp_mainloop (700);
  gtk_window_move (GTK_WINDOW (editor.window), 25, 650);
  gdk_window_raise (gtk_widget_get_window (editor.window));
  gimp_test_run_temp_mainloop (300); gdk_display_sync (gdk_display_get_default ());
  auto *root = gdk_get_default_root_window ();
  g_assert_cmpint (gdk_window_get_width (root), ==, 1080);
  g_assert_cmpint (gdk_window_get_height (root), ==, 1920);
  auto pixels = ObjectRef<GObject>::adopt (G_OBJECT (gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920)));
  g_assert_nonnull (pixels.get ()); GError *error = nullptr;
  const std::string path = std::string (prefix) + "-" + suffix + ".png";
  g_assert_true (gdk_pixbuf_save (GDK_PIXBUF (pixels.get ()), path.c_str (), "png", &error, nullptr));
  g_assert_no_error (error);
  g_test_message ("Actual portrait 1080x1920; oncanvas default TRUE, no enabling override; %s", title);
}
static void duplicate_saved_resource ()
{
  prepare (); DuplicateFixture f;
  const guint history = gimp_painter_mybrush_options_history_size (f.editor.model.get ());
  auto prompt = retained_editor_prompt (f.editor, "painter-brush-duplicate");
  const std::string default_name = gtk_entry_get_text (duplicate_prompt_entry (prompt.get ()));
  const std::string dialog_title = gtk_window_get_title (GTK_WINDOW (prompt.get ()));
  auto *accept = gtk_dialog_get_widget_for_response (GTK_DIALOG (prompt.get ()), GTK_RESPONSE_OK);
  const std::string accept_label = gtk_button_get_label (GTK_BUTTON (accept));
  gtk_entry_set_text (duplicate_prompt_entry (prompt.get ()), "Saved copy \316\261");
  gtk_dialog_response (GTK_DIALOG (prompt.get ()), GTK_RESPONSE_OK);
  auto copy = ObjectRef<GimpPainterMybrush>::retain (gimp_context_get_painter_mybrush (GIMP_CONTEXT (f.editor.model.get ())));
  g_assert_true (copy.get () != f.source.get ());
  g_assert_cmpstr (gimp_object_get_name (copy.get ()), ==, "Saved copy \316\261");
  g_assert_true (PainterMybrushRef::retain (copy.get ()).snapshot ().encode () == f.saved.encode ());
  duplicate_assert_disk (copy.get (), f.saved); f.unchanged ();
  g_assert_cmpstr (default_name.c_str (), ==, "Duplicate saved source A copy");
  g_assert_cmpstr (dialog_title.c_str (), ==, "Duplicate Painter Brush");
  g_assert_cmpstr (accept_label.c_str (), ==, "Duplicate");
  auto *tags = gimp_tagged_get_tags (GIMP_TAGGED (copy.get ()));
  auto *source_tags = gimp_tagged_get_tags (GIMP_TAGGED (f.source.get ()));
  g_assert_cmpuint (g_list_length (tags), ==, g_list_length (source_tags));
  for (auto *p = source_tags; p; p = p->next) {
    bool found = false;
    for (auto *q = tags; q; q = q->next)
      if (gimp_tag_equals (GIMP_TAG (p->data), GIMP_TAG (q->data))) found = true;
    g_assert_true (found);
  }
  duplicate_capture (f.editor, "saved-copy", "Duplicate persisted saved A (opacity 0.21); dirty B retained in history");
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (f.editor.model.get ()), ==, history + 1);
  gtk_combo_box_set_active (GTK_COMBO_BOX (f.editor.get ("painter-brush-history")), history);
  g_assert_true (f.editor.snapshot ().encode () == f.draft.encode ());
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f.editor.get ("painter-brush-name"))), ==, "Unsaved draft B");
  duplicate_capture (f.editor, "restored-draft", "Restored unsaved draft B (opacity 0.83); saved A and duplicate unchanged");
  auto save_as = retained_editor_prompt (f.editor, "painter-brush-save-as");
  gtk_entry_set_text (duplicate_prompt_entry (save_as.get ()), "Saved draft B");
  gtk_dialog_response (GTK_DIALOG (save_as.get ()), GTK_RESPONSE_OK);
  auto draft_copy = ObjectRef<GimpPainterMybrush>::retain (gimp_context_get_painter_mybrush (GIMP_CONTEXT (f.editor.model.get ())));
  g_assert_true (PainterMybrushRef::retain (draft_copy.get ()).snapshot ().encode () == f.draft.encode ());
  duplicate_assert_disk (draft_copy.get (), f.draft); f.unchanged ();
  f.erase (draft_copy.get ()); f.erase (copy.get ());
}
static void duplicate_interrupted_resource ()
{
  prepare (); DuplicateFixture f;
  for (const char *name : {"", "Duplicate saved source A", "Canceled copy"}) {
    auto prompt = retained_editor_prompt (f.editor, "painter-brush-duplicate");
    gtk_entry_set_text (duplicate_prompt_entry (prompt.get ()), name);
    gtk_dialog_response (GTK_DIALOG (prompt.get ()), !strcmp (name, "Canceled copy") ? GTK_RESPONSE_CANCEL : GTK_RESPONSE_OK);
    f.pending ();
  }
  gchar *old_writable = nullptr; g_object_get (gimp->config, "painter-mypaint-brush-path-writable", &old_writable, nullptr);
  GError *error = nullptr; String blocked (g_dir_make_tmp ("painter-duplicate-blocked-XXXXXX", &error));
  g_assert_no_error (error); g_assert_cmpint (g_rmdir (blocked.get ()), ==, 0);
  g_assert_true (g_file_set_contents (blocked.get (), "not a directory", -1, &error)); g_assert_no_error (error);
  g_object_set (gimp->config, "painter-mypaint-brush-path-writable", blocked.get (), nullptr);
  auto failed = retained_editor_prompt (f.editor, "painter-brush-duplicate");
  gtk_entry_set_text (duplicate_prompt_entry (failed.get ()), "Cannot persist copy");
  gtk_dialog_response (GTK_DIALOG (failed.get ()), GTK_RESPONSE_OK);
  f.pending ();
  g_object_set (gimp->config, "painter-mypaint-brush-path-writable", old_writable, nullptr); g_free (old_writable);
  g_assert_cmpint (g_unlink (blocked.get ()), ==, 0);
  auto stale = retained_editor_prompt (f.editor, "painter-brush-duplicate");
  gtk_entry_set_text (duplicate_prompt_entry (stale.get ()), "Stale copy");
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (f.editor.model.get ()), GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_get_standard (GIMP_CONTEXT (f.editor.model.get ()))));
  gtk_dialog_response (GTK_DIALOG (stale.get ()), GTK_RESPONSE_OK);
  g_assert_null (gimp_container_get_child_by_name (f.container, "Stale copy")); f.unchanged ();
}
static void duplicate_reentry_resource ()
{
  prepare (); DuplicateFixture f;
  auto prompt = retained_editor_prompt (f.editor, "painter-brush-duplicate");
  gtk_entry_set_text (duplicate_prompt_entry (prompt.get ()), "Reentrant saved copy");
  struct State { DuplicateFixture *f; GObject *dialog; bool fired = false; } state {&f, prompt.get ()};
  auto handler = g_signal_connect (f.container, "add", G_CALLBACK (+[](GimpContainer *, GimpObject *object, gpointer p) {
    auto &s = *static_cast<State *> (p);
    if (s.fired || g_strcmp0 (gimp_object_get_name (object), "Reentrant saved copy")) return;
    s.fired = true;
    gtk_dialog_response (GTK_DIALOG (s.dialog), GTK_RESPONSE_OK);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (s.f->editor.get ("opaque")), .97);
    gtk_entry_set_text (GTK_ENTRY (s.f->editor.get ("painter-brush-name")), "Newer synchronous draft");
  }), &state);
  gtk_dialog_response (GTK_DIALOG (prompt.get ()), GTK_RESPONSE_OK);
  g_signal_handler_disconnect (f.container, handler); g_assert_true (state.fired);
  auto *copy = GIMP_PAINTER_MYBRUSH (gimp_container_get_child_by_name (f.container, "Reentrant saved copy"));
  g_assert_nonnull (copy); duplicate_assert_disk (copy, f.saved); f.unchanged ();
  g_assert_cmpint (gimp_container_get_n_children (f.container), ==, f.count + 1);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (f.editor.model.get ())) == f.source.get ());
  g_assert_cmpfloat_with_epsilon (f.editor.snapshot ().base_value (BRUSH_OPAQUE), .97, 1e-6);
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f.editor.get ("painter-brush-name"))), ==, "Newer synchronous draft");
  f.erase (copy);
}

static void duplicate_deleted_source ()
{
  prepare (); DuplicateFixture f;
  const auto history = gimp_painter_mybrush_options_history_size (f.editor.model.get ());
  auto prompt = retained_editor_prompt (f.editor, "painter-brush-duplicate");
  gtk_entry_set_text (duplicate_prompt_entry (prompt.get ()), "Deleted source copy");
  f.erase (f.source.get ()); f.source_deleted = true;
  gtk_dialog_response (GTK_DIALOG (prompt.get ()), GTK_RESPONSE_OK);
  g_assert_null (gimp_container_get_child_by_name (f.container, "Deleted source copy"));
  g_assert_cmpint (gimp_container_get_n_children (f.container), ==, f.count - 1);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (f.editor.model.get ()), ==, history + 1);
  gtk_combo_box_set_active (GTK_COMBO_BOX (f.editor.get ("painter-brush-history")), history);
  g_assert_true (f.editor.snapshot ().encode () == f.draft.encode ());
  g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f.editor.get ("painter-brush-name"))), ==, "Unsaved draft B");
}

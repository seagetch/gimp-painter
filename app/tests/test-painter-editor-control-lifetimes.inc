/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Direct name/save/delete emitters, distinct from prompt response ownership. */
struct DirectEditorControls {
  std::vector<ObjectRef<GObject>> controls;
  std::vector<gulong> handlers;
  explicit DirectEditorControls (Editor& editor) {
    for (const char *name : {"painter-brush-name", "painter-brush-save", "painter-brush-delete"}) {
      controls.push_back (ObjectRef<GObject>::retain (G_OBJECT (editor.get (name))));
      auto *object = controls.back ().get ();
      const auto signal = g_signal_lookup (GTK_IS_ENTRY (object) ? "changed" : "clicked", G_OBJECT_TYPE (object));
      auto handler = g_signal_handler_find (object, G_SIGNAL_MATCH_ID, signal, 0, nullptr, nullptr, nullptr);
      g_assert_cmpuint (handler, >, 0); handlers.push_back (handler);
    }
  }
  void disconnected () {
    for (unsigned n = 0; n < controls.size (); ++n)
      g_assert_false (g_signal_handler_is_connected (controls[n].get (), handlers[n]));
  }
  void emit_late () {
    gtk_entry_set_text (GTK_ENTRY (controls[0].get ()), "Detached name must not propagate");
    gtk_button_clicked (GTK_BUTTON (controls[1].get ()));
    gtk_button_clicked (GTK_BUTTON (controls[2].get ()));
  }
};
static unsigned editor_dialog_count () {
  unsigned count = 0; auto *windows = gtk_window_list_toplevels ();
  for (auto *l = windows; l; l = l->next) if (GTK_IS_DIALOG (l->data)) ++count;
  g_list_free (windows); return count;
}
static void direct_control_retirement () {
  prepare ();
  for (int retirement = 0; retirement < 3; ++retirement) {
    Editor editor; DirectEditorControls retained (editor);
    auto owner = ObjectRef<GObject>::retain (G_OBJECT (editor.widget));
    auto replacement = ObjectRef<GObject>::adopt (G_OBJECT (gimp_context_new (gimp, "direct replacement", nullptr)));
    GError *error = nullptr;
    if (retirement == 0) g_assert_true (gimp_painter_mybrush_editor_set_context (editor.widget, GIMP_CONTEXT (replacement.get ()), &error));
    else if (retirement == 1) g_assert_true (gimp_painter_binding_close (owner.get (), &error));
    else { gtk_widget_destroy (GTK_WIDGET (editor.widget)); g_object_run_dispose (owner.get ()); g_object_run_dispose (owner.get ()); }
    g_assert_no_error (error); retained.disconnected ();
    gchar *before = nullptr; g_object_get (editor.model.get (), "painter-draft-name", &before, nullptr);
    const auto snapshot = editor.snapshot ().encode ();
    auto *container = gimp_data_factory_get_container (gimp->painter_mybrush_factory);
    const auto resources = gimp_container_get_n_children (container); const auto dialogs = editor_dialog_count ();
    retained.emit_late ();
    gchar *after = nullptr; g_object_get (editor.model.get (), "painter-draft-name", &after, nullptr);
    g_assert_cmpstr (before, ==, after); g_free (before); g_free (after);
    g_assert_true (editor.snapshot ().encode () == snapshot);
    g_assert_cmpint (gimp_container_get_n_children (container), ==, resources);
    g_assert_cmpuint (editor_dialog_count (), ==, dialogs);
    retained.disconnected ();
  }
}
static void direct_control_emitter_first () {
  prepare ();
  for (bool rebuild : {false, true}) {
    Editor editor; DirectEditorControls retained (editor);
    for (auto& emitter : retained.controls) gtk_widget_destroy (GTK_WIDGET (emitter.get ()));
    retained.disconnected ();
    if (rebuild) {
      auto context = ObjectRef<GObject>::adopt (G_OBJECT (gimp_context_new (gimp, "direct emitter first", nullptr)));
      GError *error = nullptr;
      g_assert_true (gimp_painter_mybrush_editor_set_context (editor.widget, GIMP_CONTEXT (context.get ()), &error));
      g_assert_no_error (error); DirectEditorControls rebuilt (editor);
      g_assert_true (gimp_painter_binding_close (G_OBJECT (editor.widget), &error));
      g_assert_no_error (error); rebuilt.disconnected ();
    } else g_assert_true (gimp_painter_binding_close (G_OBJECT (editor.widget), nullptr));
    retained.emit_late (); retained.disconnected ();
  }
}
static void direct_control_callback_close () {
  prepare ();
  // Notify is emitted synchronously from inside the name changed callback.
  {
    Editor editor; DirectEditorControls retained (editor);
    struct State { GimpPainterMybrushEditor *editor; bool fired = false; } state {editor.widget};
    auto owner = ObjectRef<GObject>::retain (G_OBJECT (editor.widget));
    auto h = g_signal_connect (editor.model.get (), "notify::painter-draft-name", G_CALLBACK (+[](GObject *, GParamSpec *, gpointer p) {
      auto& s = *static_cast<State *> (p); if (s.fired) return; s.fired = true;
      g_assert_true (gimp_painter_binding_close (G_OBJECT (s.editor), nullptr));
    }), &state);
    gtk_entry_set_text (GTK_ENTRY (retained.controls[0].get ()), "Name closes its own editor");
    g_signal_handler_disconnect (editor.model.get (), h); g_assert_true (state.fired); retained.disconnected (); retained.emit_late ();
  }
  // Save's synchronous factory-add observer closes the owner inside clicked.
  {
    Editor editor; DirectEditorControls retained (editor);
    gtk_entry_set_text (GTK_ENTRY (retained.controls[0].get ()), "Direct save callback close fixture");
    auto owner = ObjectRef<GObject>::retain (G_OBJECT (editor.widget));
    struct State { GimpPainterMybrushEditor *editor; GimpPainterMybrush *saved = nullptr; } state {editor.widget};
    auto *container = gimp_data_factory_get_container (gimp->painter_mybrush_factory);
    auto h = g_signal_connect (container, "add", G_CALLBACK (+[](GimpContainer *, GimpObject *object, gpointer p) {
      auto& s = *static_cast<State *> (p); if (s.saved) return;
      s.saved = GIMP_PAINTER_MYBRUSH (g_object_ref (object));
      g_assert_true (gimp_painter_binding_close (G_OBJECT (s.editor), nullptr));
    }), &state);
    gtk_button_clicked (GTK_BUTTON (retained.controls[1].get ()));
    g_signal_handler_disconnect (container, h); g_assert_nonnull (state.saved); retained.disconnected (); retained.emit_late ();
    remove_prompt_fixture (editor, state.saved); g_object_unref (state.saved);
  }
  // Observe a real post-class show signal: never destroy GTK mid class handler.
  {
    auto options = ObjectRef<GimpPainterMybrushOptions>::adopt (GIMP_PAINTER_MYBRUSH_OPTIONS (g_object_new (GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", gimp, nullptr)));
    seed_prompt_fixture (options.get (), "Direct delete callback fixture");
    auto saved = ObjectRef<GimpPainterMybrush>::retain (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options.get ())));
    Editor editor (false, GIMP_CONTEXT (options.get ())); DirectEditorControls retained (editor);
    auto owner = ObjectRef<GObject>::retain (G_OBJECT (editor.widget));
    struct State { GimpPainterMybrushEditor *editor; bool fired = false; bool connected = false; } state {editor.widget};
    const auto signal = g_signal_lookup ("show", GTK_TYPE_WIDGET);
    const auto hook = g_signal_add_emission_hook (signal, 0, +[](GSignalInvocationHint *, guint, const GValue *values, gpointer data) -> gboolean {
      auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
      auto& state = *static_cast<State *> (data);
      auto *dialog = gtk_widget_get_toplevel (widget);
      // Attach while a child is shown, before the dialog emission snapshots
      // its handlers. Adding during the dialog emission itself is too late.
      if (GTK_IS_DIALOG (dialog) && widget != dialog && !state.connected) {
        state.connected = true;
        g_signal_connect_after (dialog, "show", G_CALLBACK (+[](GtkWidget *, gpointer p) {
        auto& s = *static_cast<State *> (p); if (s.fired) return; s.fired = true;
        g_assert_true (gimp_painter_binding_close (G_OBJECT (s.editor), nullptr));
        }), data);
      }
      return TRUE;
    }, &state, nullptr);
    gtk_button_clicked (GTK_BUTTON (retained.controls[2].get ()));
    g_signal_remove_emission_hook (signal, hook); g_assert_true (state.fired); retained.disconnected (); retained.emit_late ();
    remove_prompt_fixture (editor, saved.get ());
  }
}

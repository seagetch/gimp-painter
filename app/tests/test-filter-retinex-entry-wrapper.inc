/* SPDX-License-Identifier: GPL-3.0-or-later */
void retinex_checked_handles ()
{
  GimpPainter::FilterEditorSchema schema (application, GimpPainter::FilterProcedure::retinex);
  g_assert_true (schema.current (application));
  auto image = image_new (); auto filter = filter_new (image.get ());
  auto args = checked_arguments ();
  g_value_set_int (&g_array_index (args.get (), GValue, 3), 16);
  auto *tail = &g_array_index (args.get (), GValue, 6);
  g_value_unset (tail); g_value_init (tail, G_TYPE_DOUBLE); g_value_set_double (tail, 0.123456789012345);
  GError *error = nullptr;
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_atomic_int_set (&legacy_calls, 0);
#endif
  g_assert_true (filter.try_set_legacy_procedure ("plug-in-retinex", args.get (), &error));
  g_assert_no_error (error);
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_assert_cmpint (g_atomic_int_get (&legacy_calls), ==, 1);
  g_atomic_int_set (&legacy_calls, 0);
#endif
  filter.set_legacy_procedure ("plug-in-retinex", nullptr);
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_assert_cmpint (g_atomic_int_get (&legacy_calls), ==, 1);
#endif
  auto values = filter.get_procedure_args ();
  g_assert_cmpuint (values.get ()->len, ==, 7);
  for (guint i = 0; i < 6; ++i)
    g_assert_cmpint (g_value_get_int (&g_array_index (values.get (), GValue, i)), ==, 0);
  g_assert_cmpuint (G_VALUE_TYPE (&g_array_index (values.get (), GValue, 6)), ==, G_TYPE_DOUBLE);
  g_assert_cmpfloat (g_value_get_double (&g_array_index (values.get (), GValue, 6)), ==, 0);
}
struct RetinexTransformState {
  GimpFilterLayer *filter;
  GArray *args;
  char *name;
  guint action, calls = 0;
};
RetinexTransformState *retinex_transform_state;
void retinex_transform (const GValue *source, GValue *destination)
{
  auto& state = *retinex_transform_state;
  g_assert_cmpuint (++state.calls, ==, 1);
  g_value_set_int (destination, static_cast<gint> (g_value_get_double (source)));
  g_value_set_double (&g_array_index (state.args, GValue, 6), 99);
  std::strcpy (state.name, "caller changed");
  switch (state.action) {
  case 0: break;
  case 1: g_assert_true (gimp_filter_layer_set_definition (state.filter, "newer retinex definition", nullptr, nullptr, nullptr)); break;
  case 2: g_assert_true (gimp_painter_binding_close (G_OBJECT (state.filter), nullptr)); break;
  case 3: application->plug_in_manager = nullptr; break;
  case 4: application->pdb = nullptr; break;
  case 5: g_value_unset (destination); g_value_init (destination, G_TYPE_STRING); g_value_set_static_string (destination, "not an int"); break;
  }
}
void retinex_checked_reentry ()
{
  if (!g_test_subprocess ()) {
    g_test_trap_subprocess (nullptr, 60 * G_TIME_SPAN_SECOND, G_TEST_SUBPROCESS_DEFAULT);
    g_test_trap_assert_passed (); return;
  }
  retire_startup_plugins ();
  g_value_register_transform_func (G_TYPE_DOUBLE, G_TYPE_INT, retinex_transform);
  for (guint action = 0; action < 6; ++action) {
    auto image = image_new (); auto filter = filter_new (image.get ());
    filter.set_definition ("before retinex transform", nullptr, nullptr);
    auto args = checked_arguments ();
    auto *first = &g_array_index (args.get (), GValue, 0);
    g_value_unset (first); g_value_init (first, G_TYPE_DOUBLE); g_value_set_double (first, 12.75);
    g_value_set_int (&g_array_index (args.get (), GValue, 3), 16);
  auto *tail = &g_array_index (args.get (), GValue, 6);
  g_value_unset (tail); g_value_init (tail, G_TYPE_DOUBLE); g_value_set_double (tail, 0.123456789012345);
    char name[32] = "plug-in-retinex";
    RetinexTransformState state {filter.get (), args.get (), name, action}; retinex_transform_state = &state;
    auto *manager = application->plug_in_manager; auto *pdb = application->pdb;
    GError *error = nullptr;
    const auto result = filter.try_set_legacy_procedure (name, args.get (), &error);
    application->plug_in_manager = manager; application->pdb = pdb;
    g_assert_cmpuint (state.calls, ==, 1);
    if (action == 0) {
      g_assert_true (result); g_assert_no_error (error);
      auto values = filter.get_procedure_args (); g_assert_cmpuint (values.get ()->len, ==, 7);
      g_assert_cmpint (g_value_get_int (&g_array_index (values.get (), GValue, 0)), ==, 12);
      g_assert_cmpint (g_value_get_int (&g_array_index (values.get (), GValue, 3)), ==, 16);
      g_assert_cmpfloat (g_value_get_double (&g_array_index (values.get (), GValue, 6)), ==, 0.123456789012345);
      auto saved = filter.get_procedure (); g_assert_cmpstr (saved.get (), ==, "plug-in-retinex");
    } else {
      g_assert_false (result); g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE); g_clear_error (&error);
      auto saved = filter.get_procedure (); g_assert_cmpstr (saved.get (), ==, action == 1 ? "newer retinex definition" : "before retinex transform");
    }
  }
}

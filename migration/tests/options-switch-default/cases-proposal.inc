/* Prepared only: requires exact current committed Options/provider link. */
static void switch_default_metadata () {
  auto model=ObjectRef<GimpPainterMybrushOptions>::adopt(GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr)));
  auto*spec=g_object_class_find_property(G_OBJECT_GET_CLASS(model.get()),"use-gimp-brushmark");g_assert_true(G_IS_PARAM_SPEC_BOOLEAN(spec));
  const bool declared=G_PARAM_SPEC_BOOLEAN(spec)->default_value;
  // Set the original metadata default explicitly to separate constructor policy.
  g_object_set(model.get(),"use-gimp-brushmark",TRUE,nullptr);
  GString*text=g_string_new(nullptr);auto*writer=gimp_config_writer_new_from_string(text);
  g_assert_true(gimp_config_serialize_changed_properties(GIMP_CONFIG(model.get()),writer));
  GError*error=nullptr;g_assert_true(gimp_config_writer_finish(writer,nullptr,&error));g_assert_no_error(error);
  bool emitted=strstr(text->str,"(use-gimp-brushmark yes)")!=nullptr;g_string_free(text,TRUE);
  gimp_config_reset_property(G_OBJECT(model.get()),"use-gimp-brushmark");gboolean reset=FALSE;g_object_get(model.get(),"use-gimp-brushmark",&reset,nullptr);
  g_test_message("old expectedTRUE; current declared=%d reset=%d defaultTRUE incorrectly serialized=%d",declared,reset,emitted);
  g_assert_true(declared);g_assert_true(reset);g_assert_false(emitted);
}

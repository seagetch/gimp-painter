data_duplicate_cmd_callback (GtkAction *action,
                             gpointer   user_data)
{
  GimpDataFactoryView *view = GIMP_DATA_FACTORY_VIEW (user_data);
  GimpContext         *context;
  GimpData            *data;

  context = gimp_container_view_get_context (GIMP_CONTAINER_EDITOR (view)->view);

  data = (GimpData *)
    gimp_context_get_by_type (context,
                              gimp_data_factory_view_get_children_type (view));

  if (data && gimp_data_factory_view_have (view,
                                           GIMP_OBJECT (data)))
    {
      GimpData *new_data;

      new_data = gimp_data_factory_data_duplicate (gimp_data_factory_view_get_data_factory (view), data);

      if (new_data)
        {
          gimp_context_set_by_type (context,
                                    gimp_data_factory_view_get_children_type (view),
                                    GIMP_OBJECT (new_data));

          gtk_button_clicked (GTK_BUTTON (gimp_data_factory_view_get_edit_button (view)));
        }
    }
}

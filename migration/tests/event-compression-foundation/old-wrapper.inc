gboolean
gimp_display_shell_canvas_tool_events (GtkWidget        *canvas,
                                       GdkEvent         *event,
                                       GimpDisplayShell *shell)
{
  GdkEvent *next_event = NULL;
  gboolean  return_val;

  g_return_val_if_fail (gtk_widget_get_realized (canvas), FALSE);

  return_val = gimp_display_shell_canvas_tool_events_internal (canvas,
                                                               event, shell,
                                                               &next_event);

  if (next_event)
    {
      gtk_main_do_event (next_event);

      gdk_event_free (next_event);
    }

  return return_val;
}

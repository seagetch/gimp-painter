static void
gimp_display_shell_space_pressed (GimpDisplayShell *shell,
                                  const GdkEvent   *event)
{
  Gimp *gimp = gimp_display_get_gimp (shell->display);

  if (shell->space_pressed)
    return;

  if (! gimp_display_shell_keyboard_grab (shell, event))
    return;

  switch (shell->display->config->space_bar_action)
    {
    case GIMP_SPACE_BAR_ACTION_NONE:
      break;

    case GIMP_SPACE_BAR_ACTION_PAN:
      {
        GimpDeviceManager *manager;
        GimpDeviceInfo    *current_device;
        GimpCoords         coords;
        gdouble start_x;
        gdouble start_y;

        manager = gimp_devices_get_manager (gimp);
        current_device = gimp_device_manager_get_current_device (manager);

        gimp_device_info_get_device_coords (current_device,
                                            gtk_widget_get_window (shell->canvas),
                                            &coords);
        start_x = coords.x;
        start_y = coords.y;
        gimp_display_shell_device_to_image_coords (shell, &start_x, &start_y);
        
        gimp_display_shell_start_scrolling (shell, event,
                                            start_x, start_y);
      }
      break;

    case GIMP_SPACE_BAR_ACTION_MOVE:
      {
        GimpTool *active_tool = tool_manager_get_active (gimp);

        if (active_tool || ! GIMP_IS_MOVE_TOOL (active_tool))
          {
            GdkModifierType state;

            shell->space_shaded_tool =
              gimp_object_get_name (active_tool->tool_info);

            gimp_context_set_tool (gimp_get_user_context (gimp),
                                   gimp_get_tool_info (gimp, "gimp-move-tool"));

            gdk_event_get_state (event, &state);

            gimp_display_shell_update_focus (shell, TRUE,
                                             NULL, state);
          }
      }
      break;
    }

  shell->space_pressed = TRUE;
}

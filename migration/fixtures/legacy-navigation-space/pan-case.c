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


/* Source-only PAN branch adapter. Device lookup returns explicit test coords;
 * inverse and integer start boundary are the unchanged legacy-pan oracle.
 * No old GTK application/device runtime is invoked. */
#define GimpDeviceManager void
#define GimpDeviceInfo void
#define gimp_devices_get_manager(g) ((void *) 1)
#define gimp_device_manager_get_current_device(m) ((void)(m), (void *) 1)
#define gimp_device_info_get_device_coords(d,w,c) ((void)(d), (void)(w), *(c) = *input)
#define gtk_widget_get_window(w) (w)
#define gimp_display_shell_device_to_image_coords legacy_pan_inverse
#define gimp_display_shell_start_scrolling legacy_pan_begin
static void legacy_space_pan_begin(LegacyPanShell *shell, const GdkEvent *event,
                                  const GimpCoords *input)
{
  switch (GIMP_SPACE_BAR_ACTION_PAN)
    {
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

    }
}
#undef GimpDeviceManager
#undef GimpDeviceInfo
#undef gimp_devices_get_manager
#undef gimp_device_manager_get_current_device
#undef gimp_device_info_get_device_coords
#undef gtk_widget_get_window
#undef gimp_display_shell_device_to_image_coords
#undef gimp_display_shell_start_scrolling

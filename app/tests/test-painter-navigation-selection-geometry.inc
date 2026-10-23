/* Original selection undraw clauses: actual GTK invalidation, no final repaint. */
static void selection_window_clear(gconstpointer data)
{
  guint a=GPOINTER_TO_UINT(data);gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();GimpImage*image=gimp_display_get_image(shell->display);
  gimp_painter_canvas_ui_set_visible(shell,TRUE);gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  gimp_channel_select_rectangle(gimp_image_get_mask(image),48,54,120,96,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);gimp_image_flush(image);
  gimp_display_shell_selection_pause(shell);
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
      DragPoint start={shell->disp_width/2+40.25,shell->disp_height/2+60.25};
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});assert_navigation_idle();
      GdkPixbuf*base=grid_window_capture();gimp_display_shell_set_show_selection(shell,TRUE);
      gtk_widget_queue_draw(shell->canvas);GdkPixbuf*shown=grid_window_capture();
      guint visible=grid_window_difference(base,shown);g_assert_cmpuint(visible,>,0);
      gimp_display_shell_set_show_selection(shell,FALSE);GdkPixbuf*hidden=grid_window_capture();
      guint residual=grid_window_difference(base,hidden);
      g_test_message("selection hide angle=%g H=%u V=%u visible=%u residual=%u",drag_angles[a].angle,h,v,visible,residual);
      g_assert_cmpuint(residual,==,0);g_object_unref(base);g_object_unref(shown);g_object_unref(hidden);
    }
  gimp_display_shell_selection_resume(shell);draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

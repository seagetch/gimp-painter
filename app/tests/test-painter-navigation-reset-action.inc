/* Original25.010: activate the registered action, not its display helper. */
#include "widgets/gimpuimanager.h"
#include "widgets/gimpaction.h"
#include "display/gimpstatusbar.h"

static void
reset_action_activate (void)
{
  GimpUIManager *ui = gimp_test_utils_get_ui_manager (gimp);
  GimpAction *action;

  gimp_context_set_display (gimp_get_user_context (gimp), shell->display);
  gimp_ui_manager_update (ui, shell->display);
  action = gimp_ui_manager_find_action (ui, "view", "view-rotate-reset");
  g_assert_nonnull (action);
  g_assert_true (gimp_action_is_sensitive (action, NULL));
  g_assert_true (gimp_ui_manager_activate_action (ui, "view", "view-rotate-reset"));
}

static void
reset_action_assert_state (guint h, guint v, gdouble zoom)
{
  GimpStatusbar *status = gimp_display_shell_get_statusbar (shell);
  gint width, height;
  const gdouble points[][2] = {{0,0}, {128,128}, {233.125,17.875}};

  g_assert_cmpfloat (shell->rotate_angle, ==, 0.0);
  g_assert_false (shell->rotate_angle_exact);
  g_assert_false (shell->painter_origin_valid);
  g_assert_cmpfloat (shell->painter_pan_fraction_x, ==, 0.0);
  g_assert_cmpfloat (shell->painter_pan_fraction_y, ==, 0.0);
  g_assert_cmpint (shell->flip_horizontally, ==, h);
  g_assert_cmpint (shell->flip_vertically, ==, v);
  g_assert_cmpfloat (gimp_zoom_model_get_factor (shell->zoom), ==, zoom);
  g_assert_false (gtk_widget_get_visible (status->rotate_widget));
  g_assert_cmpint (gtk_widget_get_visible (status->horizontal_flip_icon), ==, h);
  g_assert_cmpint (gtk_widget_get_visible (status->vertical_flip_icon), ==, v);
  assert_navigation_idle ();
  gimp_display_shell_scale_get_image_size (shell, &width, &height);
  for (guint n=0; n<G_N_ELEMENTS(points); n++)
    {
      gdouble x, y, bx, by;
      gdouble ix=RINT(points[n][0]*shell->scale_x), iy=RINT(points[n][1]*shell->scale_y);
      gimp_display_shell_transform_xy_f (shell,points[n][0],points[n][1],&x,&y);
      g_assert_cmpfloat_with_epsilon (x,(h?width-ix:ix)-shell->offset_x,1e-9);
      g_assert_cmpfloat_with_epsilon (y,(v?height-iy:iy)-shell->offset_y,1e-9);
      gimp_display_shell_untransform_xy_f (shell,x,y,&bx,&by);
      g_assert_cmpfloat_with_epsilon (bx,ix/shell->scale_x,1e-9);
      g_assert_cmpfloat_with_epsilon (by,iy/shell->scale_y,1e-9);
    }
}

static void
reset_action_assert_pixels (GdkPixbuf *frame, guint h, guint v)
{
  gint width,height,channels=gdk_pixbuf_get_n_channels(frame);
  gimp_display_shell_scale_get_image_size(shell,&width,&height);
  for(guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)
    {
      const DrawLandmark *mark=&draw_landmarks[n];guint count=0;gdouble sumx=0,sumy=0;
      gdouble x=(mark->x+mark->w/2.0)*shell->scale_x;
      gdouble y=(mark->y+mark->h/2.0)*shell->scale_y;
      x=(h?width-x:x)-shell->offset_x;y=(v?height-y:y)-shell->offset_y;
      for(gint py=shell->disp_height/2-235;py<shell->disp_height/2+235;py++)
      for(gint px=shell->disp_width/2-235;px<shell->disp_width/2+235;px++)
        {
          guchar*p=gdk_pixbuf_get_pixels(frame)+py*gdk_pixbuf_get_rowstride(frame)+px*channels;
          if(p[n]>180&&p[(n+1)%3]<80&&p[(n+2)%3]<80)
            {count++;sumx+=px+0.5;sumy+=py+0.5;}
        }
      g_assert_cmpuint(count,>,4);
      g_assert_cmpfloat_with_epsilon(sumx/count,x,1.1);
      g_assert_cmpfloat_with_epsilon(sumy/count,y,1.1);
    }
}

static void
reset_action_window (gconstpointer data)
{
  guint flips=GPOINTER_TO_UINT(data),h=flips&1,v=flips>>1,scenes=0;
  const gdouble angles[]={0.0,37.0,90.0,180.0,359.999,-37.0};
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();
  gimp_painter_canvas_ui_set_visible(shell,TRUE);
  g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);
  path_damage_wait_layout();
  gimp_canvas_item_set_visible(scene.items,TRUE);
  gimp_modifiers_manager_set(manager,device,8,GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_STEP_ROTATING,NULL);
  for(guint entry=0;entry<2;entry++)for(guint a=0;a<G_N_ELEMENTS(angles);a++)
    {
      GimpStatusbar *status=gimp_display_shell_get_statusbar(shell);
      gdouble zoom=entry?1.25:1.0;
      gimp_display_shell_scale(shell,GIMP_ZOOM_TO,zoom,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,angles[a]);
      gimp_display_shell_scroll_set_offset(shell,RINT(128*shell->scale_x)-shell->disp_width/2,RINT(128*shell->scale_y)-shell->disp_height/2);
      if(entry)
        {
          DragPoint start={shell->disp_width/2+80.25,shell->disp_height/2+0.25};
          DragPoint pan={start.x+2.6,start.y-1.4},motion={shell->disp_width/2+0.25,shell->disp_height/2+80.25};
          drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();
          drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,pan);
          drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,pan);assert_navigation_idle();
          drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_SHIFT_MASK,start);assert_grab();
          drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK|GDK_SHIFT_MASK,motion);
          drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK|GDK_SHIFT_MASK,motion);assert_navigation_idle();
          g_assert_true(shell->rotate_angle_exact);
        }
      gdouble before=shell->rotate_angle;
      GdkPixbuf *shown=grid_window_capture();g_object_unref(shown);
      g_assert_cmpint(gtk_widget_get_visible(status->rotate_widget),==,before!=0.0);
      if(before!=0.0)
        {gchar*label=g_strdup_printf(" %.2f\xC2\xB0",before);g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(status->rotate_label)),==,label);g_free(label);}
      if(flips==0&&entry==0&&a==1)grid_damage_screenshot("rotation-reset","before-action");
      reset_action_activate();
      GdkPixbuf *after=grid_window_capture();reset_action_assert_state(h,v,zoom);reset_action_assert_pixels(after,h,v);
      if(flips==0&&entry==0&&a==1)grid_damage_screenshot("rotation-reset","after-action");
      gint ox=shell->offset_x,oy=shell->offset_y;
      reset_action_activate();GdkPixbuf *again=grid_window_capture();reset_action_assert_state(h,v,zoom);
      g_assert_cmpint(shell->offset_x,==,ox);g_assert_cmpint(shell->offset_y,==,oy);
      g_assert_cmpuint(grid_window_difference(after,again),==,0);
      const gchar *description=NULL;
      g_assert_cmpint(gimp_modifiers_manager_get_action(manager,device,8,GDK_MOD1_MASK,&description),==,GIMP_MODIFIER_ACTION_STEP_ROTATING);
      g_test_message("reset action H=%u V=%u entry=%u angle=%g before=%g after=0 zoom=%g repeat-residual=0",h,v,entry,angles[a],before,zoom);
      g_object_unref(after);g_object_unref(again);scenes++;
    }
  g_assert_cmpuint(scenes,==,12);draw_scene_free(&scene);
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

static void
reset_action_reentry (gconstpointer data)
{
  guint custom=GPOINTER_TO_UINT(data);gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  if(custom)gimp_modifiers_manager_set(manager,device,8,GDK_MOD1_MASK,GIMP_MODIFIER_ACTION_STEP_ROTATING,NULL);
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      guint button=custom?8:2,state=custom?GDK_MOD1_MASK:GDK_SHIFT_MASK;
      DragPoint start={shell->disp_width/2+80.0,shell->disp_height/2};
      DragPoint motion={shell->disp_width/2,shell->disp_height/2+80.0};
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,37.0);
      reset_action_activate();gimp_test_run_temp_mainloop(40);reset_action_assert_state(h,v,1.0);
      /* No intervening public rotate/scale/scroll can repair stale reset state. */
      drag_send_pointer(GDK_BUTTON_PRESS,button,state,start);assert_grab();
      g_assert_cmpint(shell->painter_navigation_inherited,==,!custom);
      drag_send_pointer(GDK_MOTION_NOTIFY,0,state|(custom?0:GDK_BUTTON2_MASK),motion);
      g_assert_true(isfinite(shell->rotate_angle));g_assert_cmpfloat(shell->rotate_angle,!=,0.0);
      gdouble angle=shell->rotate_angle;
      drag_send_pointer(GDK_BUTTON_RELEASE,button,state,motion);assert_navigation_idle();
      reset_action_activate();GdkPixbuf*frame=grid_window_capture();reset_action_assert_state(h,v,1.0);reset_action_assert_pixels(frame,h,v);g_object_unref(frame);
      g_test_message("reset reentry custom=%u H=%u V=%u moved-angle=%g reset=0",custom,h,v,angle);
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

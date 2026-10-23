/* Applied legacy operation geometry, independent of current view helpers. */
#include "../../migration/fixtures/legacy-navigation-applied/rotation-oracle.inc"
static void old_applied_rotation_origin(void)
{
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  for(guint angle=0;angle<G_N_ELEMENTS(drag_angles);angle++)
  for(guint anisotropic=0;anisotropic<2;anisotropic++)
  for(guint vertical=0;vertical<2;vertical++)
  for(guint mirror=0;mirror<2;mirror++)
    {
      DragScene scene=drag_scene_new(&drag_angles[angle],mirror,1.5,TRUE,real_tool_names[0]);
      LegacyAppliedRotation old={0};
      DragPoint start={scene.width/2+40.25,scene.height/2-0.75};
      DragPoint motion=start;
      const gint rays[][2]={{0,80},{-80,0},{80,-40},{0,-80},{40,-1},{0,80},{40,-1}};
      const guint states[]={GDK_SHIFT_MASK,GDK_SHIFT_MASK|GDK_CONTROL_MASK,
                            GDK_CONTROL_MASK|GDK_MOD1_MASK,0,0,GDK_SHIFT_MASK,0};
      gdouble ox,oy,x,y,actual_x,actual_y,initial_x,initial_y;
      GdkEventMotion event={0};
      if(vertical)
        {gimp_display_shell_flip(shell,mirror,TRUE);gimp_display_shell_rotate_to(shell,scene.angle->angle);}
      if(anisotropic)
        {gimp_image_set_resolution(gimp_display_get_image(shell->display),144,72);gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);}
      /* Enter rotation from a real previous fractional default pan. */
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);
      motion=(DragPoint){start.x+17.6,start.y-10.6};
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
      old.view.disp_width=scene.width;old.view.disp_height=scene.height;
      old.view.mirrored=mirror!=vertical;
      old.view.rotate_angle=(old.view.mirrored?-1:1)*(scene.angle->angle+(vertical?180.0:0.0));
      old.view.scale_x=shell->scale_x;old.view.scale_y=shell->scale_y;
      gimp_display_shell_transform_xy_f(shell,0,0,&ox,&oy);
      legacy_pan_inverse(&old.view,&ox,&oy);old.view.offset_x=-ox;old.view.offset_y=-oy;
      initial_x=old.view.offset_x;initial_y=old.view.offset_y;
      for(guint reentry=0;reentry<2;reentry++)
        {
          legacy_applied_rotation_begin(&old,NULL,start.x,start.y);
          drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_SHIFT_MASK,start);assert_grab();
          g_assert_true(shell->painter_navigation_inherited);
          for(guint step=0;step<G_N_ELEMENTS(rays);step++)
            {
              motion=(DragPoint){scene.width/2+rays[step][0]+0.25,scene.height/2+rays[step][1]+0.25};
              event.state=states[step]|GDK_BUTTON2_MASK;
              legacy_applied_rotation_motion(&old,(const GdkEvent *)&event,motion.x,motion.y);
              drag_send_pointer(GDK_MOTION_NOTIFY,0,event.state,motion);
              /* Allow genuine frame-clock/damage processing; do not repair
               * offsets, force allocation, or invalidate caches in this test. */
              gimp_test_run_temp_mainloop(20);
              for(guint point=0;point<3;point++)
                {
                  gdouble px=point==0?128:point==1?0:233.125;
                  gdouble py=point==0?128:point==1?0:17.875;
                  gdouble back_x,back_y;
                  x=RINT(px*old.view.scale_x)-old.view.offset_x;
                  y=RINT(py*old.view.scale_y)-old.view.offset_y;
                  {cairo_t*cr=legacy_pan_cairo();legacy_pan_set_rotate(&old.view,cr);cairo_user_to_device(cr,&x,&y);cairo_destroy(cr);}
                  gimp_display_shell_transform_xy_f(shell,px,py,&actual_x,&actual_y);
                  g_test_message("old applied rotation a=%u h=%u v=%u aniso=%u reentry=%u step=%u point=%u expected=%.17g,%.17g actual=%.17g,%.17g",angle,mirror,vertical,anisotropic,reentry,step,point,x,y,actual_x,actual_y);
                  g_assert_cmpfloat_with_epsilon(actual_x,x,1e-8);g_assert_cmpfloat_with_epsilon(actual_y,y,1e-8);
                  gimp_display_shell_untransform_xy_f(shell,actual_x,actual_y,&back_x,&back_y);
                  g_assert_cmpfloat_with_epsilon(back_x,RINT(px*old.view.scale_x)/old.view.scale_x,1e-8);
                  g_assert_cmpfloat_with_epsilon(back_y,RINT(py*old.view.scale_y)/old.view.scale_y,1e-8);
                }
            }
          legacy_applied_rotation_end(&old,NULL);g_assert_false(old.rotating);g_assert_cmpfloat(old.rotate_start_angle,==,0.0);
          drag_send_pointer(GDK_BUTTON_RELEASE,2,event.state,motion);assert_navigation_idle();
          g_assert_cmpfloat(shell->painter_rotate_origin_x,==,0.0);
          g_assert_cmpfloat(shell->painter_rotate_origin_y,==,0.0);
          g_assert_cmpfloat(old.view.offset_x,==,initial_x);g_assert_cmpfloat(old.view.offset_y,==,initial_y);
        }
      if(anisotropic)gimp_display_shell_scale_set_dot_for_dot(shell,TRUE);
      if(vertical)gimp_display_shell_flip(shell,mirror,FALSE);
      gimp_display_shell_rotate_to(shell,scene.angle->angle);
      gimp_display_shell_scroll_set_offset(shell,shell->offset_x,shell->offset_y);
      drag_scene_free(&scene);
    }
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Actual oncanvas rendering and upright overlay against the unchanged old matrix. */
static void old_applied_rotation_render (void)
{
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  for (guint mirror=0;mirror<2;mirror++)
    {
      DrawScene scene=draw_scene_new();
      LegacyAppliedRotation rotation={0};
      LegacyPanShell *old=&rotation.view;
      GdkEventMotion event={0};
      DragPoint start,motion;
      gdouble origin_x,origin_y,sx,sy;
      GtkWidget *widget=gtk_event_box_new();
      GimpOverlayChild *child;
      gimp_painter_canvas_ui_set_visible(shell,TRUE);
      g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
      gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);
      gimp_test_run_temp_mainloop(100);
      gimp_display_shell_flip(shell,mirror,FALSE);
      gimp_display_shell_rotate_to(shell,37.0);
      gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
      old->disp_width=shell->disp_width;old->disp_height=shell->disp_height;
      old->image_width=old->image_height=256;old->scale_x=old->scale_y=1.0;
      old->rotate_angle=mirror?-37.0:37.0;old->mirrored=mirror;
      gimp_display_shell_transform_xy_f(shell,0,0,&origin_x,&origin_y);
      legacy_pan_inverse(old,&origin_x,&origin_y);old->offset_x=-origin_x;old->offset_y=-origin_y;
      start=(DragPoint){shell->disp_width/2+31.25,shell->disp_height/2-18.75};
      legacy_applied_rotation_begin(&rotation,NULL,start.x,start.y);
      gtk_widget_set_size_request(widget,19,11);
      gimp_display_shell_add_overlay(shell,widget,40,220,GIMP_HANDLE_ANCHOR_NORTH_WEST,0,0);
      gtk_widget_show(widget);
      child=gimp_overlay_child_find(GIMP_OVERLAY_BOX(shell->canvas),widget);
      g_assert_nonnull(child);
      drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_SHIFT_MASK,start);assert_grab();
      for (guint step=0;step<3;step++)
        {
          cairo_surface_t *surface;
          gchar *name=g_strdup_printf("legacy-rotation-oncanvas-m%u-step%u",mirror,step);
          motion=(DragPoint){start.x-20.6-step*14.13,start.y+50.6+step*11.17};
          event.state=GDK_BUTTON2_MASK|GDK_SHIFT_MASK|(step==1?GDK_CONTROL_MASK:0);
          legacy_applied_rotation_motion(&rotation,(GdkEvent *)&event,motion.x,motion.y);
          drag_send_pointer(GDK_MOTION_NOTIFY,0,event.state,motion);
          surface=draw_snapshot(&scene,name);g_free(name);
          for (guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)
            {
              const DrawLandmark *mark=&draw_landmarks[n];
              gdouble ex=mark->x+mark->w/2.0-old->offset_x;
              gdouble ey=mark->y+mark->h/2.0-old->offset_y;
              gdouble sumx=0,sumy=0,radius=hypot(mark->w,mark->h)/2+2;
              guint count=0;
              cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(old,cr);
              cairo_user_to_device(cr,&ex,&ey);cairo_destroy(cr);
              /* Central 440x440 ROI contains both complete transformed image
               * positions (diagonal <363), with generous surrounding trails. */
              for (gint y=shell->disp_height/2-220;y<shell->disp_height/2+220;y++)
                for (gint x=shell->disp_width/2-220;x<shell->disp_width/2+220;x++)
                  {
                    guint32 pixel=draw_pixel(surface,x,y);
                    guint rgb[]={(pixel>>16)&255,(pixel>>8)&255,pixel&255};
                    if(rgb[n]>130 && rgb[(n+1)%3]<110 && rgb[(n+2)%3]<110)
                      {
                        g_assert_cmpfloat(hypot(x+0.5-ex,y+0.5-ey),<=,radius);
                        count++;sumx+=x+0.5;sumy+=y+0.5;
                      }
                  }
              g_assert_cmpfloat(count,>=,0.45*mark->w*mark->h);
              g_assert_cmpfloat(count,<=,1.50*mark->w*mark->h);
              g_assert_cmpfloat_with_epsilon(sumx/count,ex,1.0);
              g_assert_cmpfloat_with_epsilon(sumy/count,ey,1.0);
            }
          sx=40-old->offset_x;sy=220-old->offset_y;
          {cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(old,cr);cairo_user_to_device(cr,&sx,&sy);cairo_destroy(cr);}
          g_assert_cmpfloat_with_epsilon(child->x,sx,1e-8);
          g_assert_cmpfloat_with_epsilon(child->y,sy,1e-8);
          /* GTK allocation is integer; stored image anchor remains exact. */
          g_assert_cmpfloat_with_epsilon(child->matrix.x0,sx,1.0);
          g_assert_cmpfloat_with_epsilon(child->matrix.y0,sy,1.0);
          g_assert_cmpfloat(child->angle,==,0.0);
          g_assert_cmpfloat_with_epsilon(child->matrix.xx,1.0,1e-10);
          g_assert_cmpfloat_with_epsilon(child->matrix.yy,1.0,1e-10);
          /* Actual item paint CTM, independent old origin, not just a center oracle. */
          origin_x=-shell->offset_x;origin_y=-shell->offset_y;
          cairo_matrix_transform_point(&scene.rotated->matrix,&origin_x,&origin_y);
          sx=-old->offset_x;sy=-old->offset_y;
          {cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(old,cr);cairo_user_to_device(cr,&sx,&sy);cairo_destroy(cr);}
          g_assert_cmpfloat_with_epsilon(origin_x,sx,1e-8);
          g_assert_cmpfloat_with_epsilon(origin_y,sy,1e-8);
          if(step==1 && g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR"))
            {
              GdkWindow *root=gdk_get_default_root_window();
              GdkPixbuf *shot=gdk_pixbuf_get_from_window(root,0,0,
                gdk_window_get_width(root),gdk_window_get_height(root));
              gchar *name=g_strdup_printf("portrait-oncanvas-rotation-m%u.png",mirror);
              gchar *path=g_build_filename(g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR"),name,NULL);
              g_assert_cmpint(gdk_window_get_width(root),==,1080);
              g_assert_cmpint(gdk_window_get_height(root),==,1920);
              g_assert_nonnull(shot);
              g_assert_true(gdk_pixbuf_save(shot,path,"png",NULL,NULL));
              g_object_unref(shot);g_free(name);g_free(path);
            }
          cairo_surface_destroy(surface);
        }
      legacy_applied_rotation_end(&rotation,NULL);
      drag_send_pointer(GDK_BUTTON_RELEASE,2,event.state,motion);assert_navigation_idle();
      gtk_widget_destroy(widget);
      draw_scene_free(&scene);
    }
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}


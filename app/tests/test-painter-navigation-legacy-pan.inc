/* Source-pinned legacy interior-pan geometry, not an old application run. */
#include "libgimpmath/gimpmath.h"
#include "../../migration/fixtures/legacy-pan/oracle.inc"
static void inherited_pan_fractional_origin (void)
{
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  for (guint angle_index=0;angle_index<G_N_ELEMENTS(drag_angles);angle_index++)
  for (guint anisotropic=0;anisotropic<2;anisotropic++)
  for (guint vertical=0;vertical<2;vertical++)
  for (guint mirror=0;mirror<2;mirror++)
    {
      DragScene scene=drag_scene_new(&drag_angles[angle_index],mirror,1.5,TRUE,real_tool_names[0]);
      LegacyPanShell old={0};
      DragPoint start={scene.width/2+31.25,scene.height/2-18.75};
      DragPoint motion={start.x+17.6,start.y-10.6};
      DragPoint origin=drag_forward(scene.angle,mirror,scene.scale,scene.ox,scene.oy,(DragPoint){0,0});
      gdouble sx=start.x,sy=start.y,expected_x,expected_y,actual_x,actual_y;
      cairo_t *cr;
      if (vertical)
        {
          gimp_display_shell_flip(shell,mirror,TRUE);
          /* Flip action reflects the angle too; establish the specified scene. */
          gimp_display_shell_rotate_to(shell,scene.angle->angle);
          gimp_display_shell_transform_xy_f(shell,0,0,&origin.x,&origin.y);
        }
      if (anisotropic)
        {
          gimp_image_set_resolution(gimp_display_get_image(shell->display),144.0,72.0);
          gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);
          gimp_test_run_mainloop_until_idle();
          g_assert_cmpfloat(shell->scale_x,!=,shell->scale_y);
          gimp_display_shell_transform_xy_f(shell,0,0,&origin.x,&origin.y);
        }
      old.image_width=256;old.image_height=256;old.scale_x=shell->scale_x;old.scale_y=shell->scale_y;
      old.disp_width=scene.width;old.disp_height=scene.height;
      /* Old H*R and current R*H represent the same displayed reflected angle
       * when the old angle is negated. Align the initial visible scene, not
       * the incompatible old/current offset field representations. */
      old.mirrored=mirror != vertical;
      old.rotate_angle=(old.mirrored?-1.0:1.0)*(scene.angle->angle+(vertical?180.0:0.0));
      legacy_pan_inverse(&old,&origin.x,&origin.y);old.offset_x=-origin.x;old.offset_y=-origin.y;
      legacy_pan_inverse(&old,&sx,&sy);legacy_pan_begin(&old,NULL,sx,sy);
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);
      g_assert_true(shell->painter_navigation_inherited);assert_grab();
      /* Same immutable old motion/clamp bodies, including subpixel repeats,
       * return to press, both overshoot directions and reverse after clamp. */
      for (guint step=0;step<80;step++)
        {
          if (step == 0) motion=(DragPoint){start.x+17.6,start.y-10.6};
          else if (step < 65) motion=(DragPoint){start.x+step*0.013,start.y-step*0.017};
          else if (step == 65 || step == 79) motion=start;
          else if (step == 66) motion=(DragPoint){start.x+20000.25,start.y-18000.75};
          else if (step == 67) motion=(DragPoint){start.x-20000.25,start.y+18000.75};
          else motion=(DragPoint){start.x+(step-68)*0.017,start.y+(step-68)*0.019};
          legacy_pan_motion(&old,motion.x,motion.y);
          /* Modifier changes do not switch the latched old/default pan. */
          drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK |
            (step%3==1?GDK_SHIFT_MASK:step%3==2?GDK_CONTROL_MASK:0),motion);
          g_assert_cmpint(shell->mod_action,==,GIMP_MODIFIER_ACTION_PANNING);
          g_assert_true(shell->painter_navigation_inherited);
          for (guint point=0;point<3;point++)
            {
              gdouble px=point==0?128:point==1?0:233.125;
              gdouble py=point==0?128:point==1?0:17.875;
              gdouble back_x,back_y;
              /* The existing point API rounds scaled image coordinates before rotation. */
              expected_x=RINT(px*old.scale_x)-old.offset_x;expected_y=RINT(py*old.scale_y)-old.offset_y;
              cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);cairo_user_to_device(cr,&expected_x,&expected_y);cairo_destroy(cr);
              gimp_display_shell_transform_xy_f(shell,px,py,&actual_x,&actual_y);
              g_test_message("old pan mirror=%u step=%u point=%u expected %.17g,%.17g actual %.17g,%.17g",mirror,step,point,expected_x,expected_y,actual_x,actual_y);
              g_assert_cmpfloat_with_epsilon(actual_x,expected_x,1e-8);
              g_assert_cmpfloat_with_epsilon(actual_y,expected_y,1e-8);
              gimp_display_shell_untransform_xy_f(shell,actual_x,actual_y,&back_x,&back_y);
              g_assert_cmpfloat_with_epsilon(back_x,RINT(px*old.scale_x)/old.scale_x,1e-8);
              g_assert_cmpfloat_with_epsilon(back_y,RINT(py*old.scale_y)/old.scale_y,1e-8);
            }
        }
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
      if (anisotropic)
        {
          gimp_display_shell_scale_set_dot_for_dot(shell,TRUE);
          g_assert_cmpfloat(shell->painter_pan_fraction_x,==,0.0);
          g_assert_cmpfloat(shell->painter_pan_fraction_y,==,0.0);
        }
      if(vertical)
        {
          gimp_display_shell_flip(shell,mirror,FALSE);
          gimp_display_shell_rotate_to(shell,scene.angle->angle);
        }
      gimp_display_shell_scroll_set_offset(shell,shell->offset_x,shell->offset_y);
      g_assert_cmpfloat(shell->painter_pan_fraction_x,==,0.0);
      g_assert_cmpfloat(shell->painter_pan_fraction_y,==,0.0);
      drag_scene_free(&scene);
    }
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Separate oncanvas-enabled render check: historical whole-canvas detector
 * remains unchanged. This ROI covers the image and its nearby old/new trails,
 * excluding known screen-fixed toolbar pixels, not the image's former origin. */
static void inherited_pan_render (void)
{
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  for (guint mirror=0;mirror<2;mirror++)
    {
      DrawScene scene=draw_scene_new();
      LegacyPanShell old={0};
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
      old.disp_width=shell->disp_width;old.disp_height=shell->disp_height;
      old.image_width=old.image_height=256;old.scale_x=old.scale_y=1.0;
      old.rotate_angle=mirror?-37.0:37.0;old.mirrored=mirror;
      gimp_display_shell_transform_xy_f(shell,0,0,&origin_x,&origin_y);
      legacy_pan_inverse(&old,&origin_x,&origin_y);old.offset_x=-origin_x;old.offset_y=-origin_y;
      start=(DragPoint){shell->disp_width/2+31.25,shell->disp_height/2-18.75};
      sx=start.x;sy=start.y;legacy_pan_inverse(&old,&sx,&sy);legacy_pan_begin(&old,NULL,sx,sy);
      gtk_widget_set_size_request(widget,19,11);
      gimp_display_shell_add_overlay(shell,widget,40,220,GIMP_HANDLE_ANCHOR_NORTH_WEST,0,0);
      gtk_widget_show(widget);
      child=gimp_overlay_child_find(GIMP_OVERLAY_BOX(shell->canvas),widget);
      g_assert_nonnull(child);
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();
      for (guint step=0;step<3;step++)
        {
          cairo_surface_t *surface;
          gchar *name=g_strdup_printf("legacy-pan-oncanvas-m%u-step%u",mirror,step);
          motion=(DragPoint){start.x+17.6+step*0.13,start.y-10.6-step*0.17};
          legacy_pan_motion(&old,motion.x,motion.y);
          drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
          surface=draw_snapshot(&scene,name);g_free(name);
          for (guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)
            {
              const DrawLandmark *mark=&draw_landmarks[n];
              gdouble ex=mark->x+mark->w/2.0-old.offset_x;
              gdouble ey=mark->y+mark->h/2.0-old.offset_y;
              gdouble sumx=0,sumy=0,radius=hypot(mark->w,mark->h)/2+2;
              guint count=0;
              cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);
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
          sx=40-old.offset_x;sy=220-old.offset_y;
          {cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);cairo_user_to_device(cr,&sx,&sy);cairo_destroy(cr);}
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
          sx=-old.offset_x;sy=-old.offset_y;
          {cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);cairo_user_to_device(cr,&sx,&sy);cairo_destroy(cr);}
          g_assert_cmpfloat_with_epsilon(origin_x,sx,1e-8);
          g_assert_cmpfloat_with_epsilon(origin_y,sy,1e-8);
          if(step==1 && g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR"))
            {
              GdkWindow *root=gdk_get_default_root_window();
              GdkPixbuf *shot=gdk_pixbuf_get_from_window(root,0,0,
                gdk_window_get_width(root),gdk_window_get_height(root));
              gchar *name=g_strdup_printf("portrait-oncanvas-pan-m%u.png",mirror);
              gchar *path=g_build_filename(g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR"),name,NULL);
              g_assert_cmpint(gdk_window_get_width(root),==,1080);
              g_assert_cmpint(gdk_window_get_height(root),==,1920);
              g_assert_nonnull(shot);
              g_assert_true(gdk_pixbuf_save(shot,path,"png",NULL,NULL));
              g_object_unref(shot);g_free(name);g_free(path);
            }
          cairo_surface_destroy(surface);
        }
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
      gtk_widget_destroy(widget);
      draw_scene_free(&scene);
    }
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Native transition contracts are intentionally distinct from the exact old
 * pan oracle: explicit integer view operations quantize their new origin. */
static void inherited_pan_transitions (void)
{
  gboolean previous_ui;
  DrawScene scene;
  DragPoint start,motion;
  gdouble x0,y0,x1,y1,fx,fy;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gimp_display_shell_rotate_to(shell,37.0);
  start=(DragPoint){shell->disp_width/2+31.25,shell->disp_height/2-18.75};
  motion=(DragPoint){start.x+17.6,start.y-10.6};
  drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);
  drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
  drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
  g_assert_cmpfloat(shell->painter_pan_start_x,==,0.0);
  g_assert_cmpfloat(shell->painter_pan_start_y,==,0.0);
  fx=shell->painter_pan_fraction_x;fy=shell->painter_pan_fraction_y;
  g_assert_cmpfloat(fx+fy,>,0.0);
  gimp_display_shell_transform_xy_f(shell,128,128,&x0,&y0);
  gimp_modifiers_manager_set(manager,device,2,0,GIMP_MODIFIER_ACTION_PANNING,NULL);
  drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);
  g_assert_false(shell->painter_navigation_inherited);
  motion=(DragPoint){start.x+2,start.y+4};
  drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
  gimp_display_shell_transform_xy_f(shell,128,128,&x1,&y1);
  g_assert_cmpfloat_with_epsilon(x1,x0+2,1e-8);
  g_assert_cmpfloat_with_epsilon(y1,y0+4,1e-8);
  g_assert_cmpfloat(shell->painter_pan_fraction_x,==,fx);
  g_assert_cmpfloat(shell->painter_pan_fraction_y,==,fy);
  drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
  gimp_modifiers_manager_clear(manager);
  /* Explicit integer set-offset clears even when integer fields are equal. */
  gimp_display_shell_scroll_set_offset(shell,shell->offset_x,shell->offset_y);
  gimp_display_shell_transform_xy_f(shell,128,128,&x0,&y0);
  g_assert_cmpfloat_with_epsilon(x0,x1+fx,1e-8);
  g_assert_cmpfloat_with_epsilon(y0,y1+fy,1e-8);
  g_assert_cmpfloat(shell->painter_pan_fraction_x,==,0.0);
  g_assert_cmpfloat(shell->painter_pan_fraction_y,==,0.0);
  for(guint transition=0;transition<3;transition++)
    {
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);
      g_assert_true(shell->painter_navigation_inherited);
      motion=(DragPoint){start.x+17.6,start.y-10.6};
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
      g_assert_cmpfloat(shell->painter_pan_fraction_x+shell->painter_pan_fraction_y,>,0.0);
      if(transition==0)
        gimp_display_shell_scale_by_values(shell,1.5,shell->offset_x,shell->offset_y,FALSE);
      else if(transition==1)
        gimp_display_shell_rotate_to(shell,90.0);
      else
        {
          gimp_image_set_resolution(gimp_display_get_image(shell->display),144.0,72.0);
          gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);
        }
      g_assert_cmpfloat(shell->painter_pan_fraction_x,==,0.0);
      g_assert_cmpfloat(shell->painter_pan_fraction_y,==,0.0);
    }
  draw_scene_free(&scene);
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

static void inherited_pan_invalid_and_space (void)
{
  const gdouble invalid[]={NAN,INFINITY,-INFINITY,G_MAXDOUBLE,-G_MAXDOUBLE,
                           (gdouble)G_MAXINT+1.0,(gdouble)G_MININT-1.0};
  DrawScene scene=draw_scene_new();
  DragPoint start={shell->disp_width/2+31.25,shell->disp_height/2-18.75};
  NavigationState saved=navigation_state();
  gint old_space;
  for(guint n=0;n<G_N_ELEMENTS(invalid);n++)
    {
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,(DragPoint){invalid[n],start.y});
      assert_navigation_unchanged(&saved);assert_navigation_idle();
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,(DragPoint){start.x,invalid[n]});
      assert_navigation_unchanged(&saved);assert_navigation_idle();
    }
  gimp_display_shell_rotate_to(shell,37.0);
  /* Internal helper also rejects an invalid matrix before any integer cast,
   * independently of the normal event-entry validation above. */
  shell->rotate_angle=NAN;
  g_assert_false(gimp_display_shell_painter_pan_begin(shell,start.x,start.y));
  shell->rotate_angle=INFINITY;
  g_assert_false(gimp_display_shell_painter_pan_begin(shell,start.x,start.y));
  shell->rotate_angle=37.0;
  g_assert_cmpfloat(shell->painter_pan_start_x,==,0.0);
  g_assert_cmpfloat(shell->painter_pan_start_y,==,0.0);
  /* Raw coordinates fit int but old inverse does not: reject before grab. */
  drag_send_pointer(GDK_BUTTON_PRESS,2,0,(DragPoint){G_MAXINT-1.0,G_MAXINT-1.0});
  assert_navigation_idle();
  drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();saved=navigation_state();
  for(guint n=0;n<G_N_ELEMENTS(invalid);n++)
    {
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,(DragPoint){invalid[n],start.y});
      assert_navigation_unchanged(&saved);assert_grab();
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,(DragPoint){start.x,invalid[n]});
      assert_navigation_unchanged(&saved);assert_grab();
    }
  /* Escape does not invent a rollback/cancel policy for the latched pan. */
  send_event(GDK_KEY_PRESS,0,GDK_BUTTON2_MASK,0,0,GDK_KEY_Escape);
  assert_navigation_unchanged(&saved);assert_grab();
  drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,start);assert_navigation_idle();
  g_assert_cmpfloat(shell->painter_pan_start_x,==,0.0);
  g_assert_cmpfloat(shell->painter_pan_start_y,==,0.0);
  /* The old bounded checkpoint documented native dynamic Space behavior.
   * The subsequent pinned18-duty audit proved that configured Space PAN is
   * part of the original compatibility criterion. Historical receipts remain
   * unchanged; current behavior is fixed pan, with the exact old-source
   * geometry matrix in old-space/origin (including modifier transitions). */
  g_object_get(shell->display->config,"space-bar-action",&old_space,NULL);
  g_object_set(shell->display->config,"space-bar-action",GIMP_SPACE_BAR_ACTION_PAN,NULL);
  send_event(GDK_KEY_PRESS,0,0,0,0,GDK_KEY_space);
  g_assert_true(shell->space_release_pending);assert_grab();
  g_assert_true(shell->painter_navigation_inherited);
  {
    gdouble angle=shell->rotate_angle,scale=gimp_zoom_model_get_factor(shell->zoom);
    DragPoint motion={shell->scroll_last_x+7.25,shell->scroll_last_y+5.25};
    const guint states[]={0,GDK_SHIFT_MASK,GDK_CONTROL_MASK,0};
    for(guint n=0;n<G_N_ELEMENTS(states);n++)
      {
        drag_send_pointer(GDK_MOTION_NOTIFY,0,states[n],(DragPoint){motion.x+n*3.25,motion.y+n*2.25});
        g_assert_true(shell->painter_navigation_inherited);assert_grab();
        g_assert_cmpfloat(shell->rotate_angle,==,angle);
        g_assert_cmpfloat(gimp_zoom_model_get_factor(shell->zoom),==,scale);
      }
  }
  send_event(GDK_KEY_RELEASE,0,0,0,0,GDK_KEY_space);assert_navigation_idle();
  g_assert_false(shell->space_release_pending);
  g_object_set(shell->display->config,"space-bar-action",old_space,NULL);
  draw_scene_free(&scene);
}

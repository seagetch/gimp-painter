/* Applied zoom fixture is independent pinned old source, not native geometry. */
#include "../../migration/fixtures/legacy-navigation-applied/zoom-oracle.inc"

static LegacyAppliedZoom
old_zoom_view (void)
{
  LegacyAppliedZoom old={0};
  LegacyPanShell view={0};
  gdouble ox,oy;
  GimpImage *image=gimp_display_get_image(shell->display);
  old.disp_width=shell->disp_width;old.disp_height=shell->disp_height;
  old.image_width=gimp_image_get_width(image);old.image_height=gimp_image_get_height(image);
  old.mirrored=shell->flip_horizontally!=shell->flip_vertically;
  old.rotate_angle=(old.mirrored?-1:1)*(shell->rotate_angle+(shell->flip_vertically?180.0:0.0));
  old.rotate_angle=fmod(old.rotate_angle,360.0);if(old.rotate_angle<0)old.rotate_angle+=360.0;
  old.scale_x=shell->scale_x;old.scale_y=shell->scale_y;
  old.factor=gimp_zoom_model_get_factor(shell->zoom);old.dot_for_dot=shell->dot_for_dot;
  old.monitor_xres=shell->monitor_xres;old.monitor_yres=shell->monitor_yres;
  gimp_image_get_resolution(image,&old.image_xres,&old.image_yres);
  g_object_get(shell->zoom,"minimum",&old.minimum,"maximum",&old.maximum,NULL);
  view.disp_width=old.disp_width;view.disp_height=old.disp_height;
  view.rotate_angle=old.rotate_angle;view.mirrored=old.mirrored;
  gimp_display_shell_transform_xy_f(shell,0,0,&ox,&oy);
  legacy_pan_inverse(&view,&ox,&oy);old.offset_x=-ox;old.offset_y=-oy;
  return old;
}

static void
old_zoom_assert (LegacyAppliedZoom *old,guint scene,guint entry,guint step)
{
  LegacyPanShell view={0};
  view.disp_width=old->disp_width;view.disp_height=old->disp_height;
  view.rotate_angle=old->rotate_angle;view.mirrored=old->mirrored;
  g_assert_cmpfloat_with_epsilon(gimp_zoom_model_get_factor(shell->zoom),old->factor,1e-12);
  g_assert_cmpfloat_with_epsilon(shell->scale_x,old->scale_x,1e-12);
  g_assert_cmpfloat_with_epsilon(shell->scale_y,old->scale_y,1e-12);
  for(guint point=0;point<3;point++)
    {
      gdouble px=point==0?128:point==1?0:233.125;
      gdouble py=point==0?128:point==1?0:17.875;
      gdouble x=RINT(px*old->scale_x)-old->offset_x,y=RINT(py*old->scale_y)-old->offset_y;
      gdouble actual_x,actual_y,back_x,back_y;
      cairo_t*cr=legacy_pan_cairo();legacy_pan_set_rotate(&view,cr);
      cairo_user_to_device(cr,&x,&y);cairo_destroy(cr);
      gimp_display_shell_transform_xy_f(shell,px,py,&actual_x,&actual_y);
      g_test_message("old applied zoom scene=%u entry=%u step=%u point=%u factor=%.17g focus=%d,%d target=%d,%d origin=%.17g,%.17g expected=%.17g,%.17g actual=%.17g,%.17g",scene,entry,step,point,old->factor,old->trace.focus_x,old->trace.focus_y,old->trace.target_x,old->trace.target_y,old->offset_x,old->offset_y,x,y,actual_x,actual_y);
      g_assert_cmpfloat_with_epsilon(actual_x,x,1e-8);g_assert_cmpfloat_with_epsilon(actual_y,y,1e-8);
      gimp_display_shell_untransform_xy_f(shell,actual_x,actual_y,&back_x,&back_y);
      g_assert_cmpfloat_with_epsilon(back_x,RINT(px*old->scale_x)/old->scale_x,1e-8);
      g_assert_cmpfloat_with_epsilon(back_y,RINT(py*old->scale_y)/old->scale_y,1e-8);
    }
}

static void
old_zoom_sequence (guint angle,gboolean mirror,gboolean vertical,gboolean aniso,
                   gdouble initial,gint start_radius,const gint *radii,guint n_radii,guint scene_id)
{
  DragScene scene=drag_scene_new(&drag_angles[angle],mirror,initial,TRUE,real_tool_names[0]);
  LegacyAppliedZoom old;
  LegacyAZPointerInput input={0};
  DragPoint start={scene.width/2+start_radius,scene.height/2},motion=start;
  if(vertical){gimp_display_shell_flip(shell,mirror,TRUE);gimp_display_shell_rotate_to(shell,scene.angle->angle);}
  if(aniso){gimp_image_set_resolution(gimp_display_get_image(shell->display),144,72);gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);}
  old=old_zoom_view();
  for(guint entry=0;entry<2;entry++)
    {
      gdouble anchor=MAX(old.scale_x,old.scale_y)*300-start_radius;
      drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_CONTROL_MASK,start);assert_grab();
      g_assert_true(shell->painter_navigation_inherited);
      g_assert_cmpfloat_with_epsilon(shell->painter_zoom_anchor,anchor,1e-10);
      for(guint step=0;step<n_radii;step++)
        {
          gdouble requested=(radii[step]+anchor)/300.0;
          guint state=GDK_BUTTON2_MASK|(step%2?GDK_SHIFT_MASK:GDK_CONTROL_MASK);
          motion=(DragPoint){scene.width/2+radii[step],scene.height/2};
          input=(LegacyAZPointerInput){scene.width/2+23,scene.height/2-17,LEGACY_AZ_EVENT_CANVAS,TRUE,scene.width/2+23,scene.height/2-17};
          if(shell->zoom_focus_point){g_slice_free(GdkPoint,shell->zoom_focus_point);shell->zoom_focus_point=NULL;}
          gimp_display_shell_push_zoom_focus_pointer_pos(shell,input.queued_x,input.queued_y);
          legacy_applied_zoom_apply(&old,requested,LEGACY_AZ_RETAIN_CENTERING,input);
          drag_send_pointer(GDK_MOTION_NOTIFY,0,state,motion);
          g_assert_cmpint(shell->zoom_focus_point==NULL,==,old.queue_consumed);
          if(scene_id==999)g_test_message("extreme before draw: requested=%.17g applied=%.17g nativeoffset=%d,%d residual=%.17g,%.17g oldoffset=%.17g,%.17g",requested,gimp_zoom_model_get_factor(shell->zoom),shell->offset_x,shell->offset_y,shell->painter_pan_fraction_x,shell->painter_pan_fraction_y,old.offset_x,old.offset_y);
          gimp_test_run_temp_mainloop(20);
          old_zoom_assert(&old,scene_id,entry,step);
        }
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
      g_assert_cmpfloat(shell->painter_zoom_anchor,==,0.0);
    }
  if(shell->zoom_focus_point){g_slice_free(GdkPoint,shell->zoom_focus_point);shell->zoom_focus_point=NULL;}
  if(aniso)gimp_display_shell_scale_set_dot_for_dot(shell,TRUE);
  if(vertical)gimp_display_shell_flip(shell,mirror,FALSE);
  gimp_display_shell_rotate_to(shell,scene.angle->angle);
  gimp_display_shell_scale_by_values(shell,scene.scale,shell->offset_x,shell->offset_y,FALSE);
  drag_scene_free(&scene);
}

static void old_applied_zoom_origin(void)
{
  const gint radii[]={40,80,0,150,40};guint scene=0;
  for(guint a=0;a<G_N_ELEMENTS(drag_angles);a++)
  for(guint mirror=0;mirror<2;mirror++)
  for(guint vertical=0;vertical<2;vertical++)
  for(guint aniso=0;aniso<2;aniso++)
    old_zoom_sequence(a,mirror,vertical,aniso,1.5,40,radii,G_N_ELEMENTS(radii),scene++);
}

static void old_applied_zoom_limits(void)
{
  const gint low[]={0,0,800},high[]={400,400,40},atmax[]={400,40,40};
  for(guint mirror=0;mirror<2;mirror++)
    {
      old_zoom_sequence(1,mirror,FALSE,FALSE,1.25,800,low,G_N_ELEMENTS(low),mirror);
      old_zoom_sequence(1,mirror,FALSE,FALSE,255.75,40,high,G_N_ELEMENTS(high),2+mirror);
      old_zoom_sequence(1,mirror,FALSE,FALSE,256,40,atmax,G_N_ELEMENTS(atmax),4+mirror);
    }
}

static void old_applied_zoom_render (void)
{
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  for (guint mirror=0;mirror<2;mirror++)
    {
      DrawScene scene=draw_scene_new();
      LegacyAppliedZoom zoom={0};
      LegacyPanShell view={0};
      LegacyPanShell *old=&view;
      LegacyAZPointerInput input={0};
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
      zoom=old_zoom_view();
      old->disp_width=zoom.disp_width;old->disp_height=zoom.disp_height;
      old->mirrored=zoom.mirrored;old->rotate_angle=zoom.rotate_angle;
      start=(DragPoint){shell->disp_width/2+40,shell->disp_height/2};
      gtk_widget_set_size_request(widget,19,11);
      gimp_display_shell_add_overlay(shell,widget,40,220,GIMP_HANDLE_ANCHOR_NORTH_WEST,0,0);
      gtk_widget_show(widget);
      child=gimp_overlay_child_find(GIMP_OVERLAY_BOX(shell->canvas),widget);
      g_assert_nonnull(child);
      drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_CONTROL_MASK,start);assert_grab();
      for (guint step=0;step<3;step++)
        {
          cairo_surface_t *surface;
          gchar *name=g_strdup_printf("legacy-zoom-oncanvas-m%u-step%u",mirror,step);
          const gint radii[]={55,20,40};
          motion=(DragPoint){shell->disp_width/2+radii[step],shell->disp_height/2};
          input=(LegacyAZPointerInput){shell->disp_width/2+23,shell->disp_height/2-17,LEGACY_AZ_EVENT_CANVAS,TRUE,shell->disp_width/2+23,shell->disp_height/2-17};
          gimp_display_shell_push_zoom_focus_pointer_pos(shell,input.queued_x,input.queued_y);
          legacy_applied_zoom_apply(&zoom,(radii[step]+300.0-40)/300.0,LEGACY_AZ_RETAIN_CENTERING,input);
          drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
          old->offset_x=zoom.offset_x;old->offset_y=zoom.offset_y;
          old->scale_x=zoom.scale_x;old->scale_y=zoom.scale_y;
          surface=draw_snapshot(&scene,name);g_free(name);
          for (guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)
            {
              const DrawLandmark *mark=&draw_landmarks[n];
              gdouble ex=(mark->x+mark->w/2.0)*old->scale_x-old->offset_x;
              gdouble ey=(mark->y+mark->h/2.0)*old->scale_y-old->offset_y;
              gdouble sumx=0,sumy=0,radius=hypot(mark->w*old->scale_x,mark->h*old->scale_y)/2+2;
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
              g_assert_cmpfloat(count,>=,0.45*mark->w*mark->h*old->scale_x*old->scale_y);
              g_assert_cmpfloat(count,<=,1.50*mark->w*mark->h*old->scale_x*old->scale_y);
              g_assert_cmpfloat_with_epsilon(sumx/count,ex,1.0);
              g_assert_cmpfloat_with_epsilon(sumy/count,ey,1.0);
            }
          sx=RINT(40*old->scale_x)-old->offset_x;sy=RINT(220*old->scale_y)-old->offset_y;
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
              gchar *name=g_strdup_printf("portrait-oncanvas-zoom-m%u.png",mirror);
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


/* Kept as a separate regression: the earlier80,000px synthetic motion caused
 * a real GTK/Cairo drawing failure. Ordinary clamp coverage does not replace it. */
static void old_applied_zoom_extreme_pointer(void)
{
  const gint radii[]={80000,40,40};
  old_zoom_sequence(1,TRUE,FALSE,FALSE,256,40,radii,G_N_ELEMENTS(radii),999);
}

#include "core/gimpimage-resize.h"
static void old_zoom_prime_origin(void)
{
  gdouble x,y;
  g_assert_true(gimp_display_shell_painter_origin_get(shell,&x,&y));
  gimp_display_shell_painter_origin_remember(shell,x,y);
  g_assert_true(shell->painter_origin_valid);
}

static void old_applied_zoom_invalidation(void)
{
  DragScene scene=drag_scene_new(&drag_angles[1],FALSE,1.5,TRUE,real_tool_names[0]);
  GimpImage*image=gimp_display_get_image(shell->display);
  GtkWindow*window=GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell)));
  gint ww,wh;
  gdouble x,y;
  old_zoom_prime_origin();gimp_display_shell_scroll(shell,0,0);g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gimp_display_shell_scroll_set_offset(shell,shell->offset_x,shell->offset_y);g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gimp_display_shell_rotate_to(shell,shell->rotate_angle);g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gimp_display_shell_flip(shell,FALSE,FALSE);g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gimp_display_shell_scale(shell,GIMP_ZOOM_TO,1.5,GIMP_ZOOM_FOCUS_BEST_GUESS);g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gimp_display_shell_scale_by_values(shell,1.5,shell->offset_x,shell->offset_y,FALSE);g_assert_false(shell->painter_origin_valid);
  /* Moving away and back does not resurrect provenance merely because the
   * final numeric view resembles an earlier key. */
  old_zoom_prime_origin();
  {gint ox=shell->offset_x,oy=shell->offset_y;gimp_display_shell_scroll(shell,1,-1);gimp_display_shell_scroll_set_offset(shell,ox,oy);}
  g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gtk_window_get_size(window,&ww,&wh);
  gtk_window_resize(window,ww+17,wh+19);gimp_test_run_temp_mainloop(100);
  g_assert_false(shell->painter_origin_valid);
  gtk_window_resize(window,ww,wh);gimp_test_run_temp_mainloop(100);
  g_assert_false(shell->painter_origin_valid);
  old_zoom_prime_origin();gimp_image_resize(image,gimp_get_user_context(gimp),257,259,0,0,NULL);
  /* Image signals may eagerly invalidate; the full key must reject even if
   * a particular notification path has not yet run. */
  g_assert_true(gimp_display_shell_painter_origin_get(shell,&x,&y));g_assert_false(shell->painter_origin_valid);
  gimp_image_resize(image,gimp_get_user_context(gimp),256,256,0,0,NULL);gimp_test_run_temp_mainloop(50);
  gimp_display_shell_scale_by_values(shell,1.5,shell->offset_x,shell->offset_y,FALSE);
  drag_scene_free(&scene);
}

static void old_applied_zoom_invalid_inputs(void)
{
  DragScene scene=drag_scene_new(&drag_angles[1],FALSE,1.5,TRUE,real_tool_names[0]);
  const gdouble invalid[]={NAN,INFINITY,-INFINITY,G_MAXDOUBLE,-G_MAXDOUBLE};
  gint ox=shell->offset_x,oy=shell->offset_y;
  gdouble fx=shell->painter_pan_fraction_x,fy=shell->painter_pan_fraction_y;
  for(guint n=0;n<G_N_ELEMENTS(invalid);n++)
    {
      gimp_display_shell_scale_painter(shell,invalid[n]);
      g_assert_cmpfloat(gimp_zoom_model_get_factor(shell->zoom),==,1.5);
      g_assert_cmpint(shell->offset_x,==,ox);g_assert_cmpint(shell->offset_y,==,oy);
      g_assert_cmpfloat(shell->painter_pan_fraction_x,==,fx);g_assert_cmpfloat(shell->painter_pan_fraction_y,==,fy);
    }
  /* The internal entry rejects an unrealized canvas before any GDK pointer
   * lookup or view mutation; no event loop observes the temporary test widget. */
  {GtkWidget*saved=shell->canvas;GtkWidget*unrealized=gtk_drawing_area_new();g_object_ref_sink(unrealized);
   shell->canvas=unrealized;g_assert_false(gtk_widget_get_realized(unrealized));
   gimp_display_shell_scale_painter(shell,2.0);shell->canvas=saved;g_object_unref(unrealized);}
  g_assert_cmpfloat(gimp_zoom_model_get_factor(shell->zoom),==,1.5);
  g_assert_cmpint(shell->offset_x,==,ox);g_assert_cmpint(shell->offset_y,==,oy);
  drag_scene_free(&scene);
}

#include "display/gimpcanvas-style.h"
#include "core/gimplayer.h"
#include "display/gimpcanvaslayerboundary.h"
/* The actual layer-boundary stroke must preserve the original repeating
 * stipple pixels, including phase under rotated and mirrored transforms. */
static void old_applied_zoom_boundary_pattern(void)
{
  DragScene scene=drag_scene_new(&drag_angles[0],FALSE,1.0,FALSE,real_tool_names[0]);
  const gdouble boxes[][4]={{20,20,100,100},{-100,30,200,100},{-100,-100,200,200},
    {-100,-100,400,400},{30,-100,100,200},{250,30,100,100},
    {-5000,120,5100,20},{120,-5000,20,5100}};
  const gdouble angles[]={0,37,90};
  GimpCanvasItem*item=gimp_canvas_layer_boundary_new(shell);
  GList*layers=g_list_prepend(NULL,scene.drawable);
  guint comparisons=0;
  g_assert_true(GIMP_IS_LAYER(scene.drawable));
  gimp_canvas_layer_boundary_set_layers(GIMP_CANVAS_LAYER_BOUNDARY(item),layers);
  for(guint a=0;a<G_N_ELEMENTS(angles);a++)
  for(guint mirror=0;mirror<2;mirror++)
  for(guint n=0;n<G_N_ELEMENTS(boxes);n++)
    {
      cairo_surface_t*surfaces[2];
      for(guint reference=0;reference<2;reference++)
        {
          cairo_t*cr;
          surfaces[reference]=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,256,256);
          cr=cairo_create(surfaces[reference]);cairo_rectangle(cr,0,0,256,256);cairo_clip(cr);
          cairo_translate(cr,128,128);cairo_rotate(cr,angles[a]/180.0*G_PI);
          if(mirror)cairo_scale(cr,-1,1);cairo_translate(cr,-128,-128);
          cairo_rectangle(cr,boxes[n][0]+0.5,boxes[n][1]+0.5,boxes[n][2]-1,boxes[n][3]-1);
          cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND);
          if(reference)
            {
              gimp_canvas_set_layer_style(shell->canvas,cr,GIMP_LAYER(scene.drawable),shell->offset_x,shell->offset_y);
              cairo_stroke(cr);
            }
          else
            GIMP_CANVAS_ITEM_GET_CLASS(item)->stroke(item,cr);
          g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);
          cairo_destroy(cr);cairo_surface_flush(surfaces[reference]);
        }
      g_test_message("layer stipple original-path pixels angle=%g mirror=%u box=%u",angles[a],mirror,n);
      g_assert_cmpmem(cairo_image_surface_get_data(surfaces[0]),256*256*4,
                      cairo_image_surface_get_data(surfaces[1]),256*256*4);
      cairo_surface_destroy(surfaces[0]);cairo_surface_destroy(surfaces[1]);comparisons++;
    }
  g_assert_cmpuint(comparisons,==,48);
  g_list_free(layers);g_object_unref(item);
  drag_scene_free(&scene);
}

static void old_applied_zoom_mixed_chain(void)
{
  for(guint mirror=0;mirror<2;mirror++)
    {
      DragScene scene=drag_scene_new(&drag_angles[1],mirror,1.5,TRUE,real_tool_names[0]);
      LegacyAppliedZoom old=old_zoom_view();
      DragPoint start={scene.width/2+40,scene.height/2},motion;
      for(guint cycle=0;cycle<2;cycle++)
        {
          LegacyPanShell pan={0};
          gdouble sx=start.x,sy=start.y;
          pan.disp_width=old.disp_width;pan.disp_height=old.disp_height;
          pan.image_width=old.image_width;pan.image_height=old.image_height;
          pan.rotate_angle=old.rotate_angle;pan.mirrored=old.mirrored;
          pan.offset_x=old.offset_x;pan.offset_y=old.offset_y;
          pan.scale_x=old.scale_x;pan.scale_y=old.scale_y;
          legacy_pan_inverse(&pan,&sx,&sy);legacy_pan_begin(&pan,NULL,sx,sy);
          drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);
          motion=(DragPoint){start.x+17.6,start.y-10.6};
          legacy_pan_motion(&pan,motion.x,motion.y);
          drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
          drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
          old.offset_x=pan.offset_x;old.offset_y=pan.offset_y;
          old_zoom_assert(&old,1000+mirror,cycle,0);
          if(cycle==0)
            {
              LegacyAppliedRotation rotation={0};GdkEventMotion event={0};
              rotation.view=pan;rotation.view.scrolling=FALSE;
              legacy_applied_rotation_begin(&rotation,NULL,start.x,start.y);
              drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_SHIFT_MASK,start);
              motion=(DragPoint){scene.width/2-20,scene.height/2+70};event.state=GDK_BUTTON2_MASK;
              legacy_applied_rotation_motion(&rotation,(GdkEvent*)&event,motion.x,motion.y);
              drag_send_pointer(GDK_MOTION_NOTIFY,0,event.state,motion);
              legacy_applied_rotation_end(&rotation,NULL);
              drag_send_pointer(GDK_BUTTON_RELEASE,2,event.state,motion);assert_navigation_idle();
              old.rotate_angle=rotation.view.rotate_angle;
              old_zoom_assert(&old,1000+mirror,cycle,1);
            }
          {
            LegacyAZPointerInput input={scene.width/2+23,scene.height/2-17,LEGACY_AZ_EVENT_CANVAS,TRUE,scene.width/2+23,scene.height/2-17};
            gdouble requested=(MAX(old.scale_x,old.scale_y)*300-40+85)/300;
            drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_CONTROL_MASK,start);
            gimp_display_shell_push_zoom_focus_pointer_pos(shell,input.queued_x,input.queued_y);
            legacy_applied_zoom_apply(&old,requested,LEGACY_AZ_RETAIN_CENTERING,input);
            motion=(DragPoint){scene.width/2+85,scene.height/2};
            drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK|GDK_CONTROL_MASK,motion);
            drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
            gimp_test_run_temp_mainloop(20);old_zoom_assert(&old,1000+mirror,cycle,2);
          }
        }
      gimp_display_shell_rotate_to(shell,scene.angle->angle);
      gimp_display_shell_scale_by_values(shell,scene.scale,shell->offset_x,shell->offset_y,FALSE);
      drag_scene_free(&scene);
    }
}

static void old_applied_zoom_focus_fallback(void)
{
  DragScene scene=drag_scene_new(&drag_angles[1],TRUE,1.5,TRUE,real_tool_names[0]);
  LegacyAppliedZoom old=old_zoom_view();
  for(guint n=0;n<2;n++)
    {
      LegacyAZPointerInput input={0};
      gdouble requested=n?1.8:1.7;
      input.event_origin=LEGACY_AZ_EVENT_NONE;
      if(n==0)
        {
          input.queued=TRUE;input.queued_x=-50;input.queued_y=scene.height+50;
          gimp_display_shell_push_zoom_focus_pointer_pos(shell,input.queued_x,input.queued_y);
        }
      else
        {
          GdkDisplay*d=gtk_widget_get_display(shell->canvas);GdkDevice*p=gdk_seat_get_pointer(gdk_display_get_default_seat(d));
          gdk_window_get_device_position(gtk_widget_get_window(shell->canvas),p,&input.pointer_x,&input.pointer_y,NULL);
        }
      legacy_applied_zoom_apply(&old,requested,LEGACY_AZ_RETAIN_CENTERING,input);
      gimp_display_shell_scale_painter(shell,requested);
      g_assert_cmpint(shell->zoom_focus_point==NULL,==,TRUE);
      gimp_test_run_temp_mainloop(20);old_zoom_assert(&old,1010,0,n);
    }
  gimp_display_shell_scale_by_values(shell,scene.scale,shell->offset_x,shell->offset_y,FALSE);
  drag_scene_free(&scene);
}

static void old_applied_zoom_resize_preference(void)
{
  DragScene scene=drag_scene_new(&drag_angles[1],FALSE,1.5,TRUE,real_tool_names[0]);
  gdouble minimum,maximum;
  GtkWindow*window=GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell)));
  gint ww,wh;gtk_window_get_size(window,&ww,&wh);
  const gdouble requests[]={1.75,512,-2};
  const gdouble expected[]={1.75,2,1};
  g_object_get(shell->zoom,"minimum",&minimum,"maximum",&maximum,NULL);
  g_object_set(shell->zoom,"minimum",1.0,"maximum",2.0,NULL);
  g_object_set(shell->display->config,"resize-windows-on-zoom",TRUE,NULL);
  gimp_display_shell_push_zoom_focus_pointer_pos(shell,23,29);
  for(guint n=0;n<G_N_ELEMENTS(requests);n++)
    {
      old_zoom_prime_origin();
      gimp_display_shell_scale_painter(shell,requests[n]);
      g_assert_cmpfloat(gimp_zoom_model_get_factor(shell->zoom),==,expected[n]);
      g_assert_nonnull(shell->zoom_focus_point);
      g_assert_cmpint(shell->zoom_focus_point->x,==,23);
      g_assert_cmpint(shell->zoom_focus_point->y,==,29);
      g_assert_false(shell->painter_origin_valid);
      gimp_test_run_temp_mainloop(30);
    }
  g_object_set(shell->display->config,"resize-windows-on-zoom",FALSE,NULL);
  {LegacyAppliedZoom old=old_zoom_view();LegacyAZPointerInput input={0,0,LEGACY_AZ_EVENT_NONE,TRUE,23,29};
   legacy_applied_zoom_apply(&old,1.25,LEGACY_AZ_RETAIN_CENTERING,input);
   gimp_display_shell_scale_painter(shell,1.25);old_zoom_assert(&old,1011,0,0);}
  g_object_set(shell->zoom,"minimum",minimum,"maximum",maximum,NULL);
  gimp_display_shell_scale_by_values(shell,scene.scale,shell->offset_x,shell->offset_y,FALSE);
  gtk_window_resize(window,ww,wh);gimp_test_run_temp_mainloop(100);
  drag_scene_free(&scene);
}

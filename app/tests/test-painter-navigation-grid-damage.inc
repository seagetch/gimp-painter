/* Parent25.009: native item primitives under an independent old viewport CTM.
 * This component checks primitive rendering and projected extents only;
 * real GDK-window damage/clear validation is a separate required component. */
#include "core/gimpgrid.h"
#include "display/gimpcanvasgrid.h"
static const gchar*grid_family_names[]={"grid"};
static const guint grid_family_variants[]={10};
static GimpCanvasItem *grid_geometry_new(guint family,guint variant)
{
  (void)family;
  GimpGrid*grid=g_object_new(GIMP_TYPE_GRID,"xspacing",31.0,"yspacing",23.0,"xoffset",7.0,"yoffset",11.0,"style",variant%5,NULL);
  GimpCanvasItem*item=gimp_canvas_grid_new(shell,grid);g_object_unref(grid);
  g_object_set(item,"grid-style",variant/5,NULL);return item;
}
static cairo_t *grid_geometry_old_ctm(cairo_surface_t*surface,LegacyAppliedZoom*view)
{
  LegacyPanShell old={0};cairo_t*cr=cairo_create(surface);
  old.disp_width=view->disp_width;old.disp_height=view->disp_height;
  old.rotate_angle=view->rotate_angle;old.mirrored=view->mirrored;
  legacy_pan_set_rotate(&old,cr);
  /* Primitive-local coordinates already include native integer scrolling;
   * substitute the independent old double origin before old rotation. */
  cairo_translate(cr,shell->offset_x-view->offset_x,shell->offset_y-view->offset_y);
  return cr;
}
static cairo_region_t *grid_geometry_damage(GimpCanvasItem*item,LegacyAppliedZoom*view)
{
  cairo_region_t*local=gimp_canvas_item_get_extents(item),*device_region=cairo_region_create();
  cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*cr=grid_geometry_old_ctm(s,view);
  if(local)
    for(gint n=0;n<cairo_region_num_rectangles(local);n++)
      {
        cairo_rectangle_int_t r,b;gdouble xs[4],ys[4],minx=G_MAXDOUBLE,miny=G_MAXDOUBLE,maxx=-G_MAXDOUBLE,maxy=-G_MAXDOUBLE;
        cairo_region_get_rectangle(local,n,&r);
        for(guint p=0;p<4;p++)
          {xs[p]=r.x+(p&1?r.width:0);ys[p]=r.y+(p&2?r.height:0);cairo_user_to_device(cr,&xs[p],&ys[p]);minx=MIN(minx,xs[p]);maxx=MAX(maxx,xs[p]);miny=MIN(miny,ys[p]);maxy=MAX(maxy,ys[p]);}
        b.x=floor(minx-1);b.y=floor(miny-1);b.width=ceil(maxx+1)-b.x;b.height=ceil(maxy+1)-b.y;cairo_region_union_rectangle(device_region,&b);
      }
  if(local)cairo_region_destroy(local);cairo_destroy(cr);cairo_surface_destroy(s);return device_region;
}
static void grid_geometry_family(gconstpointer data)
{
  guint family=0;(void)data;gboolean previous_ui;
  DrawScene scene;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);gimp_test_run_temp_mainloop(100);
  for(guint a=0;a<3;a++)for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      LegacyAppliedZoom view;gint width=shell->disp_width,height=shell->disp_height;
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,128-width/2,128-height/2);
      view=old_zoom_view();
      for(guint variant=0;variant<grid_family_variants[family];variant++)
        {
          GimpCanvasItem*item=grid_geometry_new(family,variant);cairo_surface_t*actual,*expected;cairo_t*cr;cairo_region_t*damage;guint ink=0;
          actual=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);expected=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
          cr=cairo_create(actual);if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);gimp_canvas_item_draw(item,cr);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
          cr=grid_geometry_old_ctm(expected,&view);gimp_canvas_item_draw(item,cr);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
          cairo_surface_flush(actual);cairo_surface_flush(expected);damage=grid_geometry_damage(item,&view);
          for(gint y=height/2-235;y<height/2+235;y++)for(gint x=width/2-235;x<width/2+235;x++)
            {
              guint32 p=draw_pixel(actual,x,y),q=draw_pixel(expected,x,y);
              for(guint shift=0;shift<32;shift+=8)g_assert_cmpint(ABS((gint)((p>>shift)&255)-(gint)((q>>shift)&255)),<=,16);
              if((p>>24)>16){ink++;if(!cairo_region_contains_point(damage,x,y)){cairo_rectangle_int_t e;cairo_region_get_extents(damage,&e);g_error("uncovered family=%s variant=%u angle=%g H=%u V=%u pixel=%d,%d alpha=%u damage=%d,%d %dx%d",grid_family_names[family],variant,drag_angles[a].angle,h,v,x,y,p>>24,e.x,e.y,e.width,e.height);}}
            }
          g_test_message("item geometry family=%s variant=%u angle=%g H=%u V=%u ink=%u",grid_family_names[family],variant,drag_angles[a].angle,h,v,ink);
          g_assert_cmpuint(ink,>,0);
          cairo_region_destroy(damage);cairo_surface_destroy(actual);cairo_surface_destroy(expected);g_object_unref(item);
        }
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Read the server-backed canvas after normal GTK frame dispatch. Never invoke
 * gtk_widget_draw or force a full redraw between hide and the final capture. */
static GdkPixbuf *grid_window_capture(void)
{
  gimp_test_run_temp_mainloop(120);
  gdk_display_sync(gtk_widget_get_display(shell->canvas));
  GdkPixbuf*p=gdk_pixbuf_get_from_window(gtk_widget_get_window(shell->canvas),0,0,shell->disp_width,shell->disp_height);
  g_assert_nonnull(p);return p;
}
static guint grid_window_difference(GdkPixbuf*a,GdkPixbuf*b)
{
  guint count=0;gint w=gdk_pixbuf_get_width(a),h=gdk_pixbuf_get_height(a),n=gdk_pixbuf_get_n_channels(a);
  g_assert_cmpint(gdk_pixbuf_get_width(b),==,w);g_assert_cmpint(gdk_pixbuf_get_height(b),==,h);
  for(gint y=h/2-235;y<h/2+235;y++)for(gint x=w/2-235;x<w/2+235;x++)
    {
      guchar*p=gdk_pixbuf_get_pixels(a)+y*gdk_pixbuf_get_rowstride(a)+x*n;
      guchar*q=gdk_pixbuf_get_pixels(b)+y*gdk_pixbuf_get_rowstride(b)+x*n;
      if(ABS(p[0]-q[0])>2||ABS(p[1]-q[1])>2||ABS(p[2]-q[2])>2)count++;
    }
  return count;
}
static void grid_damage_screenshot(const gchar*family,const gchar*phase)
{
  const gchar*directory=g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR");
  if(directory)
    {
      GdkWindow*root=gdk_get_default_root_window();gint width=gdk_window_get_width(root),height=gdk_window_get_height(root);
      g_assert_cmpint(width,==,1080);g_assert_cmpint(height,==,1920);
      GdkPixbuf*p=gdk_pixbuf_get_from_window(root,0,0,width,height);
      gchar*path=g_strdup_printf("%s/item-geometry-%s-%s.png",directory,family,phase);GError*error=NULL;
      g_assert_nonnull(p);g_assert_true(gdk_pixbuf_save(p,path,"png",&error,NULL));g_assert_no_error(error);
      g_object_unref(p);g_free(path);
    }
}
static void grid_grid_real_damage(gconstpointer data)
{
  gboolean previous_ui;g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);gimp_test_run_temp_mainloop(120);
  gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
  guint a=GPOINTER_TO_UINT(data);guint family=0;
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
  {
  gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
  for(guint style=0;style<2;style++)for(guint variant=0;variant<5;variant++)
    {
      GimpCanvasItem*item=grid_geometry_new(family,variant+5*style);
      gimp_canvas_item_set_visible(item,FALSE);gimp_display_shell_add_item(shell,item);
      GdkPixbuf*base=grid_window_capture();
      gimp_canvas_item_set_visible(item,TRUE);
      /* Initial full exposure deliberately exercises ink at every visible edge;
       * hide below must clear it using only the item's real update region. */
      gtk_widget_queue_draw(shell->canvas);GdkPixbuf*shown=grid_window_capture();
      guint visible=grid_window_difference(base,shown);g_assert_cmpuint(visible,>,0);
      if(a==1&&!h&&!v&&!style&&variant==1)grid_damage_screenshot(grid_family_names[family],"shown");
      gimp_canvas_item_set_visible(item,FALSE);GdkPixbuf*hidden=grid_window_capture();
      guint residual=grid_window_difference(base,hidden);
      if(a==1&&!h&&!v&&!style&&variant==1)grid_damage_screenshot(grid_family_names[family],"hidden");
      g_test_message("real item damage family=%s angle=%g H=%u V=%u style=%u variant=%u visible=%u residual=%u",grid_family_names[family],drag_angles[a].angle,h,v,style,variant,visible,residual);
      g_assert_cmpuint(residual,==,0);
      g_object_unref(base);g_object_unref(shown);g_object_unref(hidden);gimp_display_shell_remove_item(shell,item);g_object_unref(item);
    }
  }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

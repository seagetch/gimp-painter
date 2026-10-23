/* Parent25.009: native item primitives under an independent old viewport CTM.
 * This component checks primitive rendering and projected extents only;
 * real GDK-window damage/clear validation is a separate required component. */
#include "core/gimpgrid.h"
#include "core/gimpbezierdesc.h"
#include "display/gimpcanvasarc.h"
#include "display/gimpcanvascorner.h"
#include "display/gimpcanvasgrid.h"
#include "display/gimpcanvasguide.h"
#include "display/gimpcanvashandle.h"
#include "display/gimpcanvaspath.h"
#include "display/gimpcanvasrectangleguides.h"
static const gchar*item_family_names[]={"arc","corner","grid","guide","handle","path","rectangle","rectangle-guides"};
static const guint item_family_variants[]={4,20,5,6,13,6,4,8};
static GimpCanvasItem *item_geometry_new(guint family,guint variant)
{
  switch(family)
    {
    case 0:return gimp_canvas_arc_new(shell,128,128,variant>1?95:55,variant>1?30:75,0.3,variant>1?2*G_PI:1.4*G_PI,variant%2);
    case 1:
      if(variant>=18)
        {GimpCanvasItem*item=g_object_new(GIMP_TYPE_CANVAS_CORNER,"shell",shell,NULL);
         if(variant==19){g_object_set(item,"anchor",GIMP_HANDLE_ANCHOR_NORTH_WEST,NULL);g_object_set(item,"anchor",GIMP_HANDLE_ANCHOR_CENTER,NULL);}return item;}
      return gimp_canvas_corner_new(shell,48,58,160,140,(GimpHandleAnchor)(variant%9),23,19,variant/9);
    case 2:{GimpGrid*grid=g_object_new(GIMP_TYPE_GRID,"xspacing",31.0,"yspacing",23.0,"xoffset",7.0,"yoffset",11.0,"style",variant,NULL);GimpCanvasItem*item=gimp_canvas_grid_new(shell,grid);g_object_unref(grid);return item;}
    case 3:return gimp_canvas_guide_new(shell,variant%2?GIMP_ORIENTATION_HORIZONTAL:GIMP_ORIENTATION_VERTICAL,variant/2==0?128:variant/2==1?0:256,GIMP_GUIDE_STYLE_NORMAL);
    case 4:return gimp_canvas_handle_new(shell,(GimpHandleType)variant,GIMP_HANDLE_ANCHOR_CENTER,128,128,35,27);
    case 5:{cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*cr=cairo_create(s);cairo_move_to(cr,48,75);cairo_curve_to(cr,90,10,190,245,208,72);cairo_line_to(cr,140,205);cairo_close_path(cr);cairo_path_t*p=cairo_copy_path(cr);GimpBezierDesc*b=gimp_bezier_desc_new(g_memdup2(p->data,p->num_data*sizeof(cairo_path_data_t)),p->num_data);GimpCanvasItem*item=gimp_canvas_path_new(shell,b,0,0,variant%2,(GimpPathStyle)(variant/2));gimp_bezier_desc_free(b);cairo_path_destroy(p);cairo_destroy(cr);cairo_surface_destroy(s);return item;}
    case 6:return gimp_canvas_rectangle_new(shell,variant>1?-200:45,variant>1?90:55,variant>1?380:165,variant>1?40:145,variant%2);
    case 7:return gimp_canvas_rectangle_guides_new(shell,40,45,175,165,(GimpGuidesType)variant,5);
    }
  g_assert_not_reached();return NULL;
}
static cairo_t *item_geometry_old_ctm(cairo_surface_t*surface,LegacyAppliedZoom*view)
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
static cairo_region_t *item_geometry_damage(GimpCanvasItem*item,LegacyAppliedZoom*view)
{
  cairo_region_t*local=gimp_canvas_item_get_extents(item),*device_region=cairo_region_create();
  cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*cr=item_geometry_old_ctm(s,view);
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
static void item_geometry_family(gconstpointer data)
{
  guint family=GPOINTER_TO_UINT(data);gboolean previous_ui;
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
      for(guint variant=0;variant<item_family_variants[family];variant++)
        {
          GimpCanvasItem*item=item_geometry_new(family,variant);cairo_surface_t*actual,*expected;cairo_t*cr;cairo_region_t*damage;guint ink=0;
          actual=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);expected=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,width,height);
          cr=cairo_create(actual);if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);gimp_canvas_item_draw(item,cr);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
          cr=item_geometry_old_ctm(expected,&view);gimp_canvas_item_draw(item,cr);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
          cairo_surface_flush(actual);cairo_surface_flush(expected);damage=item_geometry_damage(item,&view);
          for(gint y=height/2-235;y<height/2+235;y++)for(gint x=width/2-235;x<width/2+235;x++)
            {
              guint32 p=draw_pixel(actual,x,y),q=draw_pixel(expected,x,y);
              for(guint shift=0;shift<32;shift+=8)g_assert_cmpint(ABS((gint)((p>>shift)&255)-(gint)((q>>shift)&255)),<=,16);
              if((p>>24)>16){ink++;if(!cairo_region_contains_point(damage,x,y)){cairo_rectangle_int_t e;cairo_region_get_extents(damage,&e);g_error("uncovered family=%s variant=%u angle=%g H=%u V=%u pixel=%d,%d alpha=%u damage=%d,%d %dx%d",item_family_names[family],variant,drag_angles[a].angle,h,v,x,y,p>>24,e.x,e.y,e.width,e.height);}}
            }
          g_test_message("item geometry family=%s variant=%u angle=%g H=%u V=%u ink=%u",item_family_names[family],variant,drag_angles[a].angle,h,v,ink);
          if((family==7&&variant==GIMP_GUIDES_NONE)||(family==1&&(variant>=18||variant%9==GIMP_HANDLE_ANCHOR_CENTER)))
            {g_assert_cmpuint(ink,==,0);g_assert_true(cairo_region_is_empty(damage));}
          else if(family==7&&variant==GIMP_GUIDES_SPACING)g_assert_cmpuint(ink,==,0); /* old/current documented enum, unimplemented draw branch */
          else g_assert_cmpuint(ink,>,0);
          cairo_region_destroy(damage);cairo_surface_destroy(actual);cairo_surface_destroy(expected);g_object_unref(item);
        }
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Read the server-backed canvas after normal GTK frame dispatch. Never invoke
 * gtk_widget_draw or force a full redraw between hide and the final capture. */
static GdkPixbuf *item_window_capture(void)
{
  gimp_test_run_temp_mainloop(120);
  gdk_display_sync(gtk_widget_get_display(shell->canvas));
  GdkPixbuf*p=gdk_pixbuf_get_from_window(gtk_widget_get_window(shell->canvas),0,0,shell->disp_width,shell->disp_height);
  g_assert_nonnull(p);return p;
}
static guint item_window_difference(GdkPixbuf*a,GdkPixbuf*b)
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
static void item_damage_screenshot(const gchar*family,const gchar*phase)
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
static void item_grid_real_damage(gconstpointer data)
{
  gboolean previous_ui;g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);gimp_test_run_temp_mainloop(120);
  gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
  guint a=GPOINTER_TO_UINT(data)%3;guint family=GPOINTER_TO_UINT(data)>=3?4:2;
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
  {
  gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
  for(guint style=0;style<(family==2?2:1);style++)for(guint variant=0;variant<item_family_variants[family];variant++)
    {
      GimpCanvasItem*item=item_geometry_new(family,variant);if(family==2)g_object_set(item,"grid-style",style,NULL);
      gimp_canvas_item_set_visible(item,FALSE);gimp_display_shell_add_item(shell,item);
      GdkPixbuf*base=item_window_capture();
      gimp_canvas_item_set_visible(item,TRUE);
      /* Initial full exposure deliberately exercises ink at every visible edge;
       * hide below must clear it using only the item's real update region. */
      gtk_widget_queue_draw(shell->canvas);GdkPixbuf*shown=item_window_capture();
      guint visible=item_window_difference(base,shown);g_assert_cmpuint(visible,>,0);
      if(a==1&&!h&&!v&&!style&&variant==(family==2?1:3))item_damage_screenshot(item_family_names[family],"shown");
      gimp_canvas_item_set_visible(item,FALSE);GdkPixbuf*hidden=item_window_capture();
      guint residual=item_window_difference(base,hidden);
      if(a==1&&!h&&!v&&!style&&variant==(family==2?1:3))item_damage_screenshot(item_family_names[family],"hidden");
      g_test_message("real item damage family=%s angle=%g H=%u V=%u style=%u variant=%u visible=%u residual=%u",item_family_names[family],drag_angles[a].angle,h,v,style,variant,visible,residual);
      g_assert_cmpuint(residual,==,0);
      g_object_unref(base);g_object_unref(shown);g_object_unref(hidden);gimp_display_shell_remove_item(shell,item);g_object_unref(item);
    }
  }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Recording-surface ink bounds complement actual raster probes. Exhaustively
 * exercise handle-local angle/anchor/aspect before the common display CTM. */
static void item_handle_local_bounds(gconstpointer data)
{
  guint type=GPOINTER_TO_UINT(data),count=0;DrawScene scene=draw_scene_new();
  const gdouble angles[]={0.0,0.63,2.1};
  for(guint view_angle=0;view_angle<3;view_angle++)for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[view_angle].angle);
      LegacyAppliedZoom view=old_zoom_view();
      for(guint anchor=0;anchor<9;anchor++)for(guint aspect=0;aspect<2;aspect++)for(guint a=0;a<3;a++)
        {
          GimpCanvasItem*item=gimp_canvas_handle_new(shell,(GimpHandleType)type,(GimpHandleAnchor)anchor,128,128,aspect?27:35,aspect?35:27);
          gimp_canvas_handle_set_angles(item,angles[a],1.37);
          cairo_surface_t*surface=cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL);cairo_t*cr=cairo_create(surface);
          if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);
          gimp_canvas_item_draw(item,cr);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
          gdouble x,y,w,hh;cairo_recording_surface_ink_extents(surface,&x,&y,&w,&hh);
          cairo_region_t*region=item_geometry_damage(item,&view);cairo_rectangle_int_t bounds;cairo_region_get_extents(region,&bounds);
          if(x<bounds.x||y<bounds.y||x+w>bounds.x+bounds.width||y+hh>bounds.y+bounds.height)
            g_error("handle bounds type=%u anchor=%u aspect=%u local-angle=%g view=%g H=%u V=%u ink=%g,%g %gx%g damage=%d,%d %dx%d",type,anchor,aspect,angles[a],drag_angles[view_angle].angle,h,v,x,y,w,hh,bounds.x,bounds.y,bounds.width,bounds.height);
          g_assert_cmpfloat(w,>,0);g_assert_cmpfloat(hh,>,0);count++;
          cairo_region_destroy(region);cairo_surface_destroy(surface);g_object_unref(item);
        }
    }
  g_assert_cmpuint(count,==,648);g_test_message("handle type=%u local-bounds scenes=%u",type,count);
  draw_scene_free(&scene);
}

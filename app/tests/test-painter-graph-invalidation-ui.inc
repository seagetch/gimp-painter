/* SPDX-License-Identifier: GPL-3.0-or-later */
// Actual rendered app; captions disclose direct native API input (not gesture).
void capture_graph(GimpDisplay*d,int x,int y,const char*file,bool red)
{
 auto*shell=gimp_display_get_shell(d);auto*root=gdk_get_default_root_window();
 g_assert_cmpint(gdk_window_get_width(root),==,1080);g_assert_cmpint(gdk_window_get_height(root),==,1920);
 gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);
 double dx,dy;gimp_display_shell_transform_xy_f(shell,x,y,&dx,&dy);int ox,oy;gdk_window_get_origin(gtk_widget_get_window(shell->canvas),&ox,&oy);
 auto*shot=gdk_pixbuf_get_from_window(root,0,0,1080,1920);g_assert_nonnull(shot);int px=int(dx)+ox,py=int(dy)+oy;g_assert_cmpint(px,>=,0);g_assert_cmpint(px,<,1080);g_assert_cmpint(py,>=,0);g_assert_cmpint(py,<,1920);
 auto*p=gdk_pixbuf_get_pixels(shot)+py*gdk_pixbuf_get_rowstride(shot)+px*gdk_pixbuf_get_n_channels(shot);
 if(red)g_assert_cmpint(p[0],>,p[2]+100);else g_assert_cmpint(p[2],>,p[0]+100);
 if(auto*dir=g_getenv("GIMP_GRAPH_SCREENSHOTS")){auto*path=g_build_filename(dir,file,nullptr);GError*error=nullptr;g_assert_true(gdk_pixbuf_save(shot,path,"png",&error,nullptr));g_assert_no_error(error);g_free(path);}
 g_test_message("RAW_CANVAS %s 1080x1920 oncanvas=1 sample=%d,%d RGB=%d,%d,%d",file,px,py,p[0],p[1],p[2]);g_object_unref(shot);
}
void graph_visible(gconstpointer data)
{
 int mode=GPOINTER_TO_INT(data);Scene s;GimpLayer*shown=s.source;GimpFilterLayer*f=nullptr;
 if(mode==1){shown=gimp_clone_layer_new(s.image,s.source,256,192,"Clone source-node ROI",1,GIMP_LAYER_MODE_NORMAL);gimp_image_add_layer(s.image,shown,nullptr,0,FALSE);gimp_item_set_offset(GIMP_ITEM(shown),201,101);gimp_item_set_visible(GIMP_ITEM(s.source),FALSE,FALSE);}
 if(mode==2){shown=gimp_filter_layer_new(s.image,256,192,"Gaussian committed result",1,GIMP_LAYER_MODE_NORMAL_LEGACY);f=GIMP_FILTER_LAYER(shown);auto*args=gimp_value_array_new_from_types(nullptr,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_DOUBLE,1.,G_TYPE_DOUBLE,1.,G_TYPE_INT,1,G_TYPE_NONE);g_assert_true(gimp_filter_layer_set_definition(f,"plug-in-gauss",nullptr,args,nullptr));gimp_value_array_unref(args);gimp_image_add_layer(s.image,shown,nullptr,0,FALSE);gimp_item_set_offset(GIMP_ITEM(shown),17,23);filter_settle(f);}
 gimp_set_focused_once(app);auto*d=gimp_create_display(app,s.image,gimp_unit_pixel(),1.,nullptr);g_assert_nonnull(d);spin(100);
 auto*shell=gimp_display_get_shell(d);auto*w=gtk_widget_get_toplevel(GTK_WIDGET(shell));gtk_window_move(GTK_WINDOW(w),0,0);gtk_window_resize(GTK_WINDOW(w),1080,1920);gtk_window_present(GTK_WINDOW(w));gimp_display_shell_scale(shell,GIMP_ZOOM_TO,1.5,GIMP_ZOOM_FOCUS_IMAGE_CENTER);gimp_image_flush(s.image);spin(500);
 // Baseline readiness only. Never invoke these forcing APIs after mutation.
 gimp_pickable_flush(GIMP_PICKABLE(s.image));gimp_display_flush_now(d);spin(150);
 int x=mode==1?284:100,y=mode==1?178:100;auto*before=g_strdup_printf("graph-%d-before.png",mode);auto*after=g_strdup_printf("graph-%d-after.png",mode);capture_graph(d,x,y,before,false);
 std::vector<GeglRectangle>updates;auto*proj=gimp_image_get_projection(s.image);auto id=g_signal_connect(proj,"update",G_CALLBACK(projection_signal),&updates);
 GeglRectangle local={70,65,32,32};fill(s.source,&local,"red");gimp_drawable_update(GIMP_DRAWABLE(s.source),local.x,local.y,local.width,local.height);gimp_image_flush(s.image);if(f)filter_settle(f);spin(500);
 capture_graph(d,x,y,after,true);g_assert_false(updates.empty());bool covers=false;for(auto r:updates)if(x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height)covers=true;g_assert_true(covers);
 g_signal_handler_disconnect(proj,id);g_free(before);g_free(after);gimp_delete_display(app,d);spin(20);
}

/* SPDX-License-Identifier: GPL-3.0-or-later
 * Included by each existing owned native-family fixture. */
static GimpCoords generic_point (double x, double y) {
  GimpCoords c = GIMP_COORDS_DEFAULT_VALUES;c.x=x;c.y=y;c.pressure=1;return c;
}
static int generic_undo (Scene& s) { return gimp_undo_stack_get_depth(gimp_image_get_undo_stack(s.image)); }
static int generic_redo (Scene& s) { return gimp_undo_stack_get_depth(gimp_image_get_redo_stack(s.image)); }
static bool generic_raw (Scene& s, bool undo, GError** error) {
  GimpCoords coords[]={generic_point(25,22),generic_point(34,24),generic_point(44,27)};
  const auto original=std::vector<GimpCoords>(coords,coords+3);
  const bool result=gimp_paint_core_stroke(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,coords,3,undo,error);
  g_assert_cmpmem(coords,sizeof coords,original.data(),sizeof coords);return result;
}
static GimpPath* generic_path (Scene& s) {
  auto* path=gimp_path_new(s.image,"Owned separated subpaths");
  for(int y:{18,42}){auto a=generic_point(23,y),b=generic_point(47,y);auto*stroke=gimp_bezier_stroke_new_moveto(&a);gimp_bezier_stroke_lineto(stroke,&b);gimp_path_stroke_add(path,stroke);g_object_unref(stroke);}
  return path;
}
static void generic_raw_undo () {
  Scene s;GError*error=nullptr;g_assert_true(generic_raw(s,true,&error));g_assert_no_error(error);
  auto painted=s.pixels();g_assert(painted!=s.initial);g_assert_cmpint(generic_undo(s),==,1);
  g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==painted);
  g_assert_false(gimp_image_has_pending_paint(s.image));g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));
}
static void generic_path_boundary () {
  Scene s;gimp_channel_select_rectangle(gimp_image_get_mask(s.image),30,0,50,80,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
  auto initial=s.pixels();auto*path=generic_path(s);GError*error=nullptr;
  for(GList*node=path->strokes->head;node;node=node->next){gboolean closed;auto*points=gimp_stroke_interpolate(GIMP_STROKE(node->data),1.,&closed);
    for(guint i=0;i<points->len;++i){const auto&point=g_array_index(points,GimpCoords,i);g_assert_cmpfloat(point.xscale,==,1.);g_assert_cmpfloat(point.yscale,==,1.);g_assert_cmpfloat(point.angle,==,0.);}
    g_array_unref(points);
  }
  g_assert_true(gimp_paint_core_stroke_path(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,FALSE,path,TRUE,&error));g_assert_no_error(error);g_object_unref(path);
  auto painted=s.pixels();g_assert(painted!=initial);g_assert_cmpint(generic_undo(s),==,1);
  g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==initial);g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==painted);
  GimpBoundSeg boundary[]={{20,10,45,10,1,0},{45,10,45,22,1,0},{45,22,20,22,1,0},{20,22,20,10,1,0},
                          {20,31,45,31,1,0},{45,31,45,44,1,0},{45,44,20,44,1,0},{20,44,20,31,1,0}};
  const auto original=std::vector<GimpBoundSeg>(boundary,boundary+8);
  g_assert_true(gimp_paint_core_stroke_boundary(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,TRUE,boundary,8,2,3,TRUE,&error));g_assert_no_error(error);
  g_assert_cmpmem(boundary,sizeof boundary,original.data(),sizeof boundary);g_assert_cmpint(generic_undo(s),==,2);
  auto bounded=s.pixels();g_assert(bounded!=painted);g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==painted);g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==bounded);
}
static void generic_no_undo_noop () {
  Scene s;GError*error=nullptr;g_assert_true(generic_raw(s,true,&error));g_assert_no_error(error);g_assert_true(gimp_image_undo(s.image));
  g_assert_true(generic_raw(s,false,&error));g_assert_no_error(error);g_assert(s.pixels()!=s.initial);g_assert_cmpint(generic_undo(s),==,0);g_assert_cmpint(generic_redo(s),==,1);
  auto before=s.pixels();GimpCoords outside=generic_point(-50,-50);
  g_assert_true(gimp_paint_core_stroke(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,&outside,1,TRUE,&error));g_assert_no_error(error);g_assert(s.pixels()==before);g_assert_cmpint(generic_undo(s),==,0);g_assert_cmpint(generic_redo(s),==,1);
  auto*empty=gimp_path_new(s.image,"empty");g_assert_false(gimp_paint_core_stroke_path(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,FALSE,empty,TRUE,nullptr));g_object_unref(empty);
}
static void generic_active_rejected () {
  Scene s;s.begin();s.motion();generic_drain(s);auto before=s.pixels();GError*error=nullptr;
  g_assert_false(generic_raw(s,true,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==before);g_assert_true(gimp_image_has_pending_paint(s.image));s.finish(false);g_assert(s.pixels()==s.initial);
}
struct GenericCallback { Scene*scene;int action;bool fired=false;bool later=false; };
static void generic_update (GimpDrawable*,gint,gint y,gint,gint,gpointer data) {
  auto&state=*static_cast<GenericCallback*>(data);if(state.fired||(state.later&&y<28))return;state.fired=true;
  auto&s=*state.scene;g_assert_true(gimp_image_has_pending_paint(s.image));
  if(state.action==0){GError*error=nullptr;g_assert_false(generic_raw(s,true,&error));g_assert_nonnull(error);g_clear_error(&error);}
  if(state.action==1)generic_cancel(s);
  if(state.action==2)gimp_painter_binding_close(G_OBJECT(generic_core(s)),nullptr);
  if(state.action==3)g_object_set(s.options,"brush-size",21.,nullptr);
  if(state.action==4)gimp_data_dirty(GIMP_DATA(gimp_context_get_brush(GIMP_CONTEXT(s.options))));
  if(state.action==5){generic_drop_core(s);g_clear_object(&s.options);}
  g_assert_true(gimp_image_has_pending_paint(s.image));
}
static void generic_reentry_and_abort () {
  for(int action=0;action<6;++action){Scene s;GenericCallback state{&s,action};auto id=g_signal_connect(s.layer,"update",G_CALLBACK(generic_update),&state);GError*error=nullptr;
    const bool result=generic_raw(s,true,&error);g_assert_true(state.fired);g_assert_cmpint(result,==,(action==0||action==5));g_signal_handler_disconnect(s.layer,id);
    if(result){g_assert_no_error(error);g_assert(s.pixels()!=s.initial);g_assert_cmpint(generic_undo(s),==,1);}else{g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==s.initial);g_assert_cmpint(generic_undo(s),==,0);}
    g_assert_false(gimp_image_has_pending_paint(s.image));g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(s.layer)));
  }
}
static void generic_later_failure_history () {
  Scene s;GError*error=nullptr;g_assert_true(generic_raw(s,true,&error));g_assert_true(gimp_image_undo(s.image));
  const auto start=generic_core(s)->start_coords,current=generic_core(s)->cur_coords,last=generic_core(s)->last_coords;const auto paint=generic_core(s)->last_paint;
  auto*path=generic_path(s);GenericCallback state{&s,3,false,true};auto id=g_signal_connect(s.layer,"update",G_CALLBACK(generic_update),&state);
  g_assert_false(gimp_paint_core_stroke_path(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,FALSE,path,TRUE,&error));g_assert_true(state.fired);g_assert_nonnull(error);g_clear_error(&error);g_signal_handler_disconnect(s.layer,id);g_object_unref(path);
  g_assert(s.pixels()==s.initial);g_assert_cmpint(generic_undo(s),==,0);g_assert_cmpint(generic_redo(s),==,1);
  g_assert_cmpmem(&generic_core(s)->start_coords,sizeof start,&start,sizeof start);g_assert_cmpmem(&generic_core(s)->cur_coords,sizeof current,&current,sizeof current);g_assert_cmpmem(&generic_core(s)->last_coords,sizeof last,&last,sizeof last);g_assert_cmpmem(&generic_core(s)->last_paint,sizeof paint,&paint,sizeof paint);
  g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()!=s.initial);
}
static void generic_invalid_axes () {
  Scene s;double GimpCoords::*axes[]={&GimpCoords::x,&GimpCoords::y,&GimpCoords::pressure,&GimpCoords::xtilt,&GimpCoords::ytilt,&GimpCoords::wheel,&GimpCoords::distance,&GimpCoords::rotation,&GimpCoords::slider,&GimpCoords::velocity,&GimpCoords::direction,&GimpCoords::xscale,&GimpCoords::yscale,&GimpCoords::angle};
  for(auto axis:axes)for(int index:{0,1}){GimpCoords c[]={s.coords,s.coords};c[index].*axis=std::numeric_limits<double>::quiet_NaN();GError*error=nullptr;g_assert_false(gimp_paint_core_stroke(generic_core(s),GIMP_DRAWABLE(s.layer),s.options,c,2,TRUE,&error));g_assert_nonnull(error);g_clear_error(&error);g_assert(s.pixels()==s.initial);g_assert_null(generic_core(s)->stroke_buffer);g_assert_false(gimp_image_has_pending_paint(s.image));}
}
static void generic_commit_notify (GimpImage*,GimpDirtyMask,gpointer data) {
  auto&state=*static_cast<GenericCallback*>(data);if(state.fired)return;state.fired=true;auto&s=*state.scene;
  g_object_set(s.options,"brush-size",19.,nullptr);generic_cancel(s);
}
static void generic_after_commit () {
  Scene s;GenericCallback state{&s,0};auto id=g_signal_connect(s.image,"dirty",G_CALLBACK(generic_commit_notify),&state);GError*error=nullptr;
  g_assert_true(generic_raw(s,true,&error));g_assert_no_error(error);g_assert_true(state.fired);g_signal_handler_disconnect(s.image,id);
  g_assert(s.pixels()!=s.initial);g_assert_cmpint(generic_undo(s),==,1);g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);
}

static void generic_start_notify (GObject*object,GParamSpec*,gpointer data) {
  auto&state=*static_cast<GenericCallback*>(data);if(state.fired||!gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(object)))return;
  state.fired=true;g_assert_true(gimp_image_has_pending_paint(state.scene->image));g_object_set(state.scene->options,"brush-size",19.,nullptr);
}
static void generic_first_failure () {
  Scene s;GenericCallback state{&s,0};auto id=g_signal_connect(s.layer,"notify::frozen",G_CALLBACK(generic_start_notify),&state);GError*error=nullptr;
  g_assert_false(generic_raw(s,true,&error));g_assert_nonnull(error);g_clear_error(&error);g_signal_handler_disconnect(s.layer,id);g_assert_true(state.fired);
  g_assert(s.pixels()==s.initial);g_assert_cmpint(generic_undo(s),==,0);g_assert_false(gimp_image_has_pending_paint(s.image));
  g_assert_true(generic_raw(s,true,&error));g_assert_no_error(error);
}
static void (*generic_saved_paint)(GimpPaintCore*,GList*,GimpPaintOptions*,GimpSymmetry*,GimpPaintState,guint32);
static GenericCallback*generic_finish_state;
static std::vector<guchar> generic_external_pixels;
static void generic_finish_paint(GimpPaintCore*core,GList*drawables,GimpPaintOptions*options,GimpSymmetry*sym,GimpPaintState phase,guint32 time) {
  generic_saved_paint(core,drawables,options,sym,phase,time);
  auto&state=*generic_finish_state;if(phase!=GIMP_PAINT_STATE_FINISH||state.fired)return;state.fired=true;auto&s=*state.scene;
  g_assert_true(gimp_image_has_pending_paint(s.image));
  if(state.action==0)g_object_set(options,"brush-size",19.,nullptr);
  if(state.action==1)generic_cancel(s);
  if(state.action==2){auto*blue=gegl_color_new("blue");gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer)),GEGL_RECTANGLE(0,0,4,4),blue);g_object_unref(blue);generic_external_pixels=s.pixels();}
}
static void generic_finish_boundary () {
  for(int action=0;action<3;++action){Scene s;GenericCallback state{&s,action};generic_finish_state=&state;auto*klass=GIMP_PAINT_CORE_GET_CLASS(generic_core(s));generic_saved_paint=klass->paint;klass->paint=generic_finish_paint;
    GError*error=nullptr;const bool result=generic_raw(s,true,&error);klass->paint=generic_saved_paint;generic_finish_state=nullptr;
    g_assert_false(result);g_assert_true(state.fired);g_assert_nonnull(error);g_clear_error(&error);g_assert_cmpint(generic_undo(s),==,0);g_assert_false(gimp_image_has_pending_paint(s.image));
    if(action==2)g_assert(s.pixels()==generic_external_pixels);else g_assert(s.pixels()==s.initial);
  }
}

static void register_generic_tests () {
  g_test_add_func("/owned-generic/first-failure",generic_first_failure);
  g_test_add_func("/owned-generic/finish-boundary",generic_finish_boundary);
  g_test_add_func("/owned-generic/raw-undo",generic_raw_undo);
  g_test_add_func("/owned-generic/path-boundary",generic_path_boundary);
  g_test_add_func("/owned-generic/no-undo-noop",generic_no_undo_noop);
  g_test_add_func("/owned-generic/active-rejected",generic_active_rejected);
  g_test_add_func("/owned-generic/reentry-abort-loss",generic_reentry_and_abort);
  g_test_add_func("/owned-generic/later-failure-history",generic_later_failure_history);
  g_test_add_func("/owned-generic/invalid-axes",generic_invalid_axes);
  g_test_add_func("/owned-generic/after-commit-notification",generic_after_commit);
}

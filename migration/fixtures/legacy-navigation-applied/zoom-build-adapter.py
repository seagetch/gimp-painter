from pathlib import Path
import json
out=Path(__file__).resolve().parent;m=json.loads((out/'zoom-source-manifest.json').read_text());names=[r['name'] for r in m['functions']]
macros={n:'legacy_az_'+n for n in names}
macros.update({'GimpDisplayShell':'LegacyAppliedZoom','GimpZoomModel':'LegacyAZModel','GimpZoomModelPrivate':'LegacyAZModel','GimpImage':'LegacyAZImage','GimpProjection':'LegacyAZImage','TileManager':'LegacyAZImage','GimpImageWindow':'void','GtkWidget':'void','GdkEvent':'LegacyAZEvent','GdkPoint':'LegacyAZPoint','GimpZoomType':'gint','GimpZoomFocus':'LegacyAZFocus','GObject':'LegacyAZModel','GParamSpec':'void','GIMP_ZOOM_FOCUS_POINTER':'LEGACY_AZ_POINTER','GIMP_ZOOM_FOCUS_IMAGE_CENTER':'LEGACY_AZ_IMAGE_CENTER','GIMP_ZOOM_FOCUS_BEST_GUESS':'LEGACY_AZ_BEST_GUESS','GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS':'LEGACY_AZ_RETAIN_CENTERING','GIMP_ZOOM_TO':'0','GIMP_ZOOM_IN':'1','GIMP_ZOOM_OUT':'2','GIMP_ZOOM_IN_MORE':'3','GIMP_ZOOM_OUT_MORE':'4','GIMP_ZOOM_IN_MAX':'5','GIMP_ZOOM_OUT_MAX':'6','PROP_VALUE':'1','PROP_MINIMUM':'2','PROP_MAXIMUM':'3','ZOOM_MIN':'(1.0/256.0)','ZOOM_MAX':'256.0','SCALE_EPSILON':'0.0001','ALMOST_CENTERED_THRESHOLD':'2','OVERPAN_FACTOR':'0.5','GTK_SENSITIVITY_ON':'1','GTK_SENSITIVITY_OFF':'0','M_PI':'G_PI'})
fn={
'GIMP_IS_DISPLAY_SHELL(s)':'((s)!=NULL)','GIMP_IS_IMAGE(i)':'((i)!=NULL)','GIMP_IS_ZOOM_MODEL(m)':'((m)!=NULL)','GIMP_ZOOM_MODEL_GET_PRIVATE(m)':'(m)','SCALE_EQUALS(a,b)':'(fabs((a)-(b))<SCALE_EPSILON)','PROJ_ROUND(coord)':'((gint) RINT (coord))','SCALEX(s,x)':'PROJ_ROUND ((x) * (s)->scale_x)','SCALEY(s,y)':'PROJ_ROUND ((y) * (s)->scale_y)','RINT(v)':'floor((v)+0.5)',
'gimp_display_get_image(d)':'((d)->image)','gimp_image_get_width(i)':'((i)->width)','gimp_image_get_height(i)':'((i)->height)','gimp_image_get_resolution(i,x,y)':'((void)(*(x)=(i)->xres), (void)(*(y)=(i)->yres))','gimp_image_get_projection(i)':'(i)','gimp_projection_get_tiles_at_level(p,l,z)':'((void)(l),(void)(z),(p))','tile_manager_width(t)':'((t)->width)','tile_manager_height(t)':'((t)->height)',
'gimp_display_shell_get_window(s)':'((void*)&(s)->window_token)','gimp_image_window_get_active_shell(w)':'((void)(w),legacy_az_active)','gimp_image_window_shrink_wrap(w,g)':'((void)(w),(void)(g))','GTK_WIDGET(w)':'(w)','GTK_RANGE(w)':'(w)','gtk_range_set_lower_stepper_sensitivity(w,s)':'((void)(w),(void)(s))','gtk_range_set_upper_stepper_sensitivity(w,s)':'((void)(w),(void)(s))',
'gtk_get_current_event()':'(legacy_az_active->pointer_input.event_origin==LEGACY_AZ_EVENT_NONE?NULL:&legacy_az_active->event_storage)','gtk_get_event_widget(e)':'((e)->widget)','gtk_widget_get_pointer(w,x,y)':'((void)(w),(void)(*(x)=legacy_az_active->pointer_input.pointer_x),(void)(*(y)=legacy_az_active->pointer_input.pointer_y))','gtk_widget_get_window(w)':'(w)','gdk_cairo_create(w)':'((void)(w),legacy_az_cairo())',
'gimp_display_shell_scale_handle_zoom_revert(s)':'((void)(s))','gimp_display_shell_pause(s)':'((void)(s))','gimp_display_shell_resume(s)':'((void)(s))','gimp_display_shell_scaled(s)':'((void)(s))','gimp_display_shell_scrolled(s)':'((void)(s))','gimp_display_shell_expose_full(s)':'((void)(s))','gimp_display_shell_update_scrollbars_and_rulers(s)':'legacy_az_record_clamp(s)',
'g_object_freeze_notify(o)':'((void)(o))','g_object_thaw_notify(o)':'((void)(o))','g_object_notify(o,p)':'((void)(o),(void)(p))','G_OBJECT_WARN_INVALID_PROPERTY_ID(o,i,p)':'do {(void)(o);(void)(i);(void)(p);g_assert_not_reached();} while(0)','g_signal_emit(o,signal,detail,old,new)':'((void)(o),(void)(old),(void)(new),gimp_display_shell_scale_changed(legacy_az_active))','g_object_set(o,name,v,end)':'legacy_az_set_value((o),(v))'}
macros.update(fn);prefix='#ifndef LEGACY_APPLIED_ZOOM_ORACLE_INC\n#define LEGACY_APPLIED_ZOOM_ORACLE_INC\n#include "zoom-adapter-prelude.inc"\n'
for k,v in macros.items():
 name=k.split('(')[0];prefix+=f'#pragma push_macro("{name}")\n#undef {name}\n#define {k} {v}\n'
prefix+='\n#include "zoom-prototypes.inc"\nstatic void legacy_az_set_value(LegacyAZModel*m,gdouble x){GValue v=G_VALUE_INIT;g_value_init(&v,G_TYPE_DOUBLE);g_value_set_double(&v,x);gimp_zoom_model_set_property(m,PROP_VALUE,&v,NULL);g_value_unset(&v);}\n#include "zoom-functions.inc"\n'
for k in reversed(macros):prefix+=f'#pragma pop_macro("{k.split("(")[0]}")\n'
prefix+='''
/* Caller supplies old geometry and resolutions explicitly. No native helper or
 * GTK pointer query is used by this source-only test boundary. */
static void legacy_applied_zoom_apply(LegacyAppliedZoom*s,gdouble requested,LegacyAZFocus focus,LegacyAZPointerInput input)
{
 g_assert_null(legacy_az_active);g_assert_nonnull(s);
 g_assert_cmpfloat(s->minimum,>,0);g_assert_cmpfloat(s->maximum,>=,s->minimum);
 GQueue queue=G_QUEUE_INIT;s->zoom_focus_pointer_queue=&queue;s->pointer_input=input;
 s->image_storage=(LegacyAZImage){s->image_width,s->image_height,s->image_xres,s->image_yres};s->config_storage.resize_windows_on_zoom=FALSE;
 s->display_storage=(LegacyAZDisplay){&s->config_storage,&s->image_storage};s->display=&s->display_storage;
 s->model_storage=(LegacyAZModel){s->factor,s->minimum,s->maximum};s->zoom=&s->model_storage;s->canvas=&s->canvas_token;
 s->event_storage.widget=input.event_origin==LEGACY_AZ_EVENT_CANVAS?s->canvas:input.event_origin==LEGACY_AZ_EVENT_WINDOW?(void*)&s->window_token:(void*)&s->menu_token;
 if(input.queued){LegacyAZPoint*p=g_slice_new(LegacyAZPoint);p->x=input.queued_x;p->y=input.queued_y;g_queue_push_tail(&queue,p);}
 legacy_az_active=s;memset(&s->trace,0,sizeof s->trace);s->trace.requested=requested;
 s->trace.changed=fabs(requested-s->factor)>=.0001;
 if(s->trace.changed){
  /* Diagnostic replay uses the same explicit pointer input; restore the queue
   * consumed by the focus helper before the normative unchanged scale call. */
  legacy_az_gimp_display_shell_scale_get_zoom_focus(s,requested,s->factor,&s->trace.focus_x,&s->trace.focus_y,focus);
  legacy_az_gimp_display_shell_scale_get_image_center_viewport(s,&s->trace.image_center_x,&s->trace.image_center_y);
  if(input.queued){g_assert_true(g_queue_is_empty(&queue));LegacyAZPoint*p=g_slice_new(LegacyAZPoint);p->x=input.queued_x;p->y=input.queued_y;g_queue_push_tail(&queue,p);}
  gdouble x,y,sx,sy;legacy_az_gimp_display_shell_untransform_xy_f(s,s->trace.focus_x,s->trace.focus_y,&x,&y);
  legacy_az_gimp_display_shell_calculate_scale_x_and_y(s,requested,&sx,&sy);
  s->trace.target_x=sx*x-s->trace.focus_x;s->trace.target_y=sy*y-s->trace.focus_y;
  gboolean fx,fy,ix,iy;
  legacy_az_gimp_display_shell_scale_image_starts_to_fit(s,requested,s->factor,&s->trace.starts_fit_x,&s->trace.starts_fit_y);
  legacy_az_gimp_display_shell_scale_viewport_coord_almost_centered(s,s->trace.focus_x,s->trace.focus_y,&fx,&fy);
  legacy_az_gimp_display_shell_scale_viewport_coord_almost_centered(s,s->trace.image_center_x,s->trace.image_center_y,&ix,&iy);
  s->trace.center_x=s->trace.starts_fit_x||(fx&&ix);s->trace.center_y=s->trace.starts_fit_y||(fy&&iy);
 }
 legacy_az_gimp_display_shell_scale(s,0,requested,focus);s->trace.applied=s->model_storage.value;
 s->factor=s->model_storage.value;s->queue_consumed=input.queued&&g_queue_is_empty(&queue);
 while(!g_queue_is_empty(&queue)){LegacyAZPoint*p=g_queue_pop_head(&queue);g_slice_free(LegacyAZPoint,p);}
 legacy_az_active=NULL;s->zoom_focus_pointer_queue=NULL;
}
#endif
'''
(out/'zoom-oracle.inc').write_text(prefix)

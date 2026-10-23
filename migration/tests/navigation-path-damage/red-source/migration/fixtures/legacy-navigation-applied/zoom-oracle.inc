#ifndef LEGACY_APPLIED_ZOOM_ORACLE_INC
#define LEGACY_APPLIED_ZOOM_ORACLE_INC
#include "zoom-adapter-prelude.inc"
#pragma push_macro("gimp_display_shell_scale")
#undef gimp_display_shell_scale
#define gimp_display_shell_scale legacy_az_gimp_display_shell_scale
#pragma push_macro("gimp_display_shell_scale_to")
#undef gimp_display_shell_scale_to
#define gimp_display_shell_scale_to legacy_az_gimp_display_shell_scale_to
#pragma push_macro("gimp_display_shell_scale_get_zoom_focus")
#undef gimp_display_shell_scale_get_zoom_focus
#define gimp_display_shell_scale_get_zoom_focus legacy_az_gimp_display_shell_scale_get_zoom_focus
#pragma push_macro("gimp_display_shell_scale_get_image_center_viewport")
#undef gimp_display_shell_scale_get_image_center_viewport
#define gimp_display_shell_scale_get_image_center_viewport legacy_az_gimp_display_shell_scale_get_image_center_viewport
#pragma push_macro("gimp_display_shell_scale_image_is_within_viewport")
#undef gimp_display_shell_scale_image_is_within_viewport
#define gimp_display_shell_scale_image_is_within_viewport legacy_az_gimp_display_shell_scale_image_is_within_viewport
#pragma push_macro("gimp_display_shell_scale_image_starts_to_fit")
#undef gimp_display_shell_scale_image_starts_to_fit
#define gimp_display_shell_scale_image_starts_to_fit legacy_az_gimp_display_shell_scale_image_starts_to_fit
#pragma push_macro("gimp_display_shell_scale_image_stops_to_fit")
#undef gimp_display_shell_scale_image_stops_to_fit
#define gimp_display_shell_scale_image_stops_to_fit legacy_az_gimp_display_shell_scale_image_stops_to_fit
#pragma push_macro("gimp_display_shell_scale_viewport_coord_almost_centered")
#undef gimp_display_shell_scale_viewport_coord_almost_centered
#define gimp_display_shell_scale_viewport_coord_almost_centered legacy_az_gimp_display_shell_scale_viewport_coord_almost_centered
#pragma push_macro("gimp_display_shell_calculate_scale_x_and_y")
#undef gimp_display_shell_calculate_scale_x_and_y
#define gimp_display_shell_calculate_scale_x_and_y legacy_az_gimp_display_shell_calculate_scale_x_and_y
#pragma push_macro("gimp_display_shell_scale_by_values")
#undef gimp_display_shell_scale_by_values
#define gimp_display_shell_scale_by_values legacy_az_gimp_display_shell_scale_by_values
#pragma push_macro("gimp_display_shell_scale_resize")
#undef gimp_display_shell_scale_resize
#define gimp_display_shell_scale_resize legacy_az_gimp_display_shell_scale_resize
#pragma push_macro("gimp_display_shell_get_screen_resolution")
#undef gimp_display_shell_get_screen_resolution
#define gimp_display_shell_get_screen_resolution legacy_az_gimp_display_shell_get_screen_resolution
#pragma push_macro("gimp_display_shell_scroll_center_image")
#undef gimp_display_shell_scroll_center_image
#define gimp_display_shell_scroll_center_image legacy_az_gimp_display_shell_scroll_center_image
#pragma push_macro("gimp_display_shell_scroll_set_offset")
#undef gimp_display_shell_scroll_set_offset
#define gimp_display_shell_scroll_set_offset legacy_az_gimp_display_shell_scroll_set_offset
#pragma push_macro("gimp_display_shell_scroll_clamp_offsets")
#undef gimp_display_shell_scroll_clamp_offsets
#define gimp_display_shell_scroll_clamp_offsets legacy_az_gimp_display_shell_scroll_clamp_offsets
#pragma push_macro("gimp_display_shell_scroll_clamp_and_update")
#undef gimp_display_shell_scroll_clamp_and_update
#define gimp_display_shell_scroll_clamp_and_update legacy_az_gimp_display_shell_scroll_clamp_and_update
#pragma push_macro("gimp_display_shell_draw_get_scaled_image_size")
#undef gimp_display_shell_draw_get_scaled_image_size
#define gimp_display_shell_draw_get_scaled_image_size legacy_az_gimp_display_shell_draw_get_scaled_image_size
#pragma push_macro("gimp_display_shell_draw_get_scaled_image_size_for_scale")
#undef gimp_display_shell_draw_get_scaled_image_size_for_scale
#define gimp_display_shell_draw_get_scaled_image_size_for_scale legacy_az_gimp_display_shell_draw_get_scaled_image_size_for_scale
#pragma push_macro("gimp_display_shell_untransform_xy_f")
#undef gimp_display_shell_untransform_xy_f
#define gimp_display_shell_untransform_xy_f legacy_az_gimp_display_shell_untransform_xy_f
#pragma push_macro("gimp_display_shell_set_cairo_rotate")
#undef gimp_display_shell_set_cairo_rotate
#define gimp_display_shell_set_cairo_rotate legacy_az_gimp_display_shell_set_cairo_rotate
#pragma push_macro("gimp_display_shell_scale_changed")
#undef gimp_display_shell_scale_changed
#define gimp_display_shell_scale_changed legacy_az_gimp_display_shell_scale_changed
#pragma push_macro("gimp_zoom_model_zoom")
#undef gimp_zoom_model_zoom
#define gimp_zoom_model_zoom legacy_az_gimp_zoom_model_zoom
#pragma push_macro("gimp_zoom_model_get_factor")
#undef gimp_zoom_model_get_factor
#define gimp_zoom_model_get_factor legacy_az_gimp_zoom_model_get_factor
#pragma push_macro("gimp_zoom_model_zoom_step")
#undef gimp_zoom_model_zoom_step
#define gimp_zoom_model_zoom_step legacy_az_gimp_zoom_model_zoom_step
#pragma push_macro("gimp_zoom_model_set_property")
#undef gimp_zoom_model_set_property
#define gimp_zoom_model_set_property legacy_az_gimp_zoom_model_set_property
#pragma push_macro("GimpDisplayShell")
#undef GimpDisplayShell
#define GimpDisplayShell LegacyAppliedZoom
#pragma push_macro("GimpZoomModel")
#undef GimpZoomModel
#define GimpZoomModel LegacyAZModel
#pragma push_macro("GimpZoomModelPrivate")
#undef GimpZoomModelPrivate
#define GimpZoomModelPrivate LegacyAZModel
#pragma push_macro("GimpImage")
#undef GimpImage
#define GimpImage LegacyAZImage
#pragma push_macro("GimpProjection")
#undef GimpProjection
#define GimpProjection LegacyAZImage
#pragma push_macro("TileManager")
#undef TileManager
#define TileManager LegacyAZImage
#pragma push_macro("GimpImageWindow")
#undef GimpImageWindow
#define GimpImageWindow void
#pragma push_macro("GtkWidget")
#undef GtkWidget
#define GtkWidget void
#pragma push_macro("GdkEvent")
#undef GdkEvent
#define GdkEvent LegacyAZEvent
#pragma push_macro("GdkPoint")
#undef GdkPoint
#define GdkPoint LegacyAZPoint
#pragma push_macro("GimpZoomType")
#undef GimpZoomType
#define GimpZoomType gint
#pragma push_macro("GimpZoomFocus")
#undef GimpZoomFocus
#define GimpZoomFocus LegacyAZFocus
#pragma push_macro("GObject")
#undef GObject
#define GObject LegacyAZModel
#pragma push_macro("GParamSpec")
#undef GParamSpec
#define GParamSpec void
#pragma push_macro("GIMP_ZOOM_FOCUS_POINTER")
#undef GIMP_ZOOM_FOCUS_POINTER
#define GIMP_ZOOM_FOCUS_POINTER LEGACY_AZ_POINTER
#pragma push_macro("GIMP_ZOOM_FOCUS_IMAGE_CENTER")
#undef GIMP_ZOOM_FOCUS_IMAGE_CENTER
#define GIMP_ZOOM_FOCUS_IMAGE_CENTER LEGACY_AZ_IMAGE_CENTER
#pragma push_macro("GIMP_ZOOM_FOCUS_BEST_GUESS")
#undef GIMP_ZOOM_FOCUS_BEST_GUESS
#define GIMP_ZOOM_FOCUS_BEST_GUESS LEGACY_AZ_BEST_GUESS
#pragma push_macro("GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS")
#undef GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS
#define GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS LEGACY_AZ_RETAIN_CENTERING
#pragma push_macro("GIMP_ZOOM_TO")
#undef GIMP_ZOOM_TO
#define GIMP_ZOOM_TO 0
#pragma push_macro("GIMP_ZOOM_IN")
#undef GIMP_ZOOM_IN
#define GIMP_ZOOM_IN 1
#pragma push_macro("GIMP_ZOOM_OUT")
#undef GIMP_ZOOM_OUT
#define GIMP_ZOOM_OUT 2
#pragma push_macro("GIMP_ZOOM_IN_MORE")
#undef GIMP_ZOOM_IN_MORE
#define GIMP_ZOOM_IN_MORE 3
#pragma push_macro("GIMP_ZOOM_OUT_MORE")
#undef GIMP_ZOOM_OUT_MORE
#define GIMP_ZOOM_OUT_MORE 4
#pragma push_macro("GIMP_ZOOM_IN_MAX")
#undef GIMP_ZOOM_IN_MAX
#define GIMP_ZOOM_IN_MAX 5
#pragma push_macro("GIMP_ZOOM_OUT_MAX")
#undef GIMP_ZOOM_OUT_MAX
#define GIMP_ZOOM_OUT_MAX 6
#pragma push_macro("PROP_VALUE")
#undef PROP_VALUE
#define PROP_VALUE 1
#pragma push_macro("PROP_MINIMUM")
#undef PROP_MINIMUM
#define PROP_MINIMUM 2
#pragma push_macro("PROP_MAXIMUM")
#undef PROP_MAXIMUM
#define PROP_MAXIMUM 3
#pragma push_macro("ZOOM_MIN")
#undef ZOOM_MIN
#define ZOOM_MIN (1.0/256.0)
#pragma push_macro("ZOOM_MAX")
#undef ZOOM_MAX
#define ZOOM_MAX 256.0
#pragma push_macro("SCALE_EPSILON")
#undef SCALE_EPSILON
#define SCALE_EPSILON 0.0001
#pragma push_macro("ALMOST_CENTERED_THRESHOLD")
#undef ALMOST_CENTERED_THRESHOLD
#define ALMOST_CENTERED_THRESHOLD 2
#pragma push_macro("OVERPAN_FACTOR")
#undef OVERPAN_FACTOR
#define OVERPAN_FACTOR 0.5
#pragma push_macro("GTK_SENSITIVITY_ON")
#undef GTK_SENSITIVITY_ON
#define GTK_SENSITIVITY_ON 1
#pragma push_macro("GTK_SENSITIVITY_OFF")
#undef GTK_SENSITIVITY_OFF
#define GTK_SENSITIVITY_OFF 0
#pragma push_macro("M_PI")
#undef M_PI
#define M_PI G_PI
#pragma push_macro("GIMP_IS_DISPLAY_SHELL")
#undef GIMP_IS_DISPLAY_SHELL
#define GIMP_IS_DISPLAY_SHELL(s) ((s)!=NULL)
#pragma push_macro("GIMP_IS_IMAGE")
#undef GIMP_IS_IMAGE
#define GIMP_IS_IMAGE(i) ((i)!=NULL)
#pragma push_macro("GIMP_IS_ZOOM_MODEL")
#undef GIMP_IS_ZOOM_MODEL
#define GIMP_IS_ZOOM_MODEL(m) ((m)!=NULL)
#pragma push_macro("GIMP_ZOOM_MODEL_GET_PRIVATE")
#undef GIMP_ZOOM_MODEL_GET_PRIVATE
#define GIMP_ZOOM_MODEL_GET_PRIVATE(m) (m)
#pragma push_macro("SCALE_EQUALS")
#undef SCALE_EQUALS
#define SCALE_EQUALS(a,b) (fabs((a)-(b))<SCALE_EPSILON)
#pragma push_macro("PROJ_ROUND")
#undef PROJ_ROUND
#define PROJ_ROUND(coord) ((gint) RINT (coord))
#pragma push_macro("SCALEX")
#undef SCALEX
#define SCALEX(s,x) PROJ_ROUND ((x) * (s)->scale_x)
#pragma push_macro("SCALEY")
#undef SCALEY
#define SCALEY(s,y) PROJ_ROUND ((y) * (s)->scale_y)
#pragma push_macro("RINT")
#undef RINT
#define RINT(v) floor((v)+0.5)
#pragma push_macro("gimp_display_get_image")
#undef gimp_display_get_image
#define gimp_display_get_image(d) ((d)->image)
#pragma push_macro("gimp_image_get_width")
#undef gimp_image_get_width
#define gimp_image_get_width(i) ((i)->width)
#pragma push_macro("gimp_image_get_height")
#undef gimp_image_get_height
#define gimp_image_get_height(i) ((i)->height)
#pragma push_macro("gimp_image_get_resolution")
#undef gimp_image_get_resolution
#define gimp_image_get_resolution(i,x,y) ((void)(*(x)=(i)->xres), (void)(*(y)=(i)->yres))
#pragma push_macro("gimp_image_get_projection")
#undef gimp_image_get_projection
#define gimp_image_get_projection(i) (i)
#pragma push_macro("gimp_projection_get_tiles_at_level")
#undef gimp_projection_get_tiles_at_level
#define gimp_projection_get_tiles_at_level(p,l,z) ((void)(l),(void)(z),(p))
#pragma push_macro("tile_manager_width")
#undef tile_manager_width
#define tile_manager_width(t) ((t)->width)
#pragma push_macro("tile_manager_height")
#undef tile_manager_height
#define tile_manager_height(t) ((t)->height)
#pragma push_macro("gimp_display_shell_get_window")
#undef gimp_display_shell_get_window
#define gimp_display_shell_get_window(s) ((void*)&(s)->window_token)
#pragma push_macro("gimp_image_window_get_active_shell")
#undef gimp_image_window_get_active_shell
#define gimp_image_window_get_active_shell(w) ((void)(w),legacy_az_active)
#pragma push_macro("gimp_image_window_shrink_wrap")
#undef gimp_image_window_shrink_wrap
#define gimp_image_window_shrink_wrap(w,g) ((void)(w),(void)(g))
#pragma push_macro("GTK_WIDGET")
#undef GTK_WIDGET
#define GTK_WIDGET(w) (w)
#pragma push_macro("GTK_RANGE")
#undef GTK_RANGE
#define GTK_RANGE(w) (w)
#pragma push_macro("gtk_range_set_lower_stepper_sensitivity")
#undef gtk_range_set_lower_stepper_sensitivity
#define gtk_range_set_lower_stepper_sensitivity(w,s) ((void)(w),(void)(s))
#pragma push_macro("gtk_range_set_upper_stepper_sensitivity")
#undef gtk_range_set_upper_stepper_sensitivity
#define gtk_range_set_upper_stepper_sensitivity(w,s) ((void)(w),(void)(s))
#pragma push_macro("gtk_get_current_event")
#undef gtk_get_current_event
#define gtk_get_current_event() (legacy_az_active->pointer_input.event_origin==LEGACY_AZ_EVENT_NONE?NULL:&legacy_az_active->event_storage)
#pragma push_macro("gtk_get_event_widget")
#undef gtk_get_event_widget
#define gtk_get_event_widget(e) ((e)->widget)
#pragma push_macro("gtk_widget_get_pointer")
#undef gtk_widget_get_pointer
#define gtk_widget_get_pointer(w,x,y) ((void)(w),(void)(*(x)=legacy_az_active->pointer_input.pointer_x),(void)(*(y)=legacy_az_active->pointer_input.pointer_y))
#pragma push_macro("gtk_widget_get_window")
#undef gtk_widget_get_window
#define gtk_widget_get_window(w) (w)
#pragma push_macro("gdk_cairo_create")
#undef gdk_cairo_create
#define gdk_cairo_create(w) ((void)(w),legacy_az_cairo())
#pragma push_macro("gimp_display_shell_scale_handle_zoom_revert")
#undef gimp_display_shell_scale_handle_zoom_revert
#define gimp_display_shell_scale_handle_zoom_revert(s) ((void)(s))
#pragma push_macro("gimp_display_shell_pause")
#undef gimp_display_shell_pause
#define gimp_display_shell_pause(s) ((void)(s))
#pragma push_macro("gimp_display_shell_resume")
#undef gimp_display_shell_resume
#define gimp_display_shell_resume(s) ((void)(s))
#pragma push_macro("gimp_display_shell_scaled")
#undef gimp_display_shell_scaled
#define gimp_display_shell_scaled(s) ((void)(s))
#pragma push_macro("gimp_display_shell_scrolled")
#undef gimp_display_shell_scrolled
#define gimp_display_shell_scrolled(s) ((void)(s))
#pragma push_macro("gimp_display_shell_expose_full")
#undef gimp_display_shell_expose_full
#define gimp_display_shell_expose_full(s) ((void)(s))
#pragma push_macro("gimp_display_shell_update_scrollbars_and_rulers")
#undef gimp_display_shell_update_scrollbars_and_rulers
#define gimp_display_shell_update_scrollbars_and_rulers(s) legacy_az_record_clamp(s)
#pragma push_macro("g_object_freeze_notify")
#undef g_object_freeze_notify
#define g_object_freeze_notify(o) ((void)(o))
#pragma push_macro("g_object_thaw_notify")
#undef g_object_thaw_notify
#define g_object_thaw_notify(o) ((void)(o))
#pragma push_macro("g_object_notify")
#undef g_object_notify
#define g_object_notify(o,p) ((void)(o),(void)(p))
#pragma push_macro("G_OBJECT_WARN_INVALID_PROPERTY_ID")
#undef G_OBJECT_WARN_INVALID_PROPERTY_ID
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(o,i,p) do {(void)(o);(void)(i);(void)(p);g_assert_not_reached();} while(0)
#pragma push_macro("g_signal_emit")
#undef g_signal_emit
#define g_signal_emit(o,signal,detail,old,new) ((void)(o),(void)(old),(void)(new),gimp_display_shell_scale_changed(legacy_az_active))
#pragma push_macro("g_object_set")
#undef g_object_set
#define g_object_set(o,name,v,end) legacy_az_set_value((o),(v))

#include "zoom-prototypes.inc"
static void legacy_az_set_value(LegacyAZModel*m,gdouble x){GValue v=G_VALUE_INIT;g_value_init(&v,G_TYPE_DOUBLE);g_value_set_double(&v,x);gimp_zoom_model_set_property(m,PROP_VALUE,&v,NULL);g_value_unset(&v);}
#include "zoom-functions.inc"
#pragma pop_macro("g_object_set")
#pragma pop_macro("g_signal_emit")
#pragma pop_macro("G_OBJECT_WARN_INVALID_PROPERTY_ID")
#pragma pop_macro("g_object_notify")
#pragma pop_macro("g_object_thaw_notify")
#pragma pop_macro("g_object_freeze_notify")
#pragma pop_macro("gimp_display_shell_update_scrollbars_and_rulers")
#pragma pop_macro("gimp_display_shell_expose_full")
#pragma pop_macro("gimp_display_shell_scrolled")
#pragma pop_macro("gimp_display_shell_scaled")
#pragma pop_macro("gimp_display_shell_resume")
#pragma pop_macro("gimp_display_shell_pause")
#pragma pop_macro("gimp_display_shell_scale_handle_zoom_revert")
#pragma pop_macro("gdk_cairo_create")
#pragma pop_macro("gtk_widget_get_window")
#pragma pop_macro("gtk_widget_get_pointer")
#pragma pop_macro("gtk_get_event_widget")
#pragma pop_macro("gtk_get_current_event")
#pragma pop_macro("gtk_range_set_upper_stepper_sensitivity")
#pragma pop_macro("gtk_range_set_lower_stepper_sensitivity")
#pragma pop_macro("GTK_RANGE")
#pragma pop_macro("GTK_WIDGET")
#pragma pop_macro("gimp_image_window_shrink_wrap")
#pragma pop_macro("gimp_image_window_get_active_shell")
#pragma pop_macro("gimp_display_shell_get_window")
#pragma pop_macro("tile_manager_height")
#pragma pop_macro("tile_manager_width")
#pragma pop_macro("gimp_projection_get_tiles_at_level")
#pragma pop_macro("gimp_image_get_projection")
#pragma pop_macro("gimp_image_get_resolution")
#pragma pop_macro("gimp_image_get_height")
#pragma pop_macro("gimp_image_get_width")
#pragma pop_macro("gimp_display_get_image")
#pragma pop_macro("RINT")
#pragma pop_macro("SCALEY")
#pragma pop_macro("SCALEX")
#pragma pop_macro("PROJ_ROUND")
#pragma pop_macro("SCALE_EQUALS")
#pragma pop_macro("GIMP_ZOOM_MODEL_GET_PRIVATE")
#pragma pop_macro("GIMP_IS_ZOOM_MODEL")
#pragma pop_macro("GIMP_IS_IMAGE")
#pragma pop_macro("GIMP_IS_DISPLAY_SHELL")
#pragma pop_macro("M_PI")
#pragma pop_macro("GTK_SENSITIVITY_OFF")
#pragma pop_macro("GTK_SENSITIVITY_ON")
#pragma pop_macro("OVERPAN_FACTOR")
#pragma pop_macro("ALMOST_CENTERED_THRESHOLD")
#pragma pop_macro("SCALE_EPSILON")
#pragma pop_macro("ZOOM_MAX")
#pragma pop_macro("ZOOM_MIN")
#pragma pop_macro("PROP_MAXIMUM")
#pragma pop_macro("PROP_MINIMUM")
#pragma pop_macro("PROP_VALUE")
#pragma pop_macro("GIMP_ZOOM_OUT_MAX")
#pragma pop_macro("GIMP_ZOOM_IN_MAX")
#pragma pop_macro("GIMP_ZOOM_OUT_MORE")
#pragma pop_macro("GIMP_ZOOM_IN_MORE")
#pragma pop_macro("GIMP_ZOOM_OUT")
#pragma pop_macro("GIMP_ZOOM_IN")
#pragma pop_macro("GIMP_ZOOM_TO")
#pragma pop_macro("GIMP_ZOOM_FOCUS_RETAIN_CENTERING_ELSE_BEST_GUESS")
#pragma pop_macro("GIMP_ZOOM_FOCUS_BEST_GUESS")
#pragma pop_macro("GIMP_ZOOM_FOCUS_IMAGE_CENTER")
#pragma pop_macro("GIMP_ZOOM_FOCUS_POINTER")
#pragma pop_macro("GParamSpec")
#pragma pop_macro("GObject")
#pragma pop_macro("GimpZoomFocus")
#pragma pop_macro("GimpZoomType")
#pragma pop_macro("GdkPoint")
#pragma pop_macro("GdkEvent")
#pragma pop_macro("GtkWidget")
#pragma pop_macro("GimpImageWindow")
#pragma pop_macro("TileManager")
#pragma pop_macro("GimpProjection")
#pragma pop_macro("GimpImage")
#pragma pop_macro("GimpZoomModelPrivate")
#pragma pop_macro("GimpZoomModel")
#pragma pop_macro("GimpDisplayShell")
#pragma pop_macro("gimp_zoom_model_set_property")
#pragma pop_macro("gimp_zoom_model_zoom_step")
#pragma pop_macro("gimp_zoom_model_get_factor")
#pragma pop_macro("gimp_zoom_model_zoom")
#pragma pop_macro("gimp_display_shell_scale_changed")
#pragma pop_macro("gimp_display_shell_set_cairo_rotate")
#pragma pop_macro("gimp_display_shell_untransform_xy_f")
#pragma pop_macro("gimp_display_shell_draw_get_scaled_image_size_for_scale")
#pragma pop_macro("gimp_display_shell_draw_get_scaled_image_size")
#pragma pop_macro("gimp_display_shell_scroll_clamp_and_update")
#pragma pop_macro("gimp_display_shell_scroll_clamp_offsets")
#pragma pop_macro("gimp_display_shell_scroll_set_offset")
#pragma pop_macro("gimp_display_shell_scroll_center_image")
#pragma pop_macro("gimp_display_shell_get_screen_resolution")
#pragma pop_macro("gimp_display_shell_scale_resize")
#pragma pop_macro("gimp_display_shell_scale_by_values")
#pragma pop_macro("gimp_display_shell_calculate_scale_x_and_y")
#pragma pop_macro("gimp_display_shell_scale_viewport_coord_almost_centered")
#pragma pop_macro("gimp_display_shell_scale_image_stops_to_fit")
#pragma pop_macro("gimp_display_shell_scale_image_starts_to_fit")
#pragma pop_macro("gimp_display_shell_scale_image_is_within_viewport")
#pragma pop_macro("gimp_display_shell_scale_get_image_center_viewport")
#pragma pop_macro("gimp_display_shell_scale_get_zoom_focus")
#pragma pop_macro("gimp_display_shell_scale_to")
#pragma pop_macro("gimp_display_shell_scale")

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

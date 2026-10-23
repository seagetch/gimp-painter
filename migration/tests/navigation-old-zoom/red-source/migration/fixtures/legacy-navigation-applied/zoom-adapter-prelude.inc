/* Explicit source-only boundary model; arithmetic is in unchanged functions. */
#include <glib-object.h>
#include <cairo.h>
#include <math.h>
#include <string.h>
typedef struct {gint x,y;} LegacyAZPoint;
typedef struct {gpointer widget;} LegacyAZEvent;
typedef struct {gdouble value,minimum,maximum;} LegacyAZModel;
typedef struct {gint width,height;gdouble xres,yres;} LegacyAZImage;
typedef struct {gboolean resize_windows_on_zoom;} LegacyAZConfig;
typedef struct {LegacyAZConfig*config;LegacyAZImage*image;} LegacyAZDisplay;
typedef enum {LEGACY_AZ_POINTER,LEGACY_AZ_IMAGE_CENTER,LEGACY_AZ_BEST_GUESS,LEGACY_AZ_RETAIN_CENTERING} LegacyAZFocus;
typedef enum {LEGACY_AZ_EVENT_NONE,LEGACY_AZ_EVENT_CANVAS,LEGACY_AZ_EVENT_WINDOW,LEGACY_AZ_EVENT_MENU} LegacyAZEventOrigin;
typedef struct {gint pointer_x,pointer_y;LegacyAZEventOrigin event_origin;gboolean queued;gint queued_x,queued_y;} LegacyAZPointerInput;
typedef struct {
 gint disp_width,disp_height,image_width,image_height;
 gdouble offset_x,offset_y,scale_x,scale_y,rotate_angle;
 gboolean mirrored,dot_for_dot;
 gdouble factor,minimum,maximum,image_xres,image_yres,monitor_xres,monitor_yres;
 gboolean queue_consumed;
 /* Adapter-only shell objects, never native application instances. */
 gpointer canvas,hsb,vsb;LegacyAZDisplay*display;LegacyAZModel*zoom;
 GQueue*zoom_focus_pointer_queue;
 gint x_dest_inc,y_dest_inc,x_src_dec,y_src_dec;
 LegacyAZModel model_storage;LegacyAZImage image_storage;LegacyAZConfig config_storage;LegacyAZDisplay display_storage;
 LegacyAZPointerInput pointer_input;LegacyAZEvent event_storage;
 char canvas_token,window_token,menu_token;
} LegacyAppliedZoom;
static LegacyAppliedZoom *legacy_az_active;
static cairo_t *legacy_az_cairo(void){cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*c=cairo_create(s);cairo_surface_destroy(s);return c;}

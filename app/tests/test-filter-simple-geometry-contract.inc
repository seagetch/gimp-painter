/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Pinned legacy translate/scale/resize always delegate then reset, including
 * zero displacement and same-size calls. No-op scheduling is not pixel damage. */
static void
filter_simple_geometry_buffer_notify (GObject *object,GParamSpec *spec,gpointer data)
{ (void)object;(void)spec;(*(guint *)data)++; }
static void
filter_gaussian_simple_geometry (guint operation,gboolean changed)
{
  GimpImage *image=image_new(32,32);
  GimpLayer *source=source_new(image,NULL,32,32);
  GimpFilterLayer *filter;
  GimpItem *item;
  guchar pixels[32*32*4],source_after[32*32*4],last[4];
  guint notifications=0;
  guint64 generation,runs;
  gint offset=changed?4:0,size=changed?24:16;
  for(guint y=0;y<32;y++)for(guint x=0;x<32;x++)
    {guint i=(y*32+x)*4;pixels[i]=x<16?255:0;pixels[i+1]=0;pixels[i+2]=x<16?0:255;pixels[i+3]=255;}
  gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(0,0,32,32),
                  0,babl_format("R'G'B'A u8"),pixels,GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update(GIMP_DRAWABLE(source),0,0,32,32);
  filter=contract_gaussian_filter(image);item=GIMP_ITEM(filter);settle(filter);
  pixel(GIMP_LAYER(filter),255,0,0,255);
  generation=gimp_filter_layer_get_generation(filter);runs=gimp_filter_layer_get_run_count(filter);
  g_signal_connect(filter,"notify::buffer",G_CALLBACK(filter_simple_geometry_buffer_notify),&notifications);
  switch(operation)
    {
    case 0:gimp_item_translate(item,offset,offset,FALSE);break;
    case 1:gimp_item_scale(item,size,size,offset,offset,GIMP_INTERPOLATION_NONE,NULL);break;
    case 2:gimp_item_resize(item,gimp_get_user_context(gimp),GIMP_FILL_TRANSPARENT,
                            size,size,-offset,-offset);break;
    default:g_assert_not_reached();
    }
  g_assert_cmpint(gimp_item_get_offset_x(item),==,offset);
  g_assert_cmpint(gimp_item_get_offset_y(item),==,offset);
  g_assert_cmpint(gimp_item_get_width(item),==,operation==0?16:size);
  g_assert_cmpint(gimp_item_get_height(item),==,operation==0?16:size);
  g_assert_cmpint(gimp_item_get_offset_x(GIMP_ITEM(source)),==,0);
  g_assert_cmpint(gimp_item_get_width(GIMP_ITEM(source)),==,32);
  g_assert_cmpint(gimp_item_get_offset_y(GIMP_ITEM(source)),==,0);
  g_assert_cmpint(gimp_item_get_height(GIMP_ITEM(source)),==,32);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(source)),GEGL_RECTANGLE(0,0,32,32),
                  1.0,babl_format("R'G'B'A u8"),source_after,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpmem(source_after,sizeof(source_after),pixels,sizeof(pixels));
  g_test_message("operation=%u changed=%d buffer-notify=%u generation=%"G_GUINT64_FORMAT"->%"G_GUINT64_FORMAT" bounds=%d,%d/%dx%d;unchanged lower source;no-op failure is scheduling-only",
                 operation,changed,notifications,generation,gimp_filter_layer_get_generation(filter),
                 gimp_item_get_offset_x(item),gimp_item_get_offset_y(item),
                 gimp_item_get_width(item),gimp_item_get_height(item));
  if(operation==1 || (operation==2 && changed))g_assert_cmpuint(notifications,>,0);
  if(operation==2 && !changed)g_assert_cmpuint(notifications,==,0);
  g_assert_cmpuint(gimp_filter_layer_get_generation(filter),>,generation);
  settle(filter);g_assert_cmpuint(gimp_filter_layer_get_run_count(filter),>,runs);
  pixel(GIMP_LAYER(filter),255,0,0,255);
  gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(filter)),
                  GEGL_RECTANGLE(gimp_item_get_width(item)-1,0,1,1),1.0,
                  babl_format("R'G'B'A u8"),last,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
  g_assert_cmpint(last[0],==,changed?0:255);g_assert_cmpint(last[1],==,0);
  g_assert_cmpint(last[2],==,changed?255:0);g_assert_cmpint(last[3],==,255);
  g_object_unref(image);
}
static void filter_gaussian_translate_noop (void) {filter_gaussian_simple_geometry(0,FALSE);}
static void filter_gaussian_translate_changed (void) {filter_gaussian_simple_geometry(0,TRUE);}
static void filter_gaussian_scale_same (void) {filter_gaussian_simple_geometry(1,FALSE);}
static void filter_gaussian_scale_changed (void) {filter_gaussian_simple_geometry(1,TRUE);}
static void filter_gaussian_resize_noop (void) {filter_gaussian_simple_geometry(2,FALSE);}
static void filter_gaussian_resize_changed (void) {filter_gaussian_simple_geometry(2,TRUE);}

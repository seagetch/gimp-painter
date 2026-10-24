#include <glib-object.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
/* Successful bounded host I/O and unused-kind adapters, not an old application. */
typedef struct {FILE *fp;guint32 cp;} XcfInfo;
typedef void GimpImage;
typedef enum {XCF_FILTER_END=0,XCF_FILTER_UNSUPPORTED=1,XCF_FILTER_INT32,XCF_FILTER_INT16,XCF_FILTER_INT8,XCF_FILTER_FLOAT,XCF_FILTER_INT8ARRAY,XCF_FILTER_FLOATARRAY,XCF_FILTER_STRING,XCF_FILTER_RGB,XCF_FILTER_DRAWABLE} XcfFilterArgType;
#define GIMP_VALUE_HOLDS_INT32(v) G_TYPE_CHECK_VALUE_TYPE((v),gimp_int32_get_type())
#define GIMP_VALUE_HOLDS_INT8(v) G_TYPE_CHECK_VALUE_TYPE((v),gimp_int8_get_type())
#define GIMP_VALUE_HOLDS_INT16(v) FALSE
#define GIMP_VALUE_HOLDS_INT8_ARRAY(v) FALSE
#define GIMP_VALUE_HOLDS_FLOAT_ARRAY(v) FALSE
#define GIMP_VALUE_HOLDS_DRAWABLE_ID(v) G_TYPE_CHECK_VALUE_TYPE((v),gimp_drawable_id_get_type())
#define g_print(...) ((void)0)
static guint xcf_write_int8(FILE *fp,const guint8 *d,gint n,GError **e){(void)e;assert(fwrite(d,1,n,fp)==(size_t)n);return n;}
static guint xcf_write_float(FILE *fp,const gfloat *d,gint n,GError **e){(void)fp;(void)d;(void)n;(void)e;g_assert_not_reached();return 0;}

static GType
gimp_int8_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      const GTypeInfo info = { 0, };

      type = g_type_register_static (G_TYPE_UINT, "GimpInt8", &info, 0);
    }

  return type;
}
static GType
gimp_int32_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      const GTypeInfo info = { 0, };

      type = g_type_register_static (G_TYPE_INT, "GimpInt32", &info, 0);
    }

  return type;
}
static GType
gimp_image_id_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      const GTypeInfo info = { 0, };

      type = g_type_register_static (G_TYPE_INT, "GimpImageID", &info, 0);
    }

  return type;
}
static GType
gimp_drawable_id_get_type (void)
{
  static GType type = 0;

  if (! type)
    {
      const GTypeInfo info = { 0, };

      type = g_type_register_static (G_TYPE_INT, "GimpDrawableID", &info, 0);
    }

  return type;
}
static guint
xcf_write_int32 (FILE           *fp,
                 const guint32  *data,
                 gint            count,
                 GError        **error)
{
  GError  *tmp_error = NULL;
  gint     i;

  if (count > 0)
    {
      for (i = 0; i < count; i++)
        {
          guint32  tmp = g_htonl (data[i]);

          xcf_write_int8 (fp, (const guint8 *) &tmp, 4, &tmp_error);

          if (tmp_error)
            {
              g_propagate_error (error, tmp_error);

              return i * 4;
            }
        }
    }

  return count * 4;
}
static guint
xcf_write_string (FILE     *fp,
                  gchar   **data,
                  gint      count,
                  GError  **error)
{
  GError  *tmp_error = NULL;
  guint    total     = 0;
  gint     i;

  for (i = 0; i < count; i++)
    {
      guint32 tmp;

      if (data[i])
        tmp = strlen (data[i]) + 1;
      else
        tmp = 0;

      xcf_write_int32 (fp, &tmp, 1, &tmp_error);

      if (tmp_error)
        {
          g_propagate_error (error, tmp_error);
          return total;
        }

      if (tmp > 0)
        xcf_write_int8 (fp, (const guint8 *) data[i], tmp, &tmp_error);

      if (tmp_error)
        {
          g_propagate_error (error, tmp_error);
          return total;
        }

      total += 4 + tmp;
    }

  return total;
}
#define xcf_write_int32_check_error(info, data, count) G_STMT_START { \
  info->cp += xcf_write_int32 (info->fp, data, count, &tmp_error); \
  if (tmp_error)                                                   \
    {                                                              \
      g_propagate_error (error, tmp_error);                        \
      return FALSE;                                                \
    }                                                              \
  } G_STMT_END

#define xcf_write_zero_int32_check_error(info, count) G_STMT_START {  \
  info->cp += xcf_write_zero_int32 (info->fp, count, &tmp_error); \
  if (tmp_error)                                                  \
    {                                                             \
      g_propagate_error (error, tmp_error);                       \
      return FALSE;                                               \
    }                                                             \
  } G_STMT_END

#define xcf_write_int8_check_error(info, data, count) G_STMT_START { \
  info->cp += xcf_write_int8 (info->fp, data, count, &tmp_error); \
  if (tmp_error)                                                  \
    {                                                             \
      g_propagate_error (error, tmp_error);                       \
      return FALSE;                                               \
    }                                                             \
  } G_STMT_END

#define xcf_write_float_check_error(info, data, count) G_STMT_START { \
  info->cp += xcf_write_float (info->fp, data, count, &tmp_error); \
  if (tmp_error)                                                   \
    {                                                              \
      g_propagate_error (error, tmp_error);                        \
      return FALSE;                                                \
    }                                                              \
  } G_STMT_END

#define xcf_write_string_check_error(info, data, count) G_STMT_START { \
  info->cp += xcf_write_string (info->fp, data, count, &tmp_error); \
  if (tmp_error)                                                    \
    {                                                               \
      g_propagate_error (error, tmp_error);                         \
      return FALSE;                                                 \
    }                                                               \
  } G_STMT_END


static gboolean
xcf_save_filter_spec_value   (XcfInfo           *info,
                              GimpImage         *image,
                              GError           **error,
                              GValue            *value)
{
  XcfFilterArgType value_type = (XcfFilterArgType)0;
  guint32 size = 0;
  GError  *tmp_error = NULL;

  if ( G_VALUE_HOLDS_DOUBLE(value) ) {
    gfloat write_value = (gfloat)g_value_get_double(value);
    value_type = XCF_FILTER_FLOAT;
    size = sizeof(write_value);
    g_print("  [%x] type=%u, value=%f\n", info->cp, value_type, write_value);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);
    xcf_write_float_check_error (info, &write_value, 1);

  } else if ( GIMP_VALUE_HOLDS_INT32(value) ) {
    guint32 write_value = (guint32)g_value_get_int(value);
    value_type = XCF_FILTER_INT32;
    size = 4;
    g_print("  [%x] type=%u, value=%u\n", info->cp, value_type, write_value);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);
    xcf_write_int32_check_error (info, &write_value, 1);

  } else if ( GIMP_VALUE_HOLDS_INT8(value) ) {
    guint8 write_value = (guint8)g_value_get_int(value);
    value_type = XCF_FILTER_INT8;
    size = sizeof(gint8);
    g_print("  [%x] type=%u, value=%u\n", info->cp, value_type, write_value);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);
    xcf_write_int8_check_error (info, &write_value, 1);

  } else if ( GIMP_VALUE_HOLDS_INT16(value) ) {
    guint32 write_value = (guint32)g_value_get_int(value);
    value_type = XCF_FILTER_INT16;
    size = 2;
    g_print("  [%x] type=%u, value=%u\n", info->cp, value_type, write_value);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);
    xcf_write_int32_check_error (info, &write_value, 1);

  } else if ( GIMP_VALUE_HOLDS_INT8_ARRAY(value) ) {
    value_type = XCF_FILTER_INT8ARRAY;
    // FIXME: int8 array is not supported for now.
    g_print("  [%x] type=%u, N/A\n", info->cp, value_type);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);

  } else if ( GIMP_VALUE_HOLDS_FLOAT_ARRAY(value) ) {
    value_type = XCF_FILTER_FLOATARRAY;
    // FIXME: float array is not supported for now.
    g_print("  [%x] type=%u, N/A\n", info->cp, value_type);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);

  } else if ( G_VALUE_HOLDS_STRING(value) ) {
    gchar* write_value = (gchar*)g_value_get_string(value);
    value_type = XCF_FILTER_FLOATARRAY;
    g_print("  [%x] type=%u, value=%s\n", info->cp, value_type, write_value);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_string_check_error (info, &write_value, 1);

  } else if ( GIMP_VALUE_HOLDS_DRAWABLE_ID(value) ) {
    value_type = XCF_FILTER_DRAWABLE;
    // FIXME: drawable is not supported for now.
    g_print("  [%x] type=%u, N/A\n", info->cp, value_type);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);

  } else {
    value_type = XCF_FILTER_UNSUPPORTED;
    // skip this value.
    g_print("  [%x] UNKNOWN\n", info->cp);
    xcf_write_int32_check_error (info, &value_type, 1);
    xcf_write_int32_check_error (info, &size, 1);

  }
  if (tmp_error) {
    g_propagate_error (error, tmp_error);
    return FALSE;
  }
  return TRUE;
}

int main(void) {
 const gint runs[]={G_MININT32,-123,-1,0,1,2,123,G_MAXINT32};
 for(guint k=0;k<G_N_ELEMENTS(runs);k++) {
  FILE *f=tmpfile();assert(f);XcfInfo info={f,0};GError *error=NULL;gchar *name="plug-in-normalize";
  info.cp+=xcf_write_string(f,&name,1,&error);assert(!error);
  GValue args[3]={G_VALUE_INIT};g_value_init(args,gimp_int32_get_type());g_value_set_int(args,runs[k]);
  g_value_init(args+1,gimp_image_id_get_type());g_value_set_int(args+1,1234+(gint)k);
  g_value_init(args+2,gimp_drawable_id_get_type());g_value_set_int(args+2,5678+(gint)k);
  for(guint i=0;i<3;i++){assert(xcf_save_filter_spec_value(&info,NULL,&error,args+i));g_value_unset(args+i);}
  guint32 end[2]={0,0};info.cp+=xcf_write_int32(f,end,2,&error);assert(!error&&info.cp==58);
  rewind(f);guint8 bytes[58];assert(fread(bytes,1,58,f)==58);fclose(f);
  printf("WIRE %d ",runs[k]);for(guint i=0;i<58;i++)printf("%02x",bytes[i]);puts("");
 }
 return 0;
}

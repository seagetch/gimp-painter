#include "config.h"
#include <cairo.h>
#include <gegl.h>
#include <stdio.h>
#include "app/gegl/gimptilehandlervalidate.h"
static unsigned pixel(GeglBuffer*b,const char*label){unsigned char p[4]={0};gegl_buffer_get(b,GEGL_RECTANGLE(0,0,1,1),1,babl_format("R'G'B'A u8"),p,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);printf("%s %u %u %u %u\n",label,p[0],p[1],p[2],p[3]);return p[0];}
int main(int argc,char**argv){gegl_init(&argc,&argv);GeglNode*graph=gegl_node_new();GeglColor*blue=gegl_color_new("blue");GeglNode*color=gegl_node_new_child(graph,"operation","gegl:color","value",blue,NULL);GeglBuffer*b=gegl_buffer_new(GEGL_RECTANGLE(0,0,1,1),babl_format("R'G'B'A u8"));GimpTileHandlerValidate*v=GIMP_TILE_HANDLER_VALIDATE(gimp_tile_handler_validate_new(color));gimp_tile_handler_validate_assign(v,b);gimp_tile_handler_validate_invalidate(v,GEGL_RECTANGLE(0,0,1,1));g_assert_cmpuint(pixel(b,"initial"),==,0);GeglColor*red=gegl_color_new("red");gegl_node_set(color,"value",red,NULL);gimp_tile_handler_validate_invalidate(v,GEGL_RECTANGLE(0,0,1,1));pixel(b,"invalidated");gegl_buffer_flush(b);g_assert_cmpuint(pixel(b,"flushed"),==,255);gimp_tile_handler_validate_unassign(v,b);g_object_unref(v);g_object_unref(b);g_object_unref(blue);g_object_unref(red);g_object_unref(graph);gegl_exit();return 0;}

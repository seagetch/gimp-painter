/* SPDX-License-Identifier: GPL-3.0-or-later
 * The same native entrypoint scene recipe is compiled against both pinned old
 * GIMP and the port. Old raw input is drawable-local; Paths/Boundaries are image-space. */
#include "paint/gimppaintcore-stroke.h"
#include "vectors/gimpstroke.h"
#ifdef OWNED_GENERIC_OLD
#include "base/boundary.h"
#include "vectors/gimpvectors.h"
#include "vectors/gimpbezierstroke.h"
#define OwnedPath GimpVectors
#define owned_path_new gimp_vectors_new
#define owned_path_add gimp_vectors_stroke_add
#define owned_stroke_path gimp_paint_core_stroke_vectors
#define OwnedBoundSeg BoundSeg
#else
#include "core/gimpboundary.h"
#include "vectors/gimppath.h"
#include "vectors/gimpbezierstroke.h"
#define OwnedPath GimpPath
#define owned_path_new gimp_path_new
#define owned_path_add gimp_path_stroke_add
#define owned_stroke_path gimp_paint_core_stroke_path
#define OwnedBoundSeg GimpBoundSeg
#endif
static gboolean owned_trace_stroke (const char* route,GimpPaintCore*core,GimpDrawable*d,GimpPaintOptions*options,
                                    GimpCoords start,gboolean smudge,gboolean dynamic,GError**error) {
  if(g_str_equal(route,"raw")) {
    GimpCoords points[8];points[0]=start;int count=smudge?8:6;
    for(int n=1;n<count;++n){points[n]=points[n-1];points[n].x+=smudge?2.5:2.;points[n].y+=smudge?(n%2?1:-1):.5;
      if(smudge)points[n].pressure=dynamic?(n%3==0?.25:n%3==1?1:.6):.7;}
    return gimp_paint_core_stroke(core,d,options,points,count,TRUE,error);
  }
  if(g_str_equal(route,"path")) {
    OwnedPath*path=owned_path_new(gimp_item_get_image(GIMP_ITEM(d)),"owned generic oracle");
    for(int n=0;n<2;++n){GimpCoords a=GIMP_COORDS_DEFAULT_VALUES,b=GIMP_COORDS_DEFAULT_VALUES;
      a.x=23;a.y=17+n*23;a.pressure=.7;b=a;b.x=49;
      GimpStroke*stroke;
#ifdef OWNED_GENERIC_OLD
      /* Old lineto prepends anchors; GIMP3 appends. Match the stored direction,
       * not opposite traversal from superficially identical construction calls. */
      stroke=gimp_bezier_stroke_new_moveto(&b);gimp_bezier_stroke_lineto(stroke,&a);
#else
      stroke=gimp_bezier_stroke_new_moveto(&a);gimp_bezier_stroke_lineto(stroke,&b);
#endif
      if(g_getenv("PAINTER_GENERIC_COORDS_PROBE")){gboolean closed;GArray*points=gimp_stroke_interpolate(stroke,1.,&closed);GimpCoords*c=(GimpCoords*)points->data;fprintf(stderr,"PATH_POINTS %u %g %g %g -> %g %g %g\n",points->len,c[0].x,c[0].y,c[0].pressure,c[points->len-1].x,c[points->len-1].y,c[points->len-1].pressure);g_array_free(points,TRUE);}
      owned_path_add(path,stroke);g_object_unref(stroke);}
    gboolean result=owned_stroke_path(core,d,options,FALSE,path,TRUE,error);g_object_unref(path);return result;
  }
  if(g_str_equal(route,"boundary")) {
    OwnedBoundSeg segs[]={{22,13,44,13,1,0},{44,13,44,24,1,0},{44,24,22,24,1,0},{22,24,22,13,1,0},
                         {22,32,44,32,1,0},{44,32,44,44,1,0},{44,44,22,44,1,0},{22,44,22,32,1,0}};
    return gimp_paint_core_stroke_boundary(core,d,options,TRUE,segs,8,2,1,TRUE,error);
  }
  g_error("Unknown generic route");return FALSE;
}
#undef OwnedPath
#undef owned_path_new
#undef owned_path_add
#undef owned_stroke_path
#undef OwnedBoundSeg

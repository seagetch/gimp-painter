#include <cairo.h>
#include <functional>
#include <cmath>
#include <cstdio>
#include <cassert>
using gint=int;using gdouble=double;
struct GimpDisplayShell { double offset_x=0,offset_y=0,scale_x=1,scale_y=1; };
struct Guide {int n;int get_vanish_point_length(){return n;}void get_vanish_points(int i,double*x,double*y){*x=40+40*i;*y=50+30*i;}};
struct PerspectiveGuideInterface {static Guide*cast(Guide*g){return g;}};
void gimp_display_shell_set_cairo_rotate(GimpDisplayShell*,cairo_t*){}
static cairo_path_t*path;
void _gimp_canvas_item_stroke(cairo_t*cr){path=cairo_copy_path(cr);}
struct Ref {auto operator[](void(*f)(cairo_t*))->void(*)(cairo_t*){return f;}};
Ref ref(int){return {};}
class CanvasPerspectiveGuide {public:Guide*guide;int g_object=0;void draw(GimpDisplayShell*,cairo_t*);};
#include "draw-source.inc"
int main(){
 for(int n=1;n<=3;n++){
  auto*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,256,256);auto*cr=cairo_create(s);Guide g{n};CanvasPerspectiveGuide overlay;overlay.guide=&g;GimpDisplayShell shell;overlay.draw(&shell,cr);
  int radial=0,lines=0;double x=0,y=0;
  for(int i=0;i<path->num_data;i+=path->data[i].header.length){auto*d=&path->data[i];
   if(d->header.type==CAIRO_PATH_MOVE_TO){x=d[1].point.x;y=d[1].point.y;}
   else if(d->header.type==CAIRO_PATH_LINE_TO){lines++;if(d[1].point.x==x+8&&d[1].point.y==y)radial++;x=d[1].point.x;y=d[1].point.y;}
   else if(d->header.type==CAIRO_PATH_CURVE_TO){x=d[3].point.x;y=d[3].point.y;}}
  std::printf("points=%d radial_segments=%d line_segments=%d\n",n,radial,lines);assert(radial==(n==2?1:n));
  cairo_path_destroy(path);cairo_destroy(cr);cairo_surface_destroy(s);
 }
}

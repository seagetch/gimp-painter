/* SPDX-License-Identifier: GPL-3.0-or-later
 * Linked against the pinned legacy libappcore; no port implementation linked.
 */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "core/core-types.h"
#include "core/gimpperspectiveguide.h"
static void point(GimpPerspectiveGuide *guide,int index)
{double x=-1,y=-1;gboolean ok=gimp_perspective_guide_get_vanish_points(guide,index,&x,&y);printf("POINT %d %d %.17g %.17g\n",index,ok,x,y);}
int main(void)
{
  GimpPerspectiveGuide *guide=gimp_perspective_guide_new(97);guint id=0;double angle=-1;
  g_object_get(guide,"id",&id,"angle",&angle,NULL);printf("PROPERTY initial %u %.17g\n",id,angle);
  for(int i=0;i<4;++i){int n=gimp_perspective_guide_add_vanish_points(guide,i*10+1,i*10+2);printf("ADD %d %d %d\n",i,n,gimp_perspective_guide_get_vanish_point_length(guide));}
  for(int i=0;i<3;++i)point(guide,i);
  printf("MOVE %d\n",gimp_perspective_guide_set_vanish_points(guide,1,30,40));point(guide,1);
  printf("REMOVE %d\n",gimp_perspective_guide_remove_vanish_points(guide,1));
  for(int i=0;i<2;++i)point(guide,i);
  g_object_set(guide,"angle",1.25,NULL);g_object_get(guide,"id",&id,"angle",&angle,NULL);printf("PROPERTY angle-set %u %.17g\n",id,angle);
  g_object_unref(guide);return 0;
}

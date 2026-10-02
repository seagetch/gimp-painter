/* SPDX-License-Identifier: GPL-3.0-or-later
 * Actual unchanged pinned default GimpDynamics/GimpCurve constructor output. */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <stdio.h>
#include "core/core-types.h"
#include "core/gimpdynamics.h"
#include "core/gimpdynamicsoutput.h"
#include "core/gimpcurve.h"
int main(int argc,char **argv) {
  gegl_init(&argc,&argv);
  GimpDynamics*d=g_object_new(GIMP_TYPE_DYNAMICS,"name","default geometry dynamics",NULL);
  GimpCurve*c=NULL;g_object_get(gimp_dynamics_get_output(d,GIMP_DYNAMICS_OUTPUT_FORCE),"pressure-curve",&c,NULL);
  printf("CURVE_DEFAULT %d %d %d\n",c->identity,c->n_points,c->n_samples);
  for(int i=0;i<c->n_samples;++i)printf("CURVE_SAMPLE %d %a\n",i,c->samples[i]);
  g_object_unref(c);g_object_unref(d);return 0;
}

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/painter/filter-colorify-kernel.hpp"
#include <optional>
static std::optional<GimpPainter::FilterColorifyKernel> kernel;
extern "C" void old_colorify_init(double r,double g,double b,double a){kernel.emplace(GimpPainter::FilterColorifyColor{r,g,b,a});}
extern "C" void old_colorify_pixel(const unsigned char *in,unsigned char *out,int channels){kernel->pixel(in,out,channels);}
extern "C" int old_colorify_lut(int i){return kernel->tint_component(0,i);}

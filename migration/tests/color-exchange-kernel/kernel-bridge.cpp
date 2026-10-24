/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "app/painter/filter-color-exchange-private.hpp"
#include "app/painter/filter-color-exchange-private.hpp"
extern "C" void color_exchange_kernel(const unsigned char*,unsigned char*,const unsigned char*,int) noexcept;
extern "C" void color_exchange_kernel(const unsigned char*input,unsigned char*output,const unsigned char*args,int bpp) noexcept {
 GimpPainter::FilterColorExchangeOptions options;
 for(unsigned c=0;c<3;++c){options.from[c]=args[c];options.to[c]=args[3+c];options.threshold[c]=args[6+c];}
 const std::array<std::uint8_t,4> pixel={{input[0],input[1],input[2],std::uint8_t(bpp==4?input[3]:255)}};
 auto result=GimpPainter::filter_color_exchange_pixel(pixel,options);
 for(int c=0;c<bpp;++c)output[c]=result[c];
}

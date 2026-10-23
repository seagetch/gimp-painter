/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "paint/painter-mypaint-surface/buffer-read.hpp"

static void temp_buf_read_domains()
{
  const char *formats[] = {"Y' u8", "Y'A u8", "R'G'B' u8", "R'G'B'A u8"};
  for (const char *name : formats)
    {
      const auto *format = babl_format(name);
      const int bpp = babl_format_get_bytes_per_pixel(format);
      auto *temp = gimp_temp_buf_new(13, 9, format);
      std::vector<guchar> original(13 * 9 * bpp);
      for (std::size_t i = 0; i < original.size(); ++i) original[i] = 1 + i % 251;
      std::memcpy(gimp_temp_buf_get_data(temp), original.data(), original.size());
      auto *parent = gimp_temp_buf_create_buffer(temp);
      const GeglRectangle outer = {3, 2, 7, 5}, inner = {4, 3, 4, 3};
      auto *child = gegl_buffer_create_sub_buffer(parent, &outer);
      auto *nested = gegl_buffer_create_sub_buffer(child, &inner);
      // The real TempBuf owner and both ancestor buffer handles may go first.
      gimp_temp_buf_unref(temp); g_object_unref(parent); g_object_unref(child);
      for (const auto& request : {GeglRectangle{2,1,8,7}, {-2,-1,8,7}, {4,3,4,3}})
        {
          const std::size_t stride = request.width * bpp + 11;
          std::vector<guchar> result(stride * request.height + 14, 0xa5);
          GimpPainter::MyPaint::BufferRead::into(nested, request, format,
            result.data() + 7, stride * request.height, stride);
          for (int y = 0; y < request.height; ++y)
            {
              for (int x = 0; x < request.width; ++x)
                for (int c = 0; c < bpp; ++c)
                  {
                    const int xx = request.x + x, yy = request.y + y;
                    const bool inside = xx >= 4 && xx < 8 && yy >= 3 && yy < 6;
                    const guchar expected = inside ? original[(yy * 13 + xx) * bpp + c] : 0;
                    g_assert_cmpuint(result[7 + y * stride + x * bpp + c], ==, expected);
                  }
              for (std::size_t x = request.width * bpp; x < stride; ++x)
                g_assert_cmpuint(result[7 + y * stride + x], ==, 0xa5);
            }
          for (unsigned i = 0; i < 7; ++i)
            { g_assert_cmpuint(result[i], ==, 0xa5); g_assert_cmpuint(result[result.size()-1-i], ==, 0xa5); }
        }
      const auto independent = GimpPainter::MyPaint::BufferRead::packed(nested, inner);
      gegl_buffer_clear(nested, &inner); g_object_unref(nested);
      for (int y = 0; y < 3; ++y)
        g_assert_cmpmem(independent.data() + y * 4 * bpp, 4 * bpp,
                        original.data() + ((y + 3) * 13 + 4) * bpp, 4 * bpp);
    }
}

static void surface_combined_read_domains()
{
  for (bool wide : {false, true})
    {
      const auto *format = babl_format(wide ? "R'G'B'A double" : "R'G'B'A u8");
      std::vector<std::vector<guchar>> outputs;
      for (int shift : {-6, 0})
        {
          const GeglRectangle backing = {shift-2,shift-2,16,16};
          const GeglRectangle extent = {shift,shift,12,12};
          auto *parent = gegl_buffer_new(&backing,format);
          auto *blue = gegl_color_new("blue"); gegl_buffer_set_color(parent,&backing,blue); g_object_unref(blue);
          auto *target = gegl_buffer_create_sub_buffer(parent,&extent);
          const auto baseline = GimpPainter::MyPaint::BufferRead::packed(parent,backing);
          auto *snapshot = gegl_buffer_dup(target);
          const GeglRectangle selection = {shift+3-13,shift+2+17,6,7};
          auto *mask = gegl_buffer_new(&selection,babl_format("Y u8"));
          std::vector<guchar> coverage(42,255); coverage[15]=0;
          gegl_buffer_set(mask,&selection,0,babl_format("Y u8"),coverage.data(),GEGL_AUTO_ROWSTRIDE);
          {
            GeglSurface surface(target); surface.set_non_incremental(true); surface.set_stroke_opacity(.65f);
            surface.set_shape_provider([](float,float,float,float){return ShapeMask{9,9,std::vector<guchar>(81,255)};});
            surface.set_selection(mask,-13,17); surface.set_texture(Texture{2,3,{20,250,180,70,220,40}});
            surface.begin_session_from(snapshot); g_object_unref(snapshot); g_object_unref(mask);
            for (int n=0;n<2;++n)
              g_assert_true(surface.draw_dab(shift+6,shift+6,4,1,0,0,.6f,1,1,1,0,0,0,.7f,1));
            outputs.push_back(GimpPainter::MyPaint::BufferRead::packed(target,extent));
            // The sparse selected ROI, not the allocated parent, is writable.
            const auto after = GimpPainter::MyPaint::BufferRead::packed(parent,backing);
            const int bpp=babl_format_get_bytes_per_pixel(format); unsigned changed=0;
            for (int y=0;y<16;++y) for(int x=0;x<16;++x)
              {
                const int local_x=x-2,local_y=y-2; const auto i=(y*16+x)*bpp;
                const bool selected=local_x>=3&&local_x<9&&local_y>=2&&local_y<9&&!(local_x==6&&local_y==4);
                if(!selected)g_assert_cmpmem(after.data()+i,bpp,baseline.data()+i,bpp);
                else changed+=std::memcmp(after.data()+i,baseline.data()+i,bpp)!=0;
              }
            g_assert_cmpuint(changed,>,0);
            surface.cancel_session();g_assert_true(GimpPainter::MyPaint::BufferRead::packed(parent,backing)==baseline);
          }
          g_object_unref(target);g_object_unref(parent);
        }
      g_assert_true(outputs[0]==outputs[1]);
    }
}

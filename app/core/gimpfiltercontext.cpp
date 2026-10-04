/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core-types.h"
#include "gimp.h"
#include "gimpchannel.h"
#include "gimpcontext.h"
#include "gimpdrawable.h"
#include "gimpimage.h"
#include "gimplayer.h"
#include "gegl/gimp-babl.h"
}
#include "gimpfiltercontext.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace GimpPainter {
namespace {
constexpr std::size_t maximum_pixels = 32768;

std::size_t next_count (std::int32_t width, std::int32_t height,
                        std::size_t cursor, std::size_t budget)
{
  budget = std::max<std::size_t> (1, std::min (maximum_pixels, budget));
  const auto total = std::size_t (width) * height;
  if (cursor >= total) return 0;
  const auto x = cursor % width;
  if (x || std::size_t (width) > budget)
    return std::min ({total - cursor, std::size_t (width) - x, budget});
  return std::min (total - cursor, (budget / width) * width);
}

GeglRectangle rectangle (std::int32_t width, std::size_t offset, std::size_t count)
{
  const auto x = offset % width;
  if ((x && count > std::size_t (width) - x) ||
      (!x && count > std::size_t (width) && count % width))
    throw std::runtime_error ("Filter context chunk crosses an incomplete scanline");
  return { int (offset % width), int (offset / width),
           int (count < std::size_t (width) ? count : width),
           int (count < std::size_t (width) ? 1 : count / width) };
}

const Babl *pixel_format (GimpDrawable *drawable, bool real_samples)
{
  const auto base = gimp_drawable_get_base_type (drawable);
  const auto precision = real_samples ?
    gimp_babl_precision (GIMP_COMPONENT_TYPE_DOUBLE, gimp_drawable_get_trc (drawable)) :
    GIMP_PRECISION_U8_NON_LINEAR;
  return gimp_babl_format (base, precision, TRUE,
                           babl_format_get_space (gimp_drawable_get_format (drawable)));
}

ObjectRef<GObject> image_for (GimpDrawable *drawable)
{
  auto *image = gimp_item_get_image (GIMP_ITEM (drawable));
  if (!image) throw std::runtime_error ("Filter context image was closed");
  return ObjectRef<GObject>::retain (G_OBJECT (image));
}
}

void FilterOwnerContext::reset () noexcept
{
  input_.reset (); mask_.reset ();
  width_ = height_ = 0; input_cursor_ = mask_cursor_ = 0;
  bounds_ = {}; final_region_ = {};
  selection_dirty_ = true; bounds_valid_ = scan_started_ = false;
  final_started_ = final_ready_ = no_merge_ = false;
  real_samples_ = false;
}

void FilterOwnerContext::capture_input (GimpDrawable *drawable, std::size_t offset,
                                       std::size_t count, const std::uint8_t *rgba)
{
  if (!offset)
    {
      const auto base = gimp_drawable_get_base_type (drawable);
      if (!GIMP_IS_LAYER (drawable) || !gimp_drawable_has_alpha (drawable) ||
          (base != GIMP_RGB && base != GIMP_GRAY))
        throw std::runtime_error ("Filter context requires an alpha-bearing RGB or Gray layer");
      reset ();
      width_ = gimp_item_get_width (GIMP_ITEM (drawable));
      height_ = gimp_item_get_height (GIMP_ITEM (drawable));
      channels_ = gimp_drawable_get_base_type (drawable) == GIMP_GRAY ? 2 : 4;
      real_samples_ = gimp_drawable_get_precision (drawable) != GIMP_PRECISION_U8_NON_LINEAR;
      if (width_ <= 0 || height_ <= 0 ||
          std::size_t (width_) > std::numeric_limits<std::size_t>::max () / height_)
        throw std::runtime_error ("Invalid Filter context extent");
      const GeglRectangle extent { 0, 0, width_, height_ };
      input_ = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (&extent, pixel_format (drawable, real_samples_))));
    }
  const auto total = std::size_t (width_) * height_;
  if (!input_ || offset != input_cursor_ || offset > total || !rgba ||
      !count || count > maximum_pixels || count > total - offset)
    throw std::runtime_error ("Invalid Filter context input chunk");
  const auto rect = rectangle (width_, offset, count);
  const auto *format = gegl_buffer_get_format (GEGL_BUFFER (input_.get ()));
  if (real_samples_)
    {
      std::vector<double> native (count * channels_);
      if (channels_ == 4) std::memcpy (native.data (), rgba, count * 4 * sizeof (double));
      else
        for (std::size_t i = 0; i < count; ++i)
          {
            double pixel[4]; std::memcpy (pixel, rgba + i * 4 * sizeof (double), sizeof (pixel));
            if (pixel[0] != pixel[1] || pixel[0] != pixel[2])
              throw std::runtime_error ("Gray Filter input returned unequal channels");
            native[i * 2] = pixel[0]; native[i * 2 + 1] = pixel[3];
          }
      for (double sample : native)
        if (!std::isfinite (sample))
          throw std::runtime_error ("Nonfinite native Filter input sample");
      gegl_buffer_set (GEGL_BUFFER (input_.get ()), &rect, 0, format, native.data (), GEGL_AUTO_ROWSTRIDE);
    }
  else if (channels_ == 4)
    gegl_buffer_set (GEGL_BUFFER (input_.get ()), &rect, 0, format, rgba, GEGL_AUTO_ROWSTRIDE);
  else
    {
      std::vector<std::uint8_t> native (count * 2);
      for (std::size_t i = 0; i < count; ++i)
        { native[i * 2] = rgba[i * 4]; native[i * 2 + 1] = rgba[i * 4 + 3]; }
      gegl_buffer_set (GEGL_BUFFER (input_.get ()), &rect, 0, format, native.data (), GEGL_AUTO_ROWSTRIDE);
    }
  input_cursor_ += count;
}

bool FilterOwnerContext::bounds_quantum (GimpDrawable *drawable, std::size_t budget)
{
  auto image = image_for (drawable);
  auto selection = ObjectRef<GObject>::retain (G_OBJECT (gimp_image_get_mask (GIMP_IMAGE (image.get ()))));
  auto *channel = GIMP_CHANNEL (selection.get ());
  if (selection_dirty_)
    {
      selection_dirty_ = false; bounds_valid_ = scan_started_ = false;
      final_started_ = false; mask_cursor_ = 0;
      /* Do not destroy an already sealed snapshot from a context-only edit
       * during import. This function is never called again for that import. */
    }
  if (bounds_valid_) return true;
  // The host cache tests native bit patterns for nonzero, including negative
  // coverage and -0. Native coverage is clamped and rejects nonfinite values,
  // so discover those bounds with the same numeric semantics in bounded reads.
  if (channel->bounds_known && !real_samples_)
    {
      bounds_ = { bool (channel->empty), channel->x1, channel->y1, channel->x2, channel->y2 };
      bounds_valid_ = true;
      return true;
    }
  const auto width = gimp_item_get_width (GIMP_ITEM (channel));
  const auto height = gimp_item_get_height (GIMP_ITEM (channel));
  if (!scan_started_)
    {
      if (!scan_.reset (width, height)) throw std::runtime_error ("Invalid Filter selection extent");
      scan_started_ = true;
    }
  const auto offset = std::size_t (scan_.consumed ());
  const auto count = next_count (width, height, offset, budget);
  const auto rect = rectangle (width, offset, count);
  auto buffer = ObjectRef<GObject>::retain (G_OBJECT (gimp_drawable_get_buffer (GIMP_DRAWABLE (channel))));
  bool appended;
  if (real_samples_)
    {
      std::vector<double> coverage (count);
      gegl_buffer_get (GEGL_BUFFER (buffer.get ()), &rect, 1.0, babl_format ("Y double"),
                       coverage.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (selection_dirty_) return false;
      appended = scan_.append_native (offset, coverage.data (), count);
    }
  else
    {
      std::vector<std::uint8_t> bytes (count);
      gegl_buffer_get (GEGL_BUFFER (buffer.get ()), &rect, 1.0, babl_format ("Y u8"),
                       bytes.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (selection_dirty_) return false;
      appended = scan_.append (offset, bytes.data (), count);
    }
  if (!appended) throw std::runtime_error ("Invalid Filter selection capture chunk");
  if (scan_.finish (bounds_)) bounds_valid_ = true;
  return false; // The scan used this owner's bounded quantum, even if finished.
}

bool FilterOwnerContext::before_process (GimpDrawable *drawable,
                                        FilterProcedureRequest& request,
                                        std::size_t budget)
{
  if (!input_ || input_cursor_ != std::size_t (width_) * height_)
    throw std::runtime_error ("Filter context input is incomplete");
  if (!bounds_quantum (drawable, budget)) return false;
  if (!filter_selection_region (bounds_, gimp_item_get_offset_x (GIMP_ITEM (drawable)),
                                 gimp_item_get_offset_y (GIMP_ITEM (drawable)), width_, height_,
                                 request.start_region))
    throw std::runtime_error ("Invalid Filter start selection bounds");
  request.raw_shadow = true;
  auto image = image_for (drawable);
  double rgba[4];
  gegl_color_get_pixel (gimp_context_get_background (gimp_get_user_context (GIMP_IMAGE (image.get ())->gimp)),
                        babl_format ("R'G'B'A double"), rgba);
  const auto byte = [] (double v) {
    return std::uint8_t (std::floor (std::max (0.0, std::min (1.0, v)) * 255.0 + 0.5));
  };
  if (channels_ == 2)
    {
      const auto y = byte (0.2126 * rgba[0] + 0.7152 * rgba[1] + 0.0722 * rgba[2]);
      request.background = {{ y, y, y, 255 }};
    }
  else request.background = {{ byte (rgba[0]), byte (rgba[1]), byte (rgba[2]), 255 }};
  return !selection_dirty_;
}

void FilterOwnerContext::seal_components (GimpDrawable *drawable)
{
  auto image = image_for (drawable);
  auto mask = unsigned (gimp_image_get_active_mask (GIMP_IMAGE (image.get ())));
  // The old layer merge looked at the target's own lock, never its ancestors.
  if (gimp_layer_get_lock_alpha (GIMP_LAYER (drawable))) mask &= ~GIMP_COMPONENT_MASK_ALPHA;
  active_ = channels_ == 2 ? ((mask & GIMP_COMPONENT_MASK_RED) ? 1U : 0U) |
                             ((mask & GIMP_COMPONENT_MASK_ALPHA) ? 2U : 0U) : mask;
  final_ready_ = true;
}

bool FilterOwnerContext::before_import (GimpDrawable *drawable,
                                       FilterProcedureDisposition disposition,
                                       std::size_t budget)
{
  if (final_ready_) return true;
  if (!input_ || input_cursor_ != std::size_t (width_) * height_)
    throw std::runtime_error ("Filter context input is incomplete at final merge");
  if (disposition == FilterProcedureDisposition::no_merge)
    { no_merge_ = true; final_ready_ = true; return true; }
  if (disposition != FilterProcedureDisposition::shadow)
    throw std::runtime_error ("Filter returned no usable shadow disposition");
  if (!bounds_quantum (drawable, budget)) return false;
  if (!final_started_)
    {
      if (!filter_selection_region (bounds_, gimp_item_get_offset_x (GIMP_ITEM (drawable)),
                                     gimp_item_get_offset_y (GIMP_ITEM (drawable)), width_, height_,
                                     final_region_))
        throw std::runtime_error ("Invalid Filter final selection bounds");
      final_started_ = true; mask_cursor_ = 0;
      if (!final_region_.selected || !final_region_.intersects)
        { mask_.reset (); seal_components (drawable); return true; }
      if (!mask_)
        {
          const GeglRectangle extent { 0, 0, width_, height_ };
          mask_ = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (&extent,
            babl_format (real_samples_ ? "Y double" : "Y u8"))));
        }
    }
  const auto total = std::size_t (width_) * height_;
  if (mask_cursor_ == total)
    { seal_components (drawable); return true; }
  const auto count = next_count (width_, height_, mask_cursor_, budget);
  const auto local = rectangle (width_, mask_cursor_, count);
  auto source = local;
  const auto source_x = std::int64_t (source.x) + gimp_item_get_offset_x (GIMP_ITEM (drawable));
  const auto source_y = std::int64_t (source.y) + gimp_item_get_offset_y (GIMP_ITEM (drawable));
  if (source_x < G_MININT || source_x > G_MAXINT || source_y < G_MININT || source_y > G_MAXINT)
    throw std::runtime_error ("Filter selection offset exceeds the native coordinate range");
  source.x = source_x; source.y = source_y;
  auto image = image_for (drawable);
  auto selection = ObjectRef<GObject>::retain (G_OBJECT (gimp_image_get_mask (GIMP_IMAGE (image.get ()))));
  auto buffer = ObjectRef<GObject>::retain (G_OBJECT (gimp_drawable_get_buffer (GIMP_DRAWABLE (selection.get ()))));
  const auto *format = gegl_buffer_get_format (GEGL_BUFFER (mask_.get ()));
  if (real_samples_)
    {
      std::vector<double> coverage (count);
      gegl_buffer_get (GEGL_BUFFER (buffer.get ()), &source, 1.0, format,
                       coverage.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (selection_dirty_) return false;
      for (double sample : coverage)
        if (!std::isfinite (sample))
          throw std::runtime_error ("Nonfinite native Filter selection sample");
      gegl_buffer_set (GEGL_BUFFER (mask_.get ()), &local, 0, format,
                       coverage.data (), GEGL_AUTO_ROWSTRIDE);
    }
  else
    {
      std::vector<std::uint8_t> bytes (count);
      gegl_buffer_get (GEGL_BUFFER (buffer.get ()), &source, 1.0, format,
                       bytes.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (selection_dirty_) return false;
      gegl_buffer_set (GEGL_BUFFER (mask_.get ()), &local, 0, format,
                       bytes.data (), GEGL_AUTO_ROWSTRIDE);
    }
  mask_cursor_ += count;
  return false;
}

void FilterOwnerContext::merge_chunk (std::size_t offset, std::size_t count,
                                     const std::uint8_t *shadow_rgba,
                                     std::vector<std::uint8_t>& rgba)
{
  const auto total = std::size_t (width_) * height_;
  if (!final_ready_ || !input_ || offset > total || !count || count > maximum_pixels || count > total - offset)
    throw std::runtime_error ("Invalid Filter final merge chunk");
  const auto rect = rectangle (width_, offset, count);
  const auto *format = gegl_buffer_get_format (GEGL_BUFFER (input_.get ()));
  const bool merge = !no_merge_ && (!final_region_.selected || final_region_.intersects);
  if (merge && !shadow_rgba)
    throw std::runtime_error ("Missing Filter shadow merge pixels");
  if (real_samples_)
    {
      std::vector<double> original (count * channels_);
      gegl_buffer_get (GEGL_BUFFER (input_.get ()), &rect, 1.0, format,
                       original.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
      if (merge)
        {
          std::vector<double> selection;
          if (final_region_.selected)
            {
              selection.resize (count);
              gegl_buffer_get (GEGL_BUFFER (mask_.get ()), &rect, 1.0, babl_format ("Y double"),
                               selection.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
            }
          std::vector<double> shadow (count * channels_);
          if (channels_ == 4) std::memcpy (shadow.data (), shadow_rgba, count * 4 * sizeof (double));
          else
            for (std::size_t i = 0; i < count; ++i)
              {
                double pixel[4];
                std::memcpy (pixel, shadow_rgba + i * 4 * sizeof (double), sizeof (pixel));
                if (pixel[0] != pixel[1] || pixel[0] != pixel[2])
                  throw std::runtime_error ("Gray Filter shadow returned unequal channels");
                shadow[i * 2] = pixel[0]; shadow[i * 2 + 1] = pixel[3];
              }
          if (!filter_replace_native_row (original.data (), shadow.data (),
                                           selection.empty () ? nullptr : selection.data (),
                                           original.data (), count, channels_, active_))
            throw std::runtime_error ("Invalid native Filter shadow merge inputs");
        }
      rgba.resize (count * 4 * sizeof (double));
      if (channels_ == 4) std::memcpy (rgba.data (), original.data (), rgba.size ());
      else
        for (std::size_t i = 0; i < count; ++i)
          {
            const double pixel[4] = { original[i * 2], original[i * 2], original[i * 2], original[i * 2 + 1] };
            std::memcpy (rgba.data () + i * sizeof (pixel), pixel, sizeof (pixel));
          }
      return;
    }
  std::vector<std::uint8_t> original (count * channels_);
  gegl_buffer_get (GEGL_BUFFER (input_.get ()), &rect, 1.0, format,
                   original.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  if (merge)
    {
      std::vector<std::uint8_t> selection;
      if (final_region_.selected)
        {
          selection.resize (count);
          gegl_buffer_get (GEGL_BUFFER (mask_.get ()), &rect, 1.0, babl_format ("Y u8"),
                           selection.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        }
      std::vector<std::uint8_t> gray_shadow;
      const std::uint8_t *shadow = shadow_rgba;
      if (channels_ == 2)
        {
          gray_shadow.resize (count * 2);
          for (std::size_t i = 0; i < count; ++i)
            {
              if (shadow_rgba[i * 4] != shadow_rgba[i * 4 + 1] || shadow_rgba[i * 4] != shadow_rgba[i * 4 + 2])
                throw std::runtime_error ("Gray Filter shadow returned unequal channels");
              gray_shadow[i * 2] = shadow_rgba[i * 4]; gray_shadow[i * 2 + 1] = shadow_rgba[i * 4 + 3];
            }
          shadow = gray_shadow.data ();
        }
      if (!filter_replace_inten_row (original.data (), shadow,
                                      selection.empty () ? nullptr : selection.data (), original.data (),
                                      count, channels_, active_))
        throw std::runtime_error ("Invalid Filter shadow merge inputs");
    }
  if (channels_ == 4) rgba = std::move (original);
  else
    {
      rgba.resize (count * 4);
      for (std::size_t i = 0; i < count; ++i)
        {
          rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = original[i * 2];
          rgba[i * 4 + 3] = original[i * 2 + 1];
        }
    }
}
} // namespace GimpPainter

/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_NORMALIZE_KERNEL_HPP
#define GIMP_PAINTER_FILTER_NORMALIZE_KERNEL_HPP
#include "gimp-painter-visibility.h"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Bounded legacy-defined nonlinear byte domain only. No native admission,
 * ROI traversal, shadow ownership or indexed colormap semantics implied. */
enum class FilterNormalizeStatus { ready, empty, no_visible_samples, hidden_outside_range };
class FilterNormalizeStats;
class FilterNormalizeKernel final {
public:
  FilterNormalizeStatus status() const noexcept { return status_; }
  std::array<std::uint8_t,4> apply(const std::array<std::uint8_t,4>& p) const {
    if (status_ != FilterNormalizeStatus::ready)
      throw std::logic_error("Normalize undefined legacy input domain");
    /* Guard misuse against a pixel not contained in the finalized scan. */
    for (unsigned i=0;i<3;++i)
      if (p[i]<low_ || p[i]>high_)
        throw std::invalid_argument("Normalize pixel outside scanned interval");
    return {{lut_[p[0]],lut_[p[1]],lut_[p[2]],p[3]}};
  }
private:
  friend class FilterNormalizeStats;
  FilterNormalizeKernel(FilterNormalizeStatus s,unsigned low,unsigned high) noexcept
    : status_(s),low_(low),high_(high),lut_{} {
    if (s==FilterNormalizeStatus::ready)
      for (unsigned v=low;v<=high;++v)
        lut_[v]=static_cast<std::uint8_t>(low==high?v:255*(v-low)/(high-low));
  }
  FilterNormalizeStatus status_;
  unsigned low_,high_;
  std::array<std::uint8_t,256> lut_;
};
/* Collect actual ROI samples only, excluding transport padding/zero-fill.
 * RGB callers use alpha255; Gray may replicate color only after owner review.
 * No sample count arithmetic or allocation occurs in this scalar collector. */
class FilterNormalizeStats final {
public:
  void add(const std::array<std::uint8_t,4>& p) noexcept {
    any_=true;
    if (p[3]) visible_=true; else hidden_=true;
    unsigned& lo=p[3]?low_:hidden_low_;
    unsigned& hi=p[3]?high_:hidden_high_;
    for (unsigned i=0;i<3;++i) {
      if(p[i]<lo)lo=p[i];
      if(p[i]>hi)hi=p[i];
    }
  }
  FilterNormalizeKernel finish() const noexcept {
    auto status=!any_?FilterNormalizeStatus::empty:
      !visible_?FilterNormalizeStatus::no_visible_samples:
      hidden_&&(hidden_low_<low_||hidden_high_>high_)?
        FilterNormalizeStatus::hidden_outside_range:FilterNormalizeStatus::ready;
    return FilterNormalizeKernel(status,low_,high_);
  }
private:
  bool any_=false,visible_=false,hidden_=false;
  unsigned low_=255,high_=0,hidden_low_=255,hidden_high_=0;
};
}
#endif

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-normalize-raster.hpp"
#include "filter-normalize-kernel.hpp"
#include "filter-progress.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
namespace GimpPainter {
FilterNormalizeOutcome filter_normalize_raw_raster(FilterRaster& input,
  FilterRaster& output,std::size_t width,std::size_t height,std::atomic<bool>& cancel,
  const std::shared_ptr<FilterProgress>& progress)
{
  const auto maximum=std::uint64_t(std::numeric_limits<std::int32_t>::max());
  if(&input==&output || width>maximum || height>maximum ||
     (height && std::uint64_t(width)>maximum/height))
    throw std::invalid_argument("Normalize invalid compact geometry or alias");
  const auto size=std::uint64_t(width)*height*4;
  if(input.size()!=size || output.size()!=size)
    throw std::invalid_argument("Normalize requires exact compact RGBA8 extents");
  if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
  if(!size)return FilterNormalizeOutcome::empty;
  std::array<std::uint8_t,32768> bytes;
  FilterNormalizeStats stats;
  for(std::uint64_t offset=0;offset<size;){
    if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
    const auto count=std::size_t(std::min<std::uint64_t>(bytes.size(),size-offset));
    input.read(offset,count,bytes.data());
    if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
    for(std::size_t i=0;i<count;i+=4){
      if((i&1023)==0 && cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
      stats.add({{bytes[i],bytes[i+1],bytes[i+2],bytes[i+3]}});
    }
    offset+=count;if(progress)progress->set_value(0.49*double(offset)/size);
  }
  const auto kernel=stats.finish();
  if(kernel.status()==FilterNormalizeStatus::no_visible_samples)
    throw std::domain_error("Normalize unsupported: nonempty ROI has no nonzero-alpha samples");
  if(kernel.status()!=FilterNormalizeStatus::ready)
    throw std::domain_error("Normalize unsupported: hidden colors outside visible interval");
  for(std::uint64_t offset=0;offset<size;){
    if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
    const auto count=std::size_t(std::min<std::uint64_t>(bytes.size(),size-offset));
    input.read(offset,count,bytes.data());
    if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
    for(std::size_t i=0;i<count;i+=4){
      if((i&1023)==0 && cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
      const auto p=kernel.apply({{bytes[i],bytes[i+1],bytes[i+2],bytes[i+3]}});
      std::copy(p.begin(),p.end(),bytes.data()+i);
    }
    output.write(offset,count,bytes.data());
    if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
    offset+=count;if(progress)progress->set_value(0.49+0.50*double(offset)/size);
  }
  if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
  output.flush();
  if(cancel.load(std::memory_order_relaxed))return FilterNormalizeOutcome::cancelled;
  if(progress)progress->set_value(1.0);
  return FilterNormalizeOutcome::complete;
}
}

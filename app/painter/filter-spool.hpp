/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_SPOOL_HPP
#define GIMP_PAINTER_FILTER_SPOOL_HPP
#include "gimp-painter-visibility.h"
#include "filter-raster.hpp"
#include "work-admission.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Independent byte transport. The owner never opens, reads, writes or closes
 * a temporary file and never waits for the worker. There is one producer and
 * one consumer per direction, with at most two bounded chunks in each queue.
 * No callback crosses back into the application and no GObject is retained. */
class FilterSpool
{
public:
  using Bytes = std::vector<std::uint8_t>;
  using Process = std::function<bool (FilterRaster&, FilterRaster&,
                                      std::atomic<bool>&, const FilterRasterFactory&)>;
  struct Chunk { std::size_t offset; Bytes bytes; };
  enum class Phase { collecting, processing, exporting, complete };
  static constexpr std::size_t pixel_budget = 32768;
  FilterSpool (std::size_t width, std::size_t height, const std::string& directory,
               Process process, WorkAdmission::Lease lease, std::size_t bytes_per_pixel = 4);
  ~FilterSpool () noexcept;
  FilterSpool (const FilterSpool&) = delete;
  FilterSpool& operator= (const FilterSpool&) = delete;
  /* Owner-thread methods. Failed submit leaves bytes untouched. The caller
   * reads pixels only after can_submit(); consumer progress cannot fill it. */
  bool can_submit () const noexcept;
  bool submit (std::size_t offset, Bytes& bytes);
  void seal_input ();
  std::unique_ptr<Chunk> take_result () noexcept;
  void cancel () noexcept;
  Phase phase () const noexcept;
  bool started () const noexcept;
  bool done () const noexcept;
  bool succeeded () const noexcept;
  /* Read only after done() observes completion. Stable until destruction. */
  const std::string& error () const;
private:
  struct State;
  std::shared_ptr<State> state_;
  const std::thread::id owner_ = std::this_thread::get_id ();
  std::size_t submitted_ = 0;
  bool sealed_ = false;
};
}
#endif

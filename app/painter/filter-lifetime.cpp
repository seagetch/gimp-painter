/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-lifetime.hpp"
#include <atomic>
namespace GimpPainter {
namespace { std::atomic<std::size_t> count {0}; std::atomic<bool> stop {false}; }
FilterLifetime::FilterLifetime () noexcept { count.fetch_add (1, std::memory_order_acq_rel); }
FilterLifetime::~FilterLifetime () noexcept { count.fetch_sub (1, std::memory_order_acq_rel); }
void FilterLifetime::stop_all () noexcept { stop.store (true, std::memory_order_release); }
bool FilterLifetime::stopping () noexcept { return stop.load (std::memory_order_acquire); }
std::size_t FilterLifetime::pending () noexcept { return count.load (std::memory_order_acquire); }
}

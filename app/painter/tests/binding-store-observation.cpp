/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "binding-store-observation.hpp"
namespace StoreObservation {
std::atomic<std::size_t> stores {0}, slots {0};
}
extern "C" void painter_observe_store_resolution (int kind) noexcept
{
  (kind == 0 ? StoreObservation::stores : StoreObservation::slots)
    .fetch_add (1, std::memory_order_relaxed);
}

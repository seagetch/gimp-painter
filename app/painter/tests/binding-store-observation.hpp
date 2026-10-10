/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_TEST_BINDING_STORE_OBSERVATION_HPP
#define PAINTER_TEST_BINDING_STORE_OBSERVATION_HPP
#include <atomic>
#include <cstddef>

/* Defined only in observed test executables, never in libapppainter. */
extern "C" void painter_observe_store_resolution (int kind) noexcept;
namespace StoreObservation {
struct Counts { std::size_t stores, slots; };
extern std::atomic<std::size_t> stores, slots;
inline Counts snapshot () noexcept
{ return {stores.load (std::memory_order_relaxed), slots.load (std::memory_order_relaxed)}; }
inline Counts since (Counts start) noexcept
{ auto end = snapshot (); return {end.stores - start.stores, end.slots - start.slots}; }
}
#endif

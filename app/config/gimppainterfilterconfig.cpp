/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gio/gio.h>
extern "C" {
#include "config-types.h"
#include "gimpgeglconfig.h"
#include "gimppainterfilterconfig.h"
}
#include "gimppainterfilterconfig.hpp"
#include "painter/binding-store.hpp"
namespace GimpPainter {
template<> struct TypeTraits<GimpGeglConfig> { static GType type () { return GIMP_TYPE_GEGL_CONFIG; } };
namespace {
constexpr std::uint64_t default_spill = std::uint64_t (8) * 1024 * 1024 * 1024;
struct Config {
  std::uint64_t spill = default_spill;
  std::shared_ptr<WorkAdmission> pool = std::make_shared<WorkAdmission> (WorkAdmission::Limits {2,1024*1024*1024,default_spill});
  void set (std::uint64_t bytes) { pool->configure ({2,1024*1024*1024,bytes});spill = bytes; }
  void close () noexcept { pool->close (); }
};
struct ConfigSlot : SlotSpec<GimpGeglConfig,Config> {};
}
std::shared_ptr<WorkAdmission> filter_admission_for_config (GObject *owner)
{
  if (!GIMP_IS_GEGL_CONFIG (owner)) throw std::invalid_argument ("Expected native GEGL configuration");
  return BindingStore::require (owner).with<ConfigSlot> ([] (Config& self) { return self.pool; });
}
}
using namespace GimpPainter;
void gimp_painter_filter_config_init (GObject *owner)
{ boundary_void (nullptr,[&] { BindingStore::ensure (owner).emplace<ConfigSlot> (); }); }
void gimp_painter_filter_config_activate (GObject *owner)
{ boundary_void (nullptr,[&] { BindingStore::require (owner).activate (); }); }
void gimp_painter_filter_config_set_spill (GObject *owner, guint64 bytes)
{
  boundary_void (nullptr,[&] {
    auto& store = BindingStore::require (owner);
    const auto change = [&] (Config& self) { self.set (bytes); };
    if (store.state () == BindingStore::State::constructing) store.initialize<ConfigSlot> (change);
    else store.with<ConfigSlot> (change);
  });
}
guint64 gimp_painter_filter_config_get_spill (GObject *owner)
{ return boundary<guint64> (nullptr,default_spill,[&] { return BindingStore::require (owner).read<ConfigSlot> ([] (const Config& self) { return self.spill; }); }); }

/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform-abi.h"
#include "binding-store.hpp"
#include "resources.hpp"
#include <cstddef>

namespace {
struct Impl { void close () noexcept {} };
struct Slot : GimpPainter::SlotSpec<GObject, Impl> {};
}

#ifdef PAINTER_PLATFORM_NEGATIVE_EXPORT
namespace GimpPainter {
PAINTER_PLATFORM_EXPORT int platform_export_leak_control () { return 714; }
}
#endif

void painter_platform_cpp_layout (PainterPlatformLayout *layout)
{
  *layout = {
    sizeof (GObject), alignof (GObject), offsetof (GObject, ref_count),
    sizeof (GValue), alignof (GValue), offsetof (GValue, data),
    sizeof (GError), alignof (GError), offsetof (GError, message),
    sizeof (GimpPainterError), sizeof (gpointer), sizeof (long)
  };
}

gboolean
painter_platform_roundtrip (GObject *owner, PainterPlatformCallback callback,
                            gpointer context, gint64 *result, GError **error)
{
  return GimpPainter::boundary<gboolean> (error, FALSE, [&] {
    auto *store = GimpPainter::BindingStore::find (owner);
    if (!store)
      {
        store = &GimpPainter::BindingStore::ensure (owner);
        store->emplace<Slot> ();
        store->activate ();
      }
    *result = store->with<Slot> ([&] (Impl&) {
      GimpPainter::Value value (G_TYPE_INT64);
      g_value_set_int64 (value.get (), G_GINT64_CONSTANT (0x123456789abc));
      return callback (owner, value.get (), -714, 0.375,
                       "C/C++ callback ABI", context) + 1;
    });
    return TRUE;
  });
}

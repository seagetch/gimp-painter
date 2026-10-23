/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "test-painter-widget-exception-fault.h"
#include "painter/binding-store.hpp"

#include <stdexcept>

namespace
{
thread_local const GimpPainter::BindingStore *armed_store = nullptr;
thread_local PainterWidgetException armed_exception = PAINTER_WIDGET_EXCEPTION_STANDARD;
thread_local unsigned fault_count = 0;
}

/* GNU-compatible ELF linkers rewrite unresolved calls from the actual Canvas
 * and LayerTiles object files. check_thread() is not noexcept. This wrapper
 * neither replaces a production object nor injects through a GTK C callback. */
extern "C" void __real__ZNK11GimpPainter12BindingStore12check_threadEv
  (const GimpPainter::BindingStore *store);
extern "C" void __wrap__ZNK11GimpPainter12BindingStore12check_threadEv
  (const GimpPainter::BindingStore *store);

extern "C" void
__wrap__ZNK11GimpPainter12BindingStore12check_threadEv
  (const GimpPainter::BindingStore *store)
{
  if (store == armed_store)
    {
      const auto exception = armed_exception;

      /* Disarm before constructing/throwing so unwinding and warning delivery
       * cannot consume a second injected fault. Other owners never consume it. */
      armed_store = nullptr;
      ++fault_count;
      if (exception == PAINTER_WIDGET_EXCEPTION_STANDARD)
        throw std::runtime_error (PAINTER_WIDGET_EXCEPTION_MESSAGE);
      throw 17;
    }

  __real__ZNK11GimpPainter12BindingStore12check_threadEv (store);
}

extern "C" void
painter_widget_exception_fault_arm (GObject                *owner,
                                    PainterWidgetException  exception)
{
  g_assert_true (G_IS_OBJECT (owner));
  g_assert_null (armed_store);
  g_assert_true (exception == PAINTER_WIDGET_EXCEPTION_STANDARD ||
                 exception == PAINTER_WIDGET_EXCEPTION_UNKNOWN);

  auto *store = GimpPainter::BindingStore::find (owner);
  g_assert_nonnull (store);
  g_assert_true (store->state () == GimpPainter::BindingStore::State::active);

  fault_count = 0;
  armed_exception = exception;
  armed_store = store;
}

extern "C" gboolean
painter_widget_exception_fault_consumed (void)
{
  return armed_store == nullptr && fault_count == 1;
}

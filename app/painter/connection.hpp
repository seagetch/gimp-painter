/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_CONNECTION_HPP
#define GIMP_PAINTER_CONNECTION_HPP
#include "object-ref.hpp"

namespace GimpPainter {

/* Signal adapters supply an exact signal signature and catch exceptions.
 * GCallback is the GLib registration transport, never a vfunc cast. */
class Connection
{
public:
  Connection () = default;
  ~Connection () { close (); }
  Connection (const Connection&) = delete;
  Connection& operator= (const Connection&) = delete;
  Connection (Connection&& other) noexcept
    : emitter_ (std::move (other.emitter_)), id_ (other.id_), blocks_ (other.blocks_)
  { other.id_ = 0; other.blocks_ = 0; }
  Connection& operator= (Connection&& other) noexcept
  {
    if (this != &other) { Connection moved (std::move (other)); swap (moved); }
    return *this;
  }
  void swap (Connection& other) noexcept
  {
    std::swap (emitter_, other.emitter_);
    std::swap (id_, other.id_);
    std::swap (blocks_, other.blocks_);
  }

  static Connection connect (const ObjectRef<GObject>& emitter, const char *signal,
                             GCallback callback, gpointer data,
                             GClosureNotify destroy, GConnectFlags flags = GConnectFlags (0))
  {
    guint signal_id = 0;
    GQuark detail = 0;
    if (!emitter || !signal || !callback ||
        !g_signal_parse_name (signal, G_OBJECT_TYPE (emitter.get ()), &signal_id, &detail, TRUE))
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid signal connection");
    Connection result;
    result.emitter_ = WeakRef<GObject> (emitter); // allocate before transferring data
    result.id_ = g_signal_connect_data (emitter.get (), signal, callback, data, destroy, flags);
    return result;
  }

  bool connected () const noexcept
  {
    auto emitter = emitter_.lock ();
    return emitter && id_ && g_signal_handler_is_connected (emitter.get (), id_);
  }
  void block () noexcept
  {
    auto emitter = emitter_.lock ();
    if (emitter && id_ && g_signal_handler_is_connected (emitter.get (), id_))
      { g_signal_handler_block (emitter.get (), id_); ++blocks_; }
  }
  void unblock () noexcept
  {
    if (!blocks_) return;
    auto emitter = emitter_.lock ();
    if (emitter && id_ && g_signal_handler_is_connected (emitter.get (), id_))
      g_signal_handler_unblock (emitter.get (), id_);
    --blocks_;
  }
  void close () noexcept
  {
    /* Detach all member state before destroy notify can replace or even
     * destroy this Connection. Never access this again after disconnect. */
    auto old_emitter = std::move (emitter_);
    const gulong old_id = id_;
    id_ = 0;
    blocks_ = 0;
    auto emitter = old_emitter.lock ();
    if (emitter && old_id && g_signal_handler_is_connected (emitter.get (), old_id))
      g_signal_handler_disconnect (emitter.get (), old_id);
  }
private:
  WeakRef<GObject> emitter_;
  gulong id_ = 0;
  unsigned blocks_ = 0;
};
} // namespace GimpPainter
#endif

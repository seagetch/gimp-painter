# 01.007/connection-signal signal Connection

旧 `app/base/delegators.hpp:121-160` の `Connection` は constructor で `signal = g_strdup(signal_)` とする。destructor は `disconnect()` を呼ぶが、`disconnect` は closure を検索・切断して `target` と `closure` を NULL にするだけで、`signal` を解放しない。`signal` 自体は検索にも使われない。`app/base/glib-cxx-utils.hpp:166-179` の `g_signal_connect_delegator` は closure の destroy notify で delegator を `delete` し、新規 `Connection` を返す。Connection 側の `target` と `closure` は借用ポインターであり、先に target が破棄される経路は別途 `07.008` で検証する。

移植時には signal 名の保持を削除するか、Connection の destructor で一回だけ `g_free` する。明示 `disconnect()` と destructor の両方を通る場合にも多重解放しない。`07.008/connection-name` にこの変更と反復接続の検証を追加した。これは旧コードの静的追跡であり、GIMP 3 側の修正は未着手。

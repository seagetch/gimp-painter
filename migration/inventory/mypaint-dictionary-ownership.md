# 01.007/dict-transfer MyPaint 設定辞書

旧 `app/core/gimpmypaintbrush-load.cpp` の `hold(...)` は次の 7 箇所。五つの getter は旧 `app/core/mypaintbrush-brushsettings.c` で返却前に毎回 `g_hash_table_ref` する。`GLib::HashTable` は `ScopedPointer` を継承し、破棄時に `g_hash_table_unref` する (`app/base/glib-cxx-utils.hpp:396-416`, `app/base/scopeguard.hpp:13-22`)。`ref<Key,Value>(_dict)` は別の `IHashTable` 参照を増やし、その破棄時に解除する (`glib-cxx-utils.hpp:428-429`)。

| 旧呼出し行 | getter の定義行 | 辞書 |
| --- | --- | --- |
| `:277`, `:551` | `mypaintbrush-brushsettings.c:321-334` | brush settings |
| `:279`, `:553` | `:296-308` | input settings |
| `:345` | `:349-362` | brush switch settings |
| `:367` | `:378-391` | brush text settings |
| `:555` | `:395-408` | setting migration |

settings、switch、text、migration の getter は初回に `g_hash_table_new` の参照に加えて `g_hash_table_ref` を一回行う。input getter は初回の追加 ref がない。旧 C ファイル内ではこれらの静的辞書の終了時解放が見当たらない。GIMP 3 移植時はキャッシュ所有者を明示し、各 getter が一回の譲渡参照を返す契約とその解放を維持する。キャッシュの追加参照と他の C 呼出し元の解放も `08.007/dict-owner` で検証する。この記録は静的な所有権追跡であり、移植実装の完了ではない。

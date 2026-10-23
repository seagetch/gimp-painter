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


## 08.007/dict-owner の実装受入

上記は旧実装の履歴である。移植先では五種の heap 辞書キャッシュと
譲渡 getter を廃し、生成された immutable metadata、境界検査付き名前検索、
Resource/Mapping の値所有へ置換した。キャッシュ所有参照と caller 参照を
新たに作らないため、終了時に残る辞書参照もない。全カテゴリと六つの旧名を
八回の consumer 生成・コピー・破棄で再取得し、C/C++ metadata と native
options/config の実試験を通した。根拠と適用範囲は
[08.007 受入](../tests/mybrush-options-type/README.md) と
[source mapping](../tests/mybrush-options-type/source-mapping.json) に記録した。
旧 grouped UI の同等性や全 brush engine の完成をこの受入からは推論しない。

# 01.007/array-wrapper GLib 配列の再代入

旧 `app/base/glib-cxx-utils.hpp:305-329` の `GLib::Array` は `ScopedPointer<GArray,...,g_array_unref>` で参照を所有する。copy constructor は `g_array_ref` で追加参照を取得し、move constructor は移動元を NULL にする。`operator=(GArray*&&)` (`:318-322`) と `operator=(Array&&)` (`:324-328`) は、代入先の現在の `obj` を unref せずに上書きする。既存の配列を保持した状態で再代入するとその参照が失われる。自己移動は `obj = src.obj` の直後に同じ `obj` を NULL にするため、保持参照を失う。

旧 `app/base/soup-cxx-utils.hpp:263,368` の `_rules = g_array_new(...)` と `app/gimp-features.cpp:73` の代入は、既定構築直後の空 wrapper を初期化する使い方である。これらの個別の代入だけでは再代入漏れを再現しないが、共通 wrapper が公開する代入の契約には旧配列の解放と自己移動への対処が必要である。GIMP 3 移植時の `06.021/array-reassignment` に引き継ぎ、空・再代入・自己移動・copy の参照数を検証する。旧コードの静的追跡であり、移植実装は未着手。

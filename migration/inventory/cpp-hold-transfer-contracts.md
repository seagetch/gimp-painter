# 01.007 C++ `hold` 返却参照の追跡

旧版 `afa43fae` の `TRANSFER_CONTRACT_REVIEW` 11 箇所と `BORROWED_SOURCE_RISK` 1 箇所を、返却元と破棄関数に照合した。個別結果は `cpp-hold-transfer-contracts.tsv` に記録し、`python3 -B tools/audit_cpp_hold_transfer.py` で候補12件との一致を検査する。移植版の修正・動作試験は追跡先の WBS で行う。

MyPaint 辞書7箇所では getter が呼出し元用の参照を増やし、`HashTable` が scope 終了時に unref する。キャッシュ自体の寿命は `mypaint-dictionary-ownership.md` に別記した。`JSON::INode::keys()` は `json_object_get_members()` のリストを返し、`GLib::List` はノードだけを解放する。`create_replacement_layer()` は旧実装でリストへ追加せず常に NULL を返す。子レイヤーは image に追加する経路で管理される。

`get_args()` は新規 `GArray` に `g_value_unset` clear 関数を設定する。`IArray<GValue>` の一時参照を解放した後の返却参照を `hold()` が引き受け、`set_procedure()` の間は保持する。同関数は個別の引数を `ProcedureRunner::set_arg()` の `g_value_transform()` に渡す。値配列の深いコピーや一時 GValue の安全性は `15.001` 以下で修正・試験する。

二つの旧不備を確認した。`gimplayertileview.cpp:1282` は `new LayerPresetApplier` を一般の `hold(T*)` に渡すため、GObject 用 `g_object_unref` が選択される。`layer-preset-gui.cpp:68,78` の二つの呼出しは `new_instance()` の戻り値を解放しない。三経路とも `28.014/applier-owner` で C++ 所有として修正・試験する。`gimplayerpopup.cpp:927` は借用 widget を `hold()` で所有扱いにするため、`29.006/menu-borrowed-reference` に追跡する。

この表は旧コードの所有権契約と不備の確認であり、GIMP 3 側での解消を示さない。残る `ref/unref`、`GValue`、`CXXPointer` の参照箇所との照合が `01.007/cpp-ownership` に残る。

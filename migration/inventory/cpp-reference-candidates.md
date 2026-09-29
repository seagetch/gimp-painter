# 01.007 旧 C++ 参照管理の候補

旧版 71 C++ ファイルで、`hold(...)`、`GValue` と操作関数、`g_object_ref*` / `g_object_unref`、`CXXPointer<T>` の構文候補 242 件を `cpp-reference-candidates.tsv` に抽出した。1 行が複数分類に該当するときは複数行に分ける。`GValue` の型宣言や helper 定義も含み、未使用・コメント内・所有権なしの候補はまだ除外していない。

旧 `app/base/scopeguard.hpp` の `CXXPointer<T>` は `ScopedPointer` を介して `delete` を呼ぶ。`app/base/glib-cxx-utils.hpp` の `Object<T>` は `g_object_unref` によって保持参照を解放し、`Value<IsOwner,IsManager>` は `g_value_unset` と必要に応じて `g_free` を行う。`hold(T*)` は渡された参照を `Object<T>` に格納する。個々の値が新規参照か借用参照か、raw pointer の寿命、コピーと移動、例外経路を後続の `01.007` 子タスクで確定する。

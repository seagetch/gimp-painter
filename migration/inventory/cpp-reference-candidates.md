# 01.007 旧 C++ 参照管理の候補

旧版 71 C++ ファイルで、`hold(...)`、`GValue` と操作関数、`g_object_ref*` / `g_object_unref`、`CXXPointer<T>` の構文候補 242 件を `cpp-reference-candidates.tsv` に抽出した。1 行が複数分類に該当するときは複数行に分ける。`GValue` の型宣言や helper 定義も含み、未使用・コメント内・所有権なしの候補はまだ除外していない。

旧 `app/base/scopeguard.hpp` の `CXXPointer<T>` は `ScopedPointer` を介して `delete` を呼ぶ。`app/base/glib-cxx-utils.hpp` の `Object<T>` は `g_object_unref` によって保持参照を解放し、`Value<IsOwner,IsManager>` は `g_value_unset` と必要に応じて `g_free` を行う。`hold(T*)` は渡された参照を `Object<T>` に格納する。個々の値が新規参照か借用参照か、raw pointer の寿命、コピーと移動、例外経路を後続の `01.007` 子タスクで確定する。

`cpp-ref-hold-review.tsv` は明示的な `hold` と `g_object_ref*` / `g_object_unref` の 52 件を構文上の役割に分ける。`app/widgets/gimplayerpopup.cpp:927` は `gtk_ui_manager_get_widget` の返り値を参照追加なしで `hold` に渡すため、所有権を確認すべき箇所である。GTK 3 の menu 構築時には、新規参照を得るか借用として保持するかを明示し、破棄順も検証する。これ以外の `TRANSFER_CONTRACT_REVIEW` も各関数の返り値契約を調べるまで確定扱いにしない。

`cpp-scoped-pointer-review.tsv` は `CXXPointer` 54 箇所を対象に、対象の C++ 型と `delete` の対象を整理する。38 箇所の `Connection` は destructor が対象 GObject の signal を解除するため、Connection が対象より長生きしないことを実装時に確認する。3 箇所の Idle/Timeout は source の取消と callback の終了順を別途検証する。`GdkPoint` は `new` で生成されるため `delete` と対応する。一覧の `DONE` は構文分類の完了を示し、実行時の安全性を意味しない。

`cpp-gvalue-review.tsv` は `GValue` 136 件の構文を helper、配列借用、初期化、引数、値ポインターに分けた。旧 `app/pdb/pdb-cxx-utils.hpp:177-181` と `:263-267` は文字列の一時 `GValue` を初期化しているが `g_value_unset` がない。`app/core/gimpfilterlayer.cpp:665-674` は `GValueArray` の要素を `GArray` に byte copy して clear callback を設定する一方、元配列の解放が見えず、同一内部ポインターの二重解放とリークの危険がある。`get_procedure_arg` は runner の戻り値を返さずに破棄する。移植先では値ごとに `g_value_copy` を行い、元配列と一時値を個別に解放し、getter の戻り値を検証する。

旧差分に含まれる C 220 ファイルで `g_object_ref*` / `g_object_unref` と `g_value_*` の呼び出しを抽出すると 645 件あり、そのうち 74 件が変更 hunk の範囲に位置する。`c-reference-candidates.tsv` に操作、出典行、hunk ID を記録した。hunk 範囲内であることは新規行という証明ではない。後続では変更前の行との照合、返り値の参照数、解放との対応を確認する。

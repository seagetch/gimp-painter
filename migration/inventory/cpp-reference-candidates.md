# 01.007 旧 C++ 参照管理の候補

旧版 71 C++ ファイルで、`hold(...)`、`GValue` と操作関数、`g_object_ref*` / `g_object_unref`、`CXXPointer<T>` の構文候補 242 件を `cpp-reference-candidates.tsv` に抽出した。1 行が複数分類に該当するときは複数行に分ける。`GValue` の型宣言や helper 定義も含み、未使用・コメント内・所有権なしの候補はまだ除外していない。

旧 `app/base/scopeguard.hpp` の `CXXPointer<T>` は `ScopedPointer` を介して `delete` を呼ぶ。`app/base/glib-cxx-utils.hpp` の `Object<T>` は `g_object_unref` によって保持参照を解放し、`Value<IsOwner,IsManager>` は `g_value_unset` と必要に応じて `g_free` を行う。`hold(T*)` は渡された参照を `Object<T>` に格納する。個々の値が新規参照か借用参照か、raw pointer の寿命、コピーと移動、例外経路を後続の `01.007` 子タスクで確定する。

`cpp-ref-hold-review.tsv` は明示的な `hold` と `g_object_ref*` / `g_object_unref` の 52 件を構文上の役割に分ける。`app/widgets/gimplayerpopup.cpp:927` は `gtk_ui_manager_get_widget` の返り値を参照追加なしで `hold` に渡すため、所有権を確認すべき箇所である。GTK 3 の menu 構築時には、新規参照を得るか借用として保持するかを明示し、破棄順も検証する。これ以外の `TRANSFER_CONTRACT_REVIEW` も各関数の返り値契約を調べるまで確定扱いにしない。

`cpp-scoped-pointer-review.tsv` は `CXXPointer` 54 箇所を対象に、対象の C++ 型と `delete` の対象を整理する。38 箇所の `Connection` は destructor が対象 GObject の signal を解除するため、Connection が対象より長生きしないことを実装時に確認する。3 箇所の Idle/Timeout は source の取消と callback の終了順を別途検証する。`GdkPoint` は `new` で生成されるため `delete` と対応する。一覧の `DONE` は構文分類の完了を示し、実行時の安全性を意味しない。

`cpp-gvalue-review.tsv` は `GValue` 136 件の構文を helper、配列借用、初期化、引数、値ポインターに分けた。旧 `app/pdb/pdb-cxx-utils.hpp:177-181` と `:263-267` は文字列の一時 `GValue` を初期化しているが `g_value_unset` がない。`app/core/gimpfilterlayer.cpp:665-674` は `GValueArray` の要素を `GArray` に byte copy して clear callback を設定する一方、元配列の解放が見えず、同一内部ポインターの二重解放とリークの危険がある。`get_procedure_arg` は runner の戻り値を返さずに破棄する。移植先では値ごとに `g_value_copy` を行い、元配列と一時値を個別に解放し、getter の戻り値を検証する。

旧差分に含まれる C 220 ファイルで `g_object_ref*` / `g_object_unref` と `g_value_*` の呼び出しを抽出すると 645 件あり、そのうち 74 件が変更 hunk の範囲に位置する。`c-reference-candidates.tsv` に操作、出典行、hunk ID を記録した。hunk 範囲内であることは新規行という証明ではない。後続では変更前の行との照合、返り値の参照数、解放との対応を確認する。

`c-reference-hunk-review.tsv` はこの 74 件を参照追加・floating 参照の sink・解放・GValue 初期化・object 格納に分ける。個別の出典を見ると、`app/paint/gimpbrushcore.c:982-987` は texture の旧参照を解放して新参照を取得する。`app/xcf/xcf-load.c:1028-1029` と `:1089-1090` は置換する layer に対して `ref_sink` と `unref` を続けて呼ぶため、floating 状態に依存する処理である。`xcf_load_filter_specs` はループごとに `GValue*` を確保し、配列への値コピー後のポインターと文字列の解放が確認できない。保存形式を移す際は `12.003/filter-args-ownership` の成功・失敗時の cleanup に含める。

`cpp-raw-lifetime-candidates.tsv` に旧 C++ の `new`、`delete`、`g_free`、weak pointer の 168 構文候補を列挙した。`GimpImageFeature` は destructor で weak pointer を解除するが、`MypaintOptionsPropertyGUIPrivate` は `options` と `widget` に weak pointer を登録し、destructor では解除していない。この helper が対象より先に消えると、GObject finalize 時に解放済みメモリーへ NULL を書く可能性がある。移植先で弱参照の登録解除と signal 接続の解除を同一 owner の終了処理に含める。

`c-reference-added-line-review.tsv` は 21 ファイルの変更前・変更後 blob の差分を再計算し、hunk に位置した 74 件すべてが実際に追加された行だと照合した。内訳は `unref` 41、`ref` 21、`ref_sink` 3、`g_value_init` 5、`g_value_set_object` 4。確認した経路のうち brush core の texture は setter の旧参照を解放して新参照を保持する。brush options GUI private は container/context の参照を取得し、destroy/reset で解除する。dock widget の付け替えでは一時参照を取得して container から取り外し、再追加後に解除する。ただし追加行であることと全経路の参照数が釣り合うことは別であり、残るファイルの分岐経路は `01.007/c-state` で追う。

`cpp-weak-pointer-review.tsv` は四つの登録先とスロットを個別に追う。静的な standard brush のスロットは process の存続中有効で、`GimpImageFeature.drawable` は destructor が対象存続時に登録解除する。`MypaintOptionsPropertyGUIPrivate` の二つのスロットは helper のメンバーだが、旧 destructor に解除がない。特に `options` が widget より長生きして widget が helper を破棄した場合、後の options finalize が無効なスロットに書き込む可能性がある。先行破棄の両順序を `08.008/weak-pointer-teardown` の実装テストに含める。

raw allocation の個別追跡では `mypaintbrush-mapping.hpp` の `new[]` と `delete` の不一致を確認した。詳細と GIMP 3 の値所有モデルは `mapping-array-ownership.md` に記録した。

`c-reference-owner-review.tsv` は追加された C 参照操作 74 行を 21 ファイルの所有者・解放契約へ対応付ける。`gimp_image_undo_push` の追加 `unref` は undo 登録後ではなく編集不可 drawable を拒否する早期 return にあり、`gimp_image_set_perspective_guide` は旧値を unref して新値を ref なしで格納する。`gimpplugin-message.c` の二箇所は不正な引数名・戻り値名で手続きを登録せずに返る失敗経路である。`gimpuiconfigurer.c` の画像 shell 移動は一時 ref を解除するが、空 display の分岐は shell を取り外した後にその参照を解除しない。`08.009/guide-owner` と `30.005/shell-reparent-ref` に移植先での修正と反復 teardown を追加した。表の `DONE` は旧差分の静的な契約追跡であり、GIMP 3 の実装・動作検証を示さない。

`cpp-raw-lifetime-review.tsv` は 168 候補をコメントと文字列を除いて分類した。実際の構文は単体 `new` 81、配列 `new[]` 2、`delete` 39、`g_free` 34、weak pointer 登録 4 で、8 件はコメントまたは文字列である。旧 `gimpmypainttool.cpp:204` の `delete core` はコメント内なので、`paint-core` data-full の破棄と重複する active 解放経路ではない。確保先の保有者や条件分岐ごとの解放は引き続き `01.007/cpp-ownership` で判定する。

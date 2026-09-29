# 旧 C++ の GObject data/qdata 候補

固定した旧版 `afa43fae3e920210146abed514f136fd49f671b5` の `app` 以下の C++ 71 ファイルを対象に、`tools/audit_cpp_object_data_sites.py` が呼び出し式を分解する。各キー、対象オブジェクト、格納値、destroy callback と出典行は `cpp-object-data-sites.tsv` に記録する。これは `01.006` の第一段階であり、全 C ソースの状態付加や呼び出し先の所有権確定は含まない。

| 構文上の分類 | 件数 | 破棄または参照の契約 |
| --- | ---: | --- |
| `g_object_set_cxx_object` 定義・委譲 | 4 | `set_data_full` / `set_qdata_full` と `destroy_cxx_object_callback<T>` に委譲し、格納された `T*` を破棄する |
| 所有権を持つ付加 | 10 | 9 箇所が上記 helper、1 箇所が `gimp_editor_button_extended_actions_free` を登録する |
| destroy callback なしの付加 | 9 | 3 種類の tool_info に可視フラグと GUI 関数ポインターを格納する |
| 借用参照 | 6 | `get_data` または `get_qdata`。参照先の寿命を延長しない |
| 所有権を取り出す参照 | 1 | `undecorate` が `steal_data` の返り値を `delete` する |

`ComplexBindAction::bind_to_full` は `this` を対象の GObject と signal 名に由来するキーへ `set_cxx_object` で渡す。同じインスタンスが複数の格納先に登録される場合の二重破棄と、signal closure が参照する寿命を後続で検証する。`gimpmypainttool.cpp:188` は tool GObject が `paint-core` の C++ オブジェクトを所有する構造なので、core と tool の finalize 順序も検証する。`Decorator` は widget 側に所有権を置き、明示解除では steal 後に一度だけ delete する設計である。これらは旧版の契約の記録であり、GIMP 3 での安全性をまだ示さない。

所有権を持つ 10 箇所の具体的な型、owner、破棄 callback、別の参照経路は `cpp-owning-data-review.tsv` に対応付けた。特に `CurveViewActions` は tree_view が所有する一方で tree selection、toggle、options、adjustment に raw `this` を登録し、空の destructor ではそれらを切断しない。`ComplexBindAction` の保持は対象 GObject の data による一回の delete を想定しているが、`PageRemindAction::bind_to` は signal 登録のみで data に格納しない。後続の `01.008` と機能タスクで接続ごとの解除・多重登録・破棄順を実装し、破棄中の callback 発火を検証する。

旧差分に含まれる C 220 ファイルについて、`c-object-data-candidates.tsv` に 185 件の `data/qdata` 呼び出し式を出典行・キー・格納値・destroy callback とともに抽出した。ファイル単位の差分であり、この 185 件すべてが追加コードという意味ではない。後続では追加行と上流由来を照合し、GIMP 3 側の対応箇所、所有者と破棄経路を確定する。

`c-object-data-delta-review.tsv` は変更 hunk 内の 19 件を切り出す。`gimp-tools.c` の toolbar widget は `g_object_ref_sink` 後に tool options の data-full に渡し、`g_object_unref` を破棄 callback に登録する。tool info の横向き GUI 関数は借用する関数ポインターである。新規 `gimpbrushoptions-gui.c` の property 名は静的文字列、`gimptooloptions-gui.c` の percentage と digits はポインターに変換した整数値で、いずれも delete しない。toolbar 側の参照は tool options に保持された widget の借用である。signal の解除と GTK widget の finalize 順は `01.008` で追跡する。

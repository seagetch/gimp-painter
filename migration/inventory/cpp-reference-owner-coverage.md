# 01.007 C++ 参照と raw pointer の全件対応

旧 tree `afa43fae` の参照構文242件を `hold` 24、明示 ref/unref 28、`CXXPointer` 54、`GValue` 136へ重複なく分割した。各行に取得元または owner、破棄契約、移植先 WBS を記録した。raw lifetime 168候補も所有者・解放契約へ対応済み。`python3 -B tools/audit_cpp_reference_owner_coverage.py` は候補台帳と四つの所有台帳の集合一致、および raw 168行の契約記載を検査する。

`cpp-hold-owner-contracts.tsv` は `hold` の採用helper7件、新規生成5件、返却値12件を含む。`g_array_new` は初期参照を `Array` が採用し、`gimp_channel_new_mask` と contiguous region の結果は `Object<GimpChannel>` が解放する。helper 自体は汎用の所有権契約であり、`hold(GValue&)` に stack 値、`hold(T*)` に非 GObject を渡してよい保証はない。旧 preset applier と借用 menu の問題は別の実装タスクに追跡した。

この区分の DONE は旧版の候補を取得・借用・解放・例外時の危険へ対応付けた**静的棚卸し**を示す。`GValue` の浅いコピー、`CXXPointer` の callback 破棄順、raw pointer の失敗経路など、旧不備の解消は追跡先の機能タスクで実装・試験する。GIMP 3 の所有権が実行時に成立したという判定ではない。

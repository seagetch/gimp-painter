# 01.008 signal と idle/timeout の着手

`signal-source-inventory.tsv` は旧 C++ の明示 signal 登録60箇所、C++ wrapper 接続47箇所、既知の deferred source 4箇所を、接続先・callback・user data・解除・優先度・再入の観点でまとめた111行の着手台帳。各行は旧コードの構文から確認できる範囲を記録し、`REVIEW` は個別の接続先と破棄順の検証が残ることを示す。`python3 -B tools/audit_signal_source_inventory.py` で対象集合と行数を検査する。

107 signal のうち15件は直接 helper の返却 Connection を保持し、22件は wrapper の `connect` の返却を保持する。34件は `g_signal_connect_delegator(...)` の返却 Connection を保存せず、25件は `connect_noret` なので切断用 handle を持たない。残る11件は C 形式の callback を接続し、明示的な handler ID は保存しない。特に前者34件は Connection allocation 自体を失う。callback の `this` が emitter より先に破棄される順序を `07.008/untracked-signals` に追跡した。

FilterLayer の300ms timeout は ID を捨て、破棄後の raw `this` に callback が入る危険がある。LayerPreview idle は `G_PRIORITY_DEFAULT` で `this` と `viewable` を生で捕捉する。tile の scroll timeout と長押し timeout も `G_PRIORITY_DEFAULT` で、後者は callback 中に自分の CXXPointer を消す経路がある。個別の追跡先は台帳に記録した。

これは C++ 側で既に抽出済みの候補を結合した第一段階。旧差分の C 側にある signal/source、汎用 helper から生成される実接続、動的 signal 名、先行破棄と再入の実経路を続けて照合する。`01.008` の完了判定はまだ行わない。

旧差分対象の C 220ファイルを追加で走査し、`c-signal-source-candidates.tsv` に517構文候補（signal 460、source登録19、source解除38）を列挙した。このうち57件は変更hunk内の signal 接続で、`c-signal-added-line-review.tsv` は57件すべてが実際の追加行であることを前後 blob の差分で照合した。残る460件は変更対象ファイルの旧文脈を含む。次に追加57件の user data、接続先先行破棄、再入を個別に追跡する。

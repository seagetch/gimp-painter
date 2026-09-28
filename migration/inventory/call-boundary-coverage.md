# 01.005 候補分類の全件照合

対象は旧ソース `afa43fae3e920210146abed514f136fd49f671b5` の
`app` 以下 71 C++ ファイルから抽出した一次候補と、同じ抽出器が検出した
C / ヘッダーの参照である。実行方法:

```sh
python3 -B tools/check_call_boundary_coverage.py
python3 tools/check_tasks.py
```

| 一次候補 | 件数 | 対照した分類表 |
|---|---:|---|
| extern C scope | 69 | `extern-c-block-review.tsv` |
| 関数ポインター構文 | 34 | `function-pointer-review.tsv` |
| delegator | 107 | `delegator-site-review.tsv` |
| callback 登録 | 94 | `callback-registration-review.tsv` |
| class への代入 | 63 | `vfunc-assignment-review.tsv` |
| vfunc binding | 92 | `binder-site-review.tsv` |
| C 入口候補 | 28 | `c-entry-candidate-review.tsv` |
| C++ の名前付き定義 | 149 | `cpp-definition-review.tsv` |
| signal wrapper | 47 | `signal-wrapper-review.tsv` |
| **計** | **683** | **全件に同じ種類・ファイル・行の DONE 分類がある** |

C / ヘッダー参照 130 件も `c-reference-review.tsv` に一対一に対応する。
直接呼出し 41 件のうち有効な C→C++ 33 件は XCF 9 件とその他
24 件に重複なく分割され、C 側の関数ポインター登録 6 件も保持箇所と
利用先を記録済み。別走査のマクロ生成 GType 6 件、property accessor
44 出現、遅延 source 2 経路も各分類表の DONE と数を検査する。

追加の逆引きで旧 app の 2,051 ソース・ヘッダーを確認し、一次抽出に
現れなかった C 入口 4 件と static callback 1 件を別表に補った。
間接関数ポインター 34 候補は `indirect-callback-owner-review.tsv` に
保持者・呼出し・解放とともに記録した。Binder 71 件と直接 class callback
40 件の現行 slot 対照は `gimp3-vfunc-contract-summary.md` に集約する。
一次候補表の REVIEW は分類済み表の DONE と矛盾しない。

正規表現抽出と逆引きは静的な棚卸しであり、実行時に登録される
全 callback の安全性を証明するものではない。移植後の C ABI 例外封じ込め、
所有者・破棄順と描画結果は、各機能タスクで実装と試験を行う。

# 01.012 上流実装との照合

旧基点 `a61915a8e62aad8855cd8c620bdb7195a25ffb90` から移植元
`afa43fae3e920210146abed514f136fd49f671b5` への全 **2,489 hunk／binary section** を、
公開版 `6ac8031c9555667e080fef0324fca9f28a02e5ab` と初期移植先
`95f6410f25c5186686db7a489d79c1e79187cd41` の双方に対照した。
台帳の DONE は照合の完了であり、旧機能の移植や動作互換性の合格ではない。

## 全件子チェック

`upstream-hunk-comparison.tsv` の 4,978 行は元の hunk ID と一対一（比較先ごと）に対応する。
元／先の blob、追加・削除 payload の SHA-256、固定比較 commit、照合結果を保持する。
同じ path の全ファイル hash と、空白・空行のみ正規化した追加／削除の連続行を比較する。
連続行の一致位置は **空行を除いた正規化テキストの行番号** であり、元ファイルの行番号ではない。
token、コメント、行の順序は変更しない。

| ソース対照結果 | 2比較先の合計 | 読み方 |
|---|---:|---|
| IDENTICAL_FILE | 2 | `app/core/gimptoolgroup.h` の blob が両比較先で一致 |
| ADDED_TEXT_PRESENT | 414 | 追加行が同じ path にある。短い宣言・括弧・コメントも含む一次候補 |
| ADDED_TEXT_NOT_FOUND | 2,788 | 同じ path に追加行の全連続列がない |
| DELETION_ONLY_REQUIRES_REVIEW | 140 | 削除のみ／非空の追加行なし。削除後の意味は別途確認 |
| PATH_ABSENT | 1,634 | 同じ path なし。rename・外部依存への移動・機能不在を区別する必要あり |

**414 件を上流取込み済みとは判定しない。** 別位置で削除行が残っているかも別列に記録する。
追加行だけの一致から制御フロー、利用先、設定、寿命、実行結果の一致は導けない。
path 不在を機能不在とも判定しない。全件を 01.013 の割当対象に残し、
この照合結果だけで旧コードを削除・互換処理を省略することを禁止する。

## 実装・動作契約の子チェック

`upstream-contracts.json` は機能36項目を19の比較契約に束ねた手書き正本。
`upstream-contract-review.tsv` の38行は旧／現行の固定ソース行と追跡 WBS を対応させる。
同名の代替候補について MyPaint、Smudge、回転、bucket fill、layer effects、tool groups
を確認し、残る独自型・UI・資源・HTTP・placeholder・C++基盤も区別した。
path 不在の出典は same-path 照合であることを明記している。

- ToolGroup はヘッダー全体が一致するが、実装ファイル全体は一致しない。
  tool editor 等の共通行も操作／保存の互換性の証明にはならない。30.001/30.002/30.017 で検証する。
- 標準 MyPaint は `GimpMybrush` と外部 libmypaint を使う。旧設定エンジン、形状採色、
  texture、floating stroke、旧 myb reader/writer、詳細 editor の代替合格にはしない。
- 標準 Smudge は Flow と浮動小数 GEGL の混合処理を使う。旧 `use-color-blending`、
  `GIMP_DYNAMICS_OUTPUT_BLENDING`、`shade_region` と8bit丸めの経路とは実装が異なる。
  同じ蓄積式がコメントにあることだけで旧筆跡の一致とはしない。
- 現行 drawable filter と旧 FilterLayer の型・保存・scheduler は異なる。
  現行の非破壊効果への置換は設計契約に反する。CloneLayer の名前による参照解決も保持対象。
- 回転／微小移動／XCF 番号には下記の実行可能な反例がある。旧差分を取り込み済みとして省略できない。
- 旧塗りつぶしブラシは投影参照を使う独立した tool。標準 bucket fill の全域探索と
  選択合成を同等とは認定しない。選択外経由の探索は 01.016 の既存子で追う。

## 切り出した式による動作証跡

`audit_upstream_contracts.py` は旧／現行の **実ソースから式を抽出して C99 でコンパイル** する。
両固定比較先について以下を実行する。GIMP 全体、GTK入力、筆跡、XCF往復の実行試験ではない。

| 入力／条件 | 旧処理 | 現行処理 | 判定 |
|---|---|---|---|
| snap角7.5°、FE_TONEAREST、HAVE_RINT の式 | 15° | 0° | tie境界で異なる |
| snap角37.5°、同上 | 45° | 30° | tie境界で異なる |
| snap角7.499°／7.501°、同上 | 0°／15° | 0°／15° | 境界の両側の対照 |
| RINT の floor(x+.5) fallback、7.5°／37.5° | 15°／45° | 15°／45° | fallbackでは上記tie反例は成立しない |
| dx=dy=0、filter=.1、筆圧 .2→.8 | 捨てない | 捨てる | pressureの独自条件が現行にはない |
| 同条件、筆圧変化なし | 捨てる | 捨てる | pressure反例の対照 |
| XCF 属性32／33 | Filter spec／Clone spec | lock position／float opacity | enum値を実ソースから評価し衝突を確認 |

RINT はプラットフォーム条件で実装が変わるため、丸め差を全ビルドに一般化しない。
Ctrl の押下・解除、scroll action、鏡映、累積角の状態遷移は 25 章で実機 replay が必要。
全機能の動作同等性は今回認定しておらず、残る試験を各 contract 行の followup に明示する。
02 章の旧実行環境／fixture と機能別実装・比較試験の完了を先取りしない。

## 再検査

```sh
python3 -B tools/inventory_upstream_hunks.py --check
python3 -B tools/audit_upstream_contracts.py --check
python3 -B tools/check_tasks.py
```

`--check` は台帳を書き換えず再計算結果を比較する。親01.012の完了時には
`check_tasks.py` も全 hunk ×2比較先の集合、子IDの一意性、固定commit、
契約子チェックの欠落、および動作同等性の過剰な認定を検査する。

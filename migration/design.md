# gimp-painter → GIMP 3.0 移植方針

更新日：2026-09-29

本書は現行の移植方針をまとめた設計文書である。独自機能の調査はソースの静的解析までであり、移植実装、旧作品の読込み試験、筆跡比較、応答時間測定は未実施。GIMP 3 基準版のビルドと標準描画の確認は完了している（`migration/baseline/`）。必須要件と、確認済みの旧実装、移植先への設計案を区別する。

## 1. 最優先の要件

1. **旧版 gimp-painter で正常に開けるファイルを、移植版でも開けること。** 別アプリでの事前変換を通常利用の前提にしない。旧形式の識別・解釈は移植版の読込み経路に組み込む。
2. **ファイルへ保存された情報と再編集可能性を維持すること。** レイヤー型、階層、参照、処理名・引数、合成モード、マスク、画素、関連メタデータを対象とする。画像への焼き込みだけでは互換性を満たさない。
3. **CloneLayer の旧仕様を維持すること。** 同一画像内の参照、更新追従、位置・サイズ・階層・合成・複製等の挙動は旧 reader と旧実装を基準にする。
4. **FilterLayer の自動更新を非ブロッキングで維持すること。** 下層の変更検知、依存順序、実行可否のチェックポイント、停止要求、結果反映、更新伝播、終了条件を移植対象とする。
5. **GEGL の既存非破壊フィルター／レイヤー効果へ独自レイヤーを統合・置換しない。** GIMP 3 の画素格納基盤である GeglBuffer 等の利用とは別の判断である。
6. **拡張 MyPaint の描画能力を維持すること。** 設定評価、押印、採色、混色状態、非累積合成、保存を一体で扱う。標準 MyPaint や独立した Smudge ツールの存在を、代替できる根拠にしない。
7. **描画操作の差分を維持すること。** 回転スナップ等は、機能名ではなく修飾キーと状態遷移を比較する。
8. **機能整理は互換性を満たす範囲で行うこと。** 保存済みの情報を失わせる削減をしない。重複実装と周辺機能の整理は、互換性の確保より後に置く。

「開ける」「見た目が合う」「編集が継続できる」「再保存後にも同じ動作が戻る」は別々の合格条件であり、すべて必要である。

## 2. 比較対象と差分規模

| 用途 | 参照 |
|---|---|
| 移植元 | seagetch/gimp-painter、gimp-2-8、afa43fae3e920210146abed514f136fd49f671b5 |
| 独自差分の抽出基点 | GNOME/gimp、gimp-2-8、a61915a8e62aad8855cd8c620bdb7195a25ffb90。移植元 HEAD の祖先 |
| GIMP 3.0 公開版の比較基準 | GIMP_3_0_8、6ac8031c9555667e080fef0324fca9f28a02e5ab |
| 移植先ブランチの確認済み HEAD | GNOME/gimp、gimp-3-0、95f6410f25c5186686db7a489d79c1e79187cd41 |

移植先は指定された GIMP 3.0 系とする。ブランチ上の Meson の版番号 3.0.9 は公開版の存在を意味しない。

上流 2.8 基点から移植元 HEAD までの差分は 947 ファイル、95,501 行追加、2,267 行削除。C/C++ の .c/.h/.cpp/.hpp は 519 ファイル、48,741 行追加、2,219 行削除。data/mypaint-brushes は 374 ファイル、45,274 行追加である。これらは GIMP 3 との世代差をすべて数えた値ではない。

同梱 .myb は 177 個で、ファイル内 version は v3 が 176 個、v2 が 1 個。資産と実行コードは別々に棚卸しする。全保存項目の監査が完了したという意味ではない。

## 3. 移植方針テーブル

P0 は旧ファイル・資産の読込み、意味、再編集、実行を維持する必須項目。P1 は旧制作操作を維持する項目。P2 は互換性を確保した後に整理する項目。P0 内には依存順序があり、表の行番号をそのまま実装順とはしない。

| 優先度 | 対象 | 方針 | 実装・合格条件 |
|---|---|---|---|
| P0 | 旧 XCF の識別と読込み | 必須維持 | 独自 v4、属性番号、合成モードを識別して読めるようにする。通常の「開く」から利用できること |
| P0 | 保存情報の往復 | 必須維持 | 読込→編集→保存→再読込で型・値・階層・参照・動作を保持。画素への焼き込みで代替しない |
| P0 | CloneLayer | 旧仕様を維持して移植 | 参照元、ライブ更新、位置・サイズ・合成・階層・複製・Undo を旧実装と比較する |
| P0 | CloneLayer の参照保存 | 旧参照解決を維持 | 旧 source name の解決結果を再現。安定 ID の導入は内部表現の案であり、旧動作を変更する理由にしない |
| P0 | FilterLayer のデータモデル | 独自レイヤーとして維持 | procedure 名、保存済み引数の型・順序・値、下層への作用範囲、位置・合成を保持 |
| P0 | FilterLayer の実行制御 | 独立した制御として移植 | 編集を止めず、自動更新、依存先優先、実行開始条件、キャンセル、結果反映、終了条件を維持 |
| P0 | FilterLayer の処理実行 | 旧処理の意味を維持 | 現行プラグイン API 等に合わせた実行器を設計する。GEGL の既存非破壊効果には置換しない |
| P0 | 独自合成モード | 演算を維持 | SRC/DST IN/OUT、Erase/Replace/Anti-Erase 等を明示的に変換・実装。数値や名称の一致だけで置換しない |
| P0 | 複合レイヤー構造 | 構造と動作を維持 | グループ・順序・マスク・参照・合成・位置・不透明度を一体で検証 |
| P0 | アニメ塗り構成 | 維持 | CloneLayer＋dst-in、乗算の shadow、元レイヤー、グループを再編集可能なまま復元 |
| P0 | 水彩構成 | 維持 | plug-in-edge の FilterLayer、difference、不透明度、元レイヤー、グループを復元 |
| P0 | その他の XCF 情報 | 保存項目台帳で確認 | 通常レイヤー、テキスト、チャンネル、パス、マスク、ロック、parasite 等も旧 reader/writer と照合し、取りこぼさない |
| P0 | 拡張 MyPaint エンジン | 独自機能を維持 | 現行 libmypaint への拡張移植を第一候補とし、必要なら派生版を保持。旧拡張エンジン移植案とも再現性で比較 |
| P0 | GIMP ブラシ形状 | 押印・採色とも維持 | 形状マスク、サイズ、角度、変形を押印と採色の両経路に接続 |
| P0 | 紙目・テクスチャ | 維持 | 通常ブラシと MyPaint の処理・資源管理を共通化できる部分を整理し、紙目の座標系・位相・合成順を保持 |
| P0 | 動的紙目パラメーター | 維持 | texture_grain／texture_contrast の入力カーブ評価を押印ごとに Surface へ渡す |
| P0 | MyPaint 内の拡張 Smudge | 維持 | 形状を反映した採色、直前の有効な採色結果の保持、混色状態の更新を移植。独立 Smudge ツールでは代替しない |
| P0 | 非累積描画 | 維持 | 開始前画像とストローク用バッファ、採色元、合成順、確定と Undo を再現 |
| P0 | ストローク不透明度 | 維持 | stroke_opacity を保持し、押印単位の opacity への単純置換をしない |
| P0 | 拡張ブラシ保存・詳細編集 | 維持・共通化 | 独自設定、入力カーブ、ブラシ／紙目参照を保持。設定モデルと editor を共有し、保存後に再編集できること |
| P0 | ブラシ履歴・プレビュー | 維持・共通化 | 保存資源と編集中状態を区別し、独自設定を反映したプレビューを作る |
| P0 | レイヤープリセット資産 | 内容と生成結果を維持 | 標準型だけの定義も独自型を使う定義も保持。外出しは必要な API が整った場合の実装配置として判断 |
| P1 | 独立 Smudge の色混合拡張 | 同等性を確認して実装 | 旧 blending-output、蓄積バッファ、サイズ変化、標準 Flow／Rate を比較。同等性未確認のまま統合しない |
| P1 | 手ぶれ補正 | 旧操作を比較して移植 | 標準処理の再利用は入力履歴・補正順序・定規併用で差がない範囲に限る |
| P1 | 微小移動時の筆圧 | 維持 | 小さな座標変化や位置固定時にも筆圧変化を失わないこと |
| P1 | 回転・反転 | 表示基盤を利用し操作を移植 | 開始条件、角度更新、反転中の方向、修飾キーの状態遷移を検証 |
| P1 | 回転スナップ | 独立差分として移植 | 15°刻みだけでなく、Ctrl の押下・解除、追加キー、丸め境界を比較 |
| P1 | パース定規 | 維持・再実装 | 最大 3 消失点、方向決定と筆跡拘束、表示変換、入力処理順を維持 |
| P1 | 塗りつぶしブラシ | 維持を前提に移植仕様化 | ストローク開始時の参照画像、ブラシ境界内の領域探索、塗り／消去を再現 |
| P1 | 選択境界を使う塗りつぶし | 独立差分として維持 | 領域探索自体が境界外へ出ない処理を扱う。探索後に切り取るだけでは同等としない |
| P1 | キャンバス UI | 維持・GTK3 化 | レイヤータイル、色、ツール、縦長配置、表示退避、入力透過を移植。現行の複数選択等にも対応 |
| P1 | ツールグループ・操作バー | 挙動を維持し共有化 | 標準状態モデルと既存アクションを使う。旧操作を失わせる簡略化はしない |
| P2 | editor／横型 GUI の重複 | 整理 | モデルと部品を共有し、ドック・ポップアップ・コンパクト表示の二重保守を減らす |
| P2 | HTTP／REST 連携 | 任意機能へ分離する案 | 利用機能を確認し、プラグイン／外部連携へ移す範囲を決める |
| P2 | ImageGenerator placeholder | 初期移植から除外する案 | 実装済みの生成機能として扱わない |
| P2 | 旧基盤・補助コード | 現行基盤へ置換 | GTK3、Meson、GeglBuffer、現行 Undo を使用。旧ラッパー等の整理で保存・描画仕様を変更しない |

## 4. 保存互換性と独自レイヤー

XCF writer に独自属性として確認できる追加レイヤー型は FilterLayer と CloneLayer である。アニメ塗り・水彩等は、これらと通常レイヤー、グループ、合成モードを組み合わせた保存構造であり、独立した C++ 型の数だけで維持対象を判断しない。

| 保存内容 | 旧ファイルの表現 | 維持方法 |
|---|---|---|
| FilterLayer | PROP_FILTER_SPEC、procedure 名、引数列 | 独自レイヤーと処理定義を復元し、下層変更への自動更新を維持 |
| CloneLayer | PROP_CLONE_SPEC、source name | 旧解決規則で参照を復元し、保存後も参照動作を維持 |
| グループ内の構成 | 子レイヤーの順序・型・モード等 | 参照と合成の関係まで復元 |
| 通常のレイヤー属性 | 名前、画素、位置、サイズ、不透明度、可視性、マスク等 | 独自レイヤーへの復元時にも属性を落とさない |
| 拡張 .myb | settings、switches、texts、関連メタデータ | 独自値と未知項目を黙って捨てず、描画・編集・保存へ接続 |
| プリセット JSON | レイヤー構成、処理と引数、参照、モード | 定義と生成される構造を維持 |

旧 XCF と標準 XCF には次の衝突がある。

| 項目 | gimp-painter | GIMP 3.0 |
|---|---|---|
| v4 ヘッダー | base type の後に従来の属性列 | version 4 以上では precision がある |
| 属性番号 32 | FilterLayer の仕様 | 位置ロック |
| 属性番号 33 | CloneLayer の仕様 | 浮動小数点の不透明度 |
| モード 23 | Erase | Overlay |
| モード 24 | Replace | LCh Hue |
| モード 25 | Anti-Erase | LCh Chroma |
| モード 26 | src-in | LCh Color |
| モード 27 | dst-in | LCh Lightness |
| モード 28 | src-out | Normal |
| モード 29 | dst-out | Behind |

右列は変換先ではなく、同じ数値をそのまま読んだ場合の衝突を示す。v004 という文字だけで識別せず、実際のヘッダー・属性構造と旧 reader の挙動を基に読込み経路を選ぶ。形式判定を誤って旧ファイルが開けなくなる設計は不可。

新しい保存表現は、衝突しない識別と版番号を持つ拡張メタデータ等を設計候補とする。現行標準 XCF を土台にする場合も、独自レイヤーの型・参照・処理定義を往復できることが採用条件。標準 GIMP で独自機能まで再編集できることを保証するという意味ではない。

CloneLayer の安定 ID 化は提案であり、旧仕様の変更ではない。旧版の名前解決を先に再現し、その結果を内部 ID に対応付ける。同名・改名・グループ複製・削除・Undo の挙動は旧版を基準にする。旧版でも未解決の参照は、元の記録を保持した状態で開けるよう扱い、勝手な別レイヤーへの接続や焼き込みをしない。

旧 FilterLayer writer には配列や drawable 参照等の未対応経路があり、文字列の型や INT16 の長さにも不整合が見える。これは旧ファイル対応を外す理由にせず、実際の writer/reader と fixture に合わせた互換処理の対象とする。旧ファイルに記録されていない情報まで復元できるとは主張しない。

保存項目台帳には、所有オブジェクト、保存先、フィールド名／番号、型、既定値、参照先、reader/writer、移植先、試験ケースを記録する。通常属性・parasite・資源設定も含めて追加監査する。

## 5. FilterLayer の実行制御

FilterLayer は、処理定義を保存するレイヤーであると同時に、依存する下層の変更に非ブロッキングで追従する実行制御を持つ。移植ではデータモデル・実行器・スケジューリング・結果反映を一体で扱う。

### 確認済みの旧実装

| 制御点 | コード上の処理 | 維持する意味 |
|---|---|---|
| 変更の影響判定 | on_stack_update() が上側の変更を除外 | 無関係な変更で再実行しない |
| 依存先優先 | 下側の可視 FilterLayer が未処理なら waiting_process_stack を立て、自分の runner に stop を要求 | 下層が確定する前に上層を評価しない |
| 更新の収集 | invalidate_area() がタイル単位の領域を登録し、同一領域の重複を抑える | 変更を収集し、無制限の重複登録を避ける |
| 入力準備 | project_region() が投影済み領域を消化する | 入力が準備できたか確認する |
| 開始条件 | projected_tiles_updated、下層待ちなし、未投影領域なし | 不完全な入力で処理を始めない |
| 起動予約 | runner->preserve() が実行中／予約済みを判定 | 多重起動を防ぐ |
| 非同期実行 | ProcedureRunner が PDBAsyncExecutor を使用 | 完了待ちで編集操作を止めない |
| 未完成結果の遮断 | 処理中・待機中は通常のレイヤー合成へ進まない | 未完成結果を上層へ確定入力として渡さない |
| 完了通知 | end() → runner->on_end() → notify_filter_end() | 実行状態を解除して更新を伝播する |
| 自己更新の除外 | 一度投影された自身の更新通知を除外 | 自動更新の循環を防ぐ |
| 初回読込み | loaded 状態の初回投影で保存済み画素を使用する経路 | 読込み時の表示・再評価の扱いを維持する |

旧 stop() はキャンセル要求であり、フィルター内部の計算状態を保存して任意位置から再開する機構と同一ではない。チェックポイントは、入力準備、実行開始、依存変更時の停止判断、完了後の反映判断を含む。

旧投影の idle 経路は優先度 150、256×128 の領域単位で処理する。一方、FilterLayer の通知には同期 flush やコピーもある。非同期 API の名前だけで UI 応答の保証が成立したとせず、移植では準備・コピー・反映を含めた実行時間を検証する。

### 移植先の制御契約と設計案

以下の世代番号、処理量制限、独立した結果バッファは移植先の設計案であり、旧コードに同じ形で存在するという主張ではない。旧動作との互換性を確認して採用する。

| 項目 | 契約 |
|---|---|
| 優先順位 | 入力・編集・表示の応答を確保し、FilterLayer 間では依存先から処理する |
| UI 側の仕事量 | 入力準備、コピー、結果反映にも実行量の上限を設け、長時間の同期処理を避ける |
| 入力の確定 | 処理対象の画素・設定・参照構造を、一貫した世代として扱う |
| 追加変更 | dirty 領域と最新世代を追跡する。古い計算結果を新しい状態として反映しない |
| 結果の所有 | 実行途中の書込みを確定結果から分離し、反映時点を明示する |
| 反映のチェックポイント | 入力世代、設定世代、依存構造、レイヤー生存状態を検証する |
| 停止・失敗 | 正常完了と区別し、予約・待機を解除。最新入力への再処理を失わず、無限再試行を防ぐ |
| 終了条件 | 対象世代の結果反映済み、未処理領域なし、依存待ちなし、実行中なし。更新が続く間の状態とは区別する |
| 保存 | 定義とキャッシュの世代を混同しない。処理中の保存でも再読込み後に正しい状態へ戻れるようにする |
| ライフサイクル | 削除・画像終了・Undo・設定変更後に古い callback が結果を書き戻さない |

実行器には現行プラグイン手順への接続や隔離された処理実行を検討するが、既存 GEGL 非破壊効果の評価経路に委ねない。旧フィルター処理の意味、停止時の振舞い、結果の受渡しを個別に仕様化する。

## 6. 拡張 MyPaint と Smudge

| 段階 | 維持する内容 |
|---|---|
| 設定の読込み | 標準値、独自値、入力カーブ、GIMP ブラシ・紙目参照 |
| 入力評価 | 筆圧、速度、方向、傾き等による旧設定の評価 |
| 押印 | GIMP ブラシ形状、紙目、硬さ、縦横比、角度 |
| 採色 | 硬さ・形状・向きを考慮した採色と、有効な直前色の保持 |
| 混色状態 | 旧エンジンの状態更新と採色・押印の順序 |
| 合成 | 非累積描画、ストローク不透明度、アルファ、採色元バッファ |
| 保存・編集 | 独自値を欠落させず再編集可能な形で保存 |

旧エンジンは押印ごとに texture_grain／texture_contrast を Surface へ渡す。標準の draw_dab インターフェースだけでは、この独自評価値の受渡しを満たさない。

旧 get_color は硬さ・縦横比・角度を受け取り、GIMP ブラシ形状での採色へ分岐する。GIMP 3.0 の標準 MyPaint Surface の採色は円形の重み付き採色で、コードの注記は硬さ 0.5、縦横比 1、角度 0 相当である。旧エンジンには採色結果の alpha が 0 の場合に直前の有効な採色結果を使う経路もある。押印マスクだけを移して同等とはしない。

現行 libmypaint への拡張移植と必要な Surface API 拡張を第一候補とする。再現性が不足する場合は旧拡張エンジンを現行基盤へ移植する案を比較する。標準エンジンへの一本化を前提にしない。

非累積描画は開始前画像、ストローク用画像、表示合成画像のどれから採色するかを含む。一筆を一つの Undo として扱い、押印ごとの画像全体コピーを避ける。

独立した GIMP Smudge ツールには Flow／Rate があるが、MyPaint 内の拡張 Smudge の代替にはならない。独立ツール側の旧 blending-output と現行 Flow の対応も、蓄積バッファとサイズ変化を含めて比較するまで同等としない。

## 7. 回転とその他の操作仕様

| 比較点 | 旧 gimp-painter | GIMP 3.0 の確認済み経路 | 移植方針 |
|---|---|---|---|
| スナップ刻み | 15° | 15° | 刻みだけで同等としない |
| Ctrl 判定 | イベントの Ctrl ビットを検査 | 回転／段階回転アクション。動的切替では修飾キー集合の一致を検査 | 旧キー操作を維持 |
| 角度更新 | 開始角度と現在位置から算出 | 前回位置との角度差を累積 | 境界とキー切替を比較 |
| 丸め | 半刻みを加えて整数化 | RINT。構成により rint または floor(x+0.5) | 境界値の振舞いを確認 |
| 反転中 | 旧実装は水平成分を反転して角度計算 | 現行表示変換と別途照合 | 旧操作方向を維持 |

回転の表示行列・再描画には現行基盤を使えるが、入力操作を丸ごと標準へ置き換えない。

パース定規は画像座標で管理し、消失点の編集と筆跡拘束を分離する。手ぶれ補正・対称描画との順序を明示する。旧版の方向決定距離には画面縮尺が関与するため、数値だけを移さず挙動を比較する。

塗りつぶしブラシは開始時の参照画像と開始色を使い、ブラシ範囲を領域探索の境界として扱う。範囲外を回り込んでつながる領域では、全体を探索した後でマスクを掛ける処理とは異なる。選択境界についても同じ比較を行う。

キャンバス UI は、透明化と入力透過を区別する。ペンを離した時、ポップアップを閉じた時、画像を切り替えた時の状態復帰まで含める。

### 7.1 GEGL の描画経路と塗りつぶしブラシ

GIMP 3 の表示・合成は GEGL ノードグラフ、画素の格納と paint core は `GeglBuffer` を使う。独自機能を旧 `PixelRegion` や旧投影の更新処理のまま挿入しない。GEGL Operator を自作するか、既存の operator と buffer を接続するかは、評価の入口ごとに決める。「GEGL 非破壊効果への置換禁止」は独自 FilterLayer のデータモデルとスケジューラに対する制約であり、GIMP 3 の描画グラフを使わないという意味ではない。

| 機能 | 計算と状態の所有 | GIMP 3 への接続と確認点 |
|---|---|---|
| 合成モード | 旧画素式、alpha と合成空間の意味を保持 | `GimpLayer` の mode node と現行 operator を数値照合し、結果が異なる式には独自 GEGL 合成 operator を接続。opacity、mask、group の順序も試験 |
| CloneLayer | 独自参照、座標変換、更新通知を保持 | 参照元の確定画素を drawable の source node に供給し、変更範囲と投影キャッシュを無効化。循環とグループ参照も確認 |
| FilterLayer | 独自型と runner、依存先優先のスケジューラを保持 | 旧世代・処理途中の画素を公開せず、確定結果の buffer をレイヤーの source node へ供給。再評価は operator の `process` callback に起動させない |
| MyPaint・紙目・通常ブラシ・Smudge | ストローク時の状態、採色元、押印を paint core / Surface で管理 | 現行 paint buffer、マスク、Undo へ接続し dirty 領域を通知。dab ごとの独自 GEGL operator を前提にしない |
| 塗りつぶしブラシ | stroke 開始時の投影 snapshot と開始色、dab ごとの有界探索を管理 | 変形済みブラシ mask を探索の通過条件と coverage に使用し、結果 mask を現行 paint core の `GeglBuffer` 合成へ渡す。ブラシ範囲を後から切り抜く実装は採用しない |
| 選択境界を使う塗りつぶし | 探索中に選択外へ進まない境界条件を管理 | 標準 bucket の全域探索→選択範囲で裁断する経路と比較し、有界探索への入力 mask を別途用意する |

移植先の `gimp_pickable_contiguous_region_by_seed()` は pickable の現在画素を flush し、探索元、開始色、探索境界を外から指定できない。標準 `gimp_drawable_get_bucket_fill_buffer()` は探索後に選択範囲を適用する。このため旧ブラシの `gimp_image_contiguous_region_by_seed_full()` をそのまま呼び替えることはできない。旧ブラシでは固定開始色とブラシ境界、mask による探索制限、1 px の領域拡大、constant mode の paint paste が一つの dab に含まれる。旧版で有効な比較式は共有候補だが、範囲・座標・stroke 単位の Undo とともに実画像で比較する（`migration/inventory/fill-brush-rendering.md`）。

描画途中の preview と確定結果を分離し、paint core が Undo、合成、表示更新を管理する。GEGL グラフは遅延評価・領域別評価されるため、ブラシの開始スナップショットや flood fill の副作用を operator の `process` に置かない。取り込み点、ROI、フォーマット、色空間、キャッシュ無効化を個別の WBS 子タスクとして検証する。

## 8. 実装の配置

| 配置 | 主な対象 | 境界 |
|---|---|---|
| GIMP 本体 | 独自レイヤー、参照、保存読込、FilterLayer 制御、描画入力、キャンバス UI | 現行型・寿命・Undo と接続し、機能単位の変更群に分ける |
| 拡張ブラシエンジン | 入力評価、混色状態、独自パラメーター | Surface への受渡し仕様を明示する |
| 描画・画素処理基盤 | GeglBuffer、マスク、必要な合成演算 | GEGL の既存非破壊効果への置換と混同しない |
| 非同期実行器 | FilterLayer の処理起動・停止・終了通知 | 独自制御の優先順位とチェックポイントに従う |
| プラグイン等 | 対応可能なプリセット操作、任意の外部連携 | 本体の型や UI に依存する処理を無理に外出ししない |
| 資源パッケージ | ブラシ、紙目、プレビュー、プリセット | 資源参照と欠落時の情報保持を設計する |

### 8.1 実装言語の推奨

**GIMP 本体の C/GObject 接続を維持し、gimp-painter の独自実装は C++ を主軸として移植する。Rust 全面移行は今回の基準案にしない。** Rust は、GIMP オブジェクトから分離できる新規モジュールで、導入効果を検証した場合に限定して採用する候補とする。Rust を使うために既存 C++ の互換性確保を後回しにしない。

これは「C++ でしか実装できない」という判断ではない。GObject の Rust subclass 機構は存在する。しかし、本件は GTK widget の作成だけでなく、GimpLayer／GimpDrawable／GimpPaintCore 等の app 内部型、仮想関数、シグナル、Undo、投影、プラグイン実行に接続する必要がある。公開 libgimp や GTK の binding だけでは、この内部接続は完成しない。

旧版は C++14 を要求し、調査対象 GIMP 3.0 の Meson も C と C++、cpp_std=c++14 を指定している。初期移植の基準は C++14 とし、C++17 以降が具体的に必要な場合は独自ターゲットのコンパイラー条件と全対象環境への影響を確認して決める。言語更新だけを目的に GIMP 全体の標準を引き上げない。

### 8.2 C++ コードの実体

移植元 app 配下の .cpp/.hpp を集計すると 71 ファイル、28,633 行。これはコメント等を含む現存ファイルの行数であり、独自差分行数や移植工数ではない。多数の .c/.h 側にも C++ 宣言・接続用の変更があるため、71 ファイルだけの作業とはしない。

| 場所 | .cpp/.hpp 数 | 行数 | 役割と移植方針 |
|---|---:|---:|---|
| app/base | 12 | 3,952 | GObject の型・所有権・callback・JSON 等の独自 C++ ラッパー。依存を棚卸しし、必要部分を適合させてから段階的に縮小 |
| app/core | 9 | 4,373 | CloneLayer、FilterLayer、独自 Undo、ブラシ資源、定規モデル。C++ の実装ロジックを維持して現行型へ接続 |
| app/paint | 14 | 4,353 | 拡張 MyPaint、Surface、DrawableFeature、ストローク状態。演算・状態更新と GIMP 依存を分離 |
| app/paint-funcs | 1 | 687 | MyPaint の混色・合成等。数値挙動を比較しながら現行画素表現へ対応 |
| app/display | 2 | 1,032 | overlay、定規表示。現行キャンバス／GTK3 接続を再実装 |
| app/tools | 11 | 4,638 | 描画ツール、オプション、editor。C++ モデルと GTK/GObject 接続を分ける |
| app/widgets | 7 | 5,016 | レイヤータイル、ツールタイル、popup、editor。状態モデルを共有し、ライフサイクルを明示 |
| app/pdb | 2 | 297 | ProcedureRunner、引数設定、同期／非同期実行器。現行手順 API へ適合し、制御契約を保持 |
| app/presets | 5 | 1,754 | JSON 資源とレイヤー構成処理。保存意味を維持して実装配置を判断 |
| app/httpd | 6 | 2,401 | REST/PDB/画像操作。任意機能として分離を検討 |
| app/gimp-features.cpp | 1 | 130 | 独自機能の接続・登録。現行初期化へ適合 |
| app/dummy.cpp | 1 | 0 | 空のビルド用ファイル。Meson の構成に応じて整理 |

glib-cxx-impl.hpp の NewGClass は、単なる便利関数ではない。GType 登録、親 class への接続、private 領域内の placement new、C++ destructor の呼出し、vfunc trampoline を持つ。FilterLayer と CloneLayer はこの仕組みに依存している。delegators.hpp は std::function 等を GClosure に接続し、Connection の破棄でシグナルを切断する。これらの寿命の意味を分析せず置換すると、画像終了やレイヤー削除時の callback が壊れる。

glib-cxx-bridge.hpp には GdkDrawable 等の旧 GTK 型への接続もある。旧 bridge 全体を機械的に修正する作業と、各機能の GTK3 適合を分ける。標準型への接続が不要な純粋な数学・状態更新処理まで書き直す必要はない。

### 8.3 C++ と Rust の技術比較

| 観点 | C++ を主軸にする場合 | Rust を主軸にする場合 | 本件の判断 |
|---|---|---|---|
| 旧ロジックの再利用 | 演算・状態遷移を保持して接続層から変更できる | 同じ仕様を別実装として書き直す範囲が増える | 互換性確保では C++ が有利 |
| GIMP 内部型との接続 | 現行 C header、vfunc、既存 GObject 規約へ直接適合できる | 内部型の binding／subclass glue、または C shim が必要 | 本体境界は C/GObject に固定 |
| 寿命・所有権 | RAII、unique_ptr、専用 ref/weak-ref ラッパーで明示。規約と検証が必要 | safe な所有データは型で制約できるが、外部 GObject の寿命保証は binding の契約次第 | Rust の効果は所有データが独立する部分で大きい |
| 並行実行 | 型と実行規約で分離し、競合は解析・試験する | safe Rust の Send/Sync と所有権が共有の制約を強める | いずれも古い結果の反映や依存順序は別途設計が必要 |
| 非ブロッキング性 | destructor、ロック、copy、join、同期 callback まで監査する | Drop、Mutex、blocking I/O、非 yield の async 処理等を監査する | 言語や async の採用だけでは保証できない |
| 数値互換性 | 旧式・演算順序・乱数を保ちやすい | 丸め、変換、整数 overflow、SIMD、演算順を再検証する | 描き味の核はまず C++ で維持 |
| ファイル解析 | 旧 reader の特殊動作を保持しやすい。境界検査等は補強する | slice・enum・checked arithmetic を使った解析を分離しやすい | Rust の候補だが、旧 writer の癖を正規仕様で上書きしない |
| UI | GTK3 C API と現行 GIMP widget へ接続 | GTK3 binding に加えて GIMP 独自 widget の接続が必要 | UI の全面 Rust 化は初期範囲にしない |
| ビルド | 既存 Meson の C/C++ 構成を利用 | rustc、crate、ABI、リンク、対象 triple の管理を追加 | 技術的に可能だが、追加の再現性検証が必要 |
| 性能 | 既存実装と比較可能 | コピーを抑えた境界と最適化を設計すれば高性能にできる | 言語名では決めず、筆跡遅延・UI占有・メモリで測定 |

Rust による GObject subclass は実現可能であり、gtk3-rs も調査時点では archived=false で開発が再開されている。「GTK3 の Rust binding がない／永久に保守終了している」ことを不採用理由にはしない。ただし、GIMP app 内部型を安全に扱える完成済み binding があるという意味ではない。利用する場合は採用 release、glib 関連 crate、Rust 最低版を固定する。

### 8.4 複数の C++ ブリッジをどう移植するか

旧コードには異なる役割のブリッジが併存する。以下はソースで確認した機構と、その移植案である。すべてを一つの汎用 FFI に置換する計画にはしない。「C から C++ への呼出し」「C++ から C API の利用」「GObject の実装」「既存オブジェクトへの状態付加」「画像処理の backend 抽象化」は、それぞれ異なる境界である。

| ブリッジの種類 | 確認した旧実装・役割 | 初期の C++ 移植方針 | Rust を採用する場合の境界・注意 |
|---|---|---|---|
| 型宣言・型検査・キャスト | `glib-cxx-types.hpp` の `Traits`、`g_type<T>()`、`__DECLARE_GTK_CLASS__`、GTK/GIMP interface 用 macro。`glib-cxx-bridge.hpp` が GTK 型を登録 | 必要な特殊化を維持し、GTK3／現行 GIMP の型へ一件ずつ適合。class と interface、`Iface` と `Interface` の区別を保持。旧 `GdkDrawable` 等の廃止型は利用箇所ごとに変更 | GTK binding だけでは app 内部型は揃わない。内部型の宣言と wrapper が別途必要。opaque handle を基本とし、旧 cast macro を直訳しない |
| GObject 型登録・実体配置 | `glib-cxx-impl.hpp` の `DerivedFrom`、`UseCStructs`、`NewGClass`。C instance/class と C++ Impl を分離し、private 領域に placement new | 型登録・親 class 接続を現行 GLib に適合し、Impl の所有と取得は 8.4.1 の共通 BindingStore に統一する。private 領域内の placement new は移行中だけ許す | Rust subclass は可能だが、GimpLayer 等の内部親型の binding／subclass 契約も実装が必要。初期段階は GObject を C/C++ 側に置き、Rust の状態を handle で所有する方が変更範囲が小さい |
| 仮想関数・GInterface 実装 | `NewGClass::Binder` が C 関数ポインターから C++ メンバーへ転送。`add_interface_iter` が GInterface を登録 | 現行 vfunc の引数・返値・親呼出しを一件ずつ確認し、正しい型の trampoline を使う。旧 `reinterpret_cast` による関数ポインター変換を API 差分の吸収手段にしない | `extern "C"` trampoline と明示した内部操作が必要。Rust の trait object を C の vtable にそのまま渡さない |
| C API と C++ 抽象 interface の二重入口 | `gimpclonelayer.h/.cpp` の `gimp_clone_layer_*` と `CloneLayerInterface`。型検査→private Impl→`dynamic_cast` で接続 | C 側の呼出し契約を保ち、C++ 呼出し元は型付きハンドルに統一。旧抽象 interface は移行用 adapter とし、切替完了後に削除。GObject と C++ Impl のアドレスを同一視しない | Rust は C 入口または専用 handle API を使用。C++ 多重・仮想継承や RTTI の layout を FFI 契約にしない。必要なら C++ interface の実装が Rust に転送する |
| C 関数をメソッド風に呼ぶ wrapper | `glib-cxx-utils.hpp` の `IObject::BoundMethod`、`ref(obj)[c_function](args)` | 型付きハンドルの名前付きメソッドへ置換し、C API へ転送。旧糖衣と raw pointer を保持する BoundMethod は移行中だけ許す。変更された引数・返値をコンパイル型検査で追う | Rust では安全な wrapper メソッドとして必要分だけ実装。呼出し対象が生存する期間と実行 context を型・API の契約にする |
| 参照・値・メモリー所有権 | `Object`／`IObject`、`HoldValue`／`CopyValue`／`IValue`、`ScopeGuard`／`CXXPointer` | 所有権を明記して適合。`IObject` は構築・コピーで ref を増やす。`Object`／`hold(T*)` は受け取った参照を所有し、破棄時 unref。`IValue` は借用、`CopyValue` は複製。move／代入／解放を監査し、必要に応じ専用 RAII 型へ置換 | GObject 用所有型、借用型、C allocator に対応した解放 API を分ける。`Box::from_raw` で GObject や GLib のメモリーを回収しない |
| プロパティ宣言・GValue 変換 | `glib-cxx-impl.hpp` の property getter/setter 接続、`glib-cxx-utils.hpp` の `FundamentalTraits` と文字列添字アクセス | property 名・GType・default・notify の意味を保持。現行 API に合わせ、暗黙変換・一時 GValue・文字列の寿命を確認。保存引数の型と実行用の型を混同しない | Rust の property／Value wrapper に置換可能だが、enum 数値、null、所有権、notify 順序は個別検証が必要 |
| シグナル・closure の接続 | `delegators.hpp` の `Delegator`／`ObjectDelegator`、`GClosure`、`Connection` | callback＋user data＋destroy notify の契約を維持。旧 Connection は target を raw pointer で持つため、target 先行破棄に対応する。after／block／unblock と切断順序を維持 | Rust closure 化でも C 側の呼出し寿命は残る。weak reference、handler 管理、再入と遅延 callback の失効判定が必要。切断だけで投入済み処理を消せるとは扱わない |
| 既存 GObject への C++ 状態付加 | `g_object_set_cxx_object` が `g_object_set_data_full`／`g_object_set_qdata_full` と delete callback を使う | 自由な key による付加を廃止し、独自 subclass と同じ BindingStore・型付き slot に集約。停止してから解放し、二重所有と稼働中の置換を防ぐ | 同じ方式で Rust handle を付加可能。専用 destroy 関数へ返して解放し、後着 callback から解放済み状態を触らせない |
| GTK 構築 DSL・C からの UI 入口 | `glib-cxx-def-utils.hpp` の `Definer`／`Packer`／`with`、`gimpeditor-cxx.cpp` 等の C 入口 | DSL はそのまま残せる部分と GTK2 API 依存を分ける。box 等を GTK3 API に適合し、widget 所有権と signal を検証。UI 配置・操作を言語移行のために削らない | UI の Rust 全面置換は初期案にしない。必要な model 操作だけを C 境界に出す。GIMP 内部 widget の binding は別作業 |
| PDB 引数・非同期 runner | `pdb-cxx-utils.hpp` の `ArgConfigurator`、`ProcedureRunnerImpl`、同期／非同期 executor。引数変換だけでなく running、cancel、Undo の制御を含む | 現行 GIMP の procedure／値配列／実行 API に適合する独立 adapter にする。起動、停止要求、完了通知、Undo 制御を明示し、FilterLayer の制御契約を維持 | Rust 化するなら状態モデルと実行器を分離。GObject/PDB 操作は C/C++ 側。future や task の中止だけで旧処理の停止完了と扱わない |
| 描画アルゴリズムと画像格納方式 | `gimpmypaintcore-drawablefeature.hpp` の DrawableFeature 群、`gimpmypaintcore-brushfeature.hpp` の brush 抽象化 | C++ template による backend 境界を活用。TileManager／PixelRegion 依存を buffer adapter に置換し、採色・押印・非累積・Undo を保持。GeglBuffer 利用は保存基盤の選択であり、GEGL 非破壊効果への統合ではない | 純粋な画素演算の分離は候補。ポインター、形式、stride、領域、所有期間を固定し、細粒度の FFI 呼出しを増やさない。描き味の同等性を先に確立 |

`json-cxx-utils.hpp`、`soup-cxx-utils.hpp` 等の外部ライブラリー用 wrapper も所有権・callback の点検対象に含める。HTTP を任意機能へ分離する案と、保存資産の JSON 読込みを維持する要件は分ける。wrapper の廃止を、その利用機能の削除と同義にしない。

型登録と寿命については、推奨構造を「現行の GObject オブジェクトが所有する C++ 実装オブジェクト」とする。GimpLayer 等の C instance/class レイアウトを C++ 継承のレイアウトで置き換えない。8.4.1 の単一インターフェースを移植完了時の目標とし、旧 bridge は互換 adapter として段階的に撤去する。GTK 全体の binding を作り直すことは前提としない。

特に次を移植の合格条件とする。

- 構築時には C++ constructor を実行する。GObject の zero-initialization では代替できない。private 領域内方式を残す場合も、構築・破棄・alignment の対応を検証する。
- dispose で接続解除・ジョブ失効等を行い、繰返し呼出しに耐える。finalize で残る所有物を解放し、親処理へつなぐ。旧版で finalize に偏っていた寿命処理を、そのまま安全と仮定しない。
- FilterLayer の削除・画像終了・再実行で、停止要求と停止完了を分ける。UI thread の destructor、finalize、Rust Drop で無制限に join しない。実行器が必要な資源を停止完了まで保持し、世代の古い結果を反映しない。
- CloneLayer の source 参照、シグナル切断、Undo による復元は一体で検証する。新しい所有権型を導入して旧参照解決や更新順序を変えない。
- C callback／C ABI に C++ exception を越境させない。旧 delegator の catch 部分には無効化されたコードがあるため、安全な境界が既にあるとは扱わない。エラー値への変換と失敗時の状態整理を実装する。

### 8.4.1 C++ インターフェースを一種類に統一する提案

**型付きハンドルを唯一の C++ 利用 API とし、GObject が所有する BindingStore を唯一の C++ 実装管理方式にする。** 以下は未実装の設計案であり、旧ソースに存在する名称ではない。

統一対象は二つの軸で定義する。利用側では `ref(obj)[C関数]` と `XxxInterface::cast(obj)` を統一する。実装側では NewGClass の private 領域内 Impl と、既存 widget に data/qdata で付加する C++ 状態を統一する。単に二方式へ同名の関数を被せて永久併存させる案にはしない。

| 要素 | 統一後の設計 |
|---|---|
| C++ 利用 API | `CloneLayerRef`、`FilterLayerRef`、`WidgetRef` 等を共通の `ObjectRef<T>` 基盤から定義。名前付きメソッドを使い、raw Impl・抽象 interface の cast を利用側に公開しない |
| C API | 既存の C 入口を現行型へ適合して残す。C++ の操作メソッドはこの C 入口へ薄く転送し、同じ検証・処理経路に入る。内部 Impl から自分の入口へ再転送して再帰させない |
| Impl 管理 | 各 GObject の予約済み qdata key 一つに BindingStore を付加。store が型付き slot ごとに Impl を unique ownership で保持。複数の UI behavior がある場合も同じ store 内で管理 |
| 型安全性 | slot の宣言に期待する GType、Impl 型、ライフサイクル方針を固定。任意の文字列 key と任意の T を組み合わせる取得 API を禁止。多重継承のアドレス変換を reinterpret_cast で行わない |
| GType と継承 | CloneLayer／FilterLayer 等の GType、親 class、vfunc、property、signal は維持。C 互換 instance/class に対する登録と trampoline は型ごとの adapter が担当 |
| 一般 widget | 型を変更せず store に behavior を登録。destroy または所有 UI controller の終了時に同じ close 操作を呼ぶ。任意の GObject に共通の dispose signal があるとは仮定しない |
| 所有権 | ハンドルは GObject の strong ref を持つ。retain、adopt、floating ref の sink を明示的な factory で区別。Impl から owner への strong ref は持たせず循環を防止。内部の短期借用は同じ基盤の所有権規則に従う |
| C++ の多態性 | 利用 API の多態性は GType と既存 vfunc を基準にする。独立した計算モデル内の C++ interface は必要に応じ残せるが、GObject を取得するための第二の方式にはしない |
| エラー処理 | 型不一致、未登録、終了状態を明示。取得で暗黙に Impl を生成しない。C 入口と callback で例外を捕捉し、契約に合うエラーへ変換 |
| 性能 | store／slot の解決は操作・job・描画領域単位。画素ごとの qdata lookup は行わない。Impl の短期借用を処理内に閉じ、buffer コピーも計測 |

C++ の呼出し経路は「型付きハンドルのメソッド → C 入口 → 共通 store の Impl → 処理」とする。GIMP の vfunc は「型ごとの trampoline → 同じ Impl → 処理」とする。この二入口は GIMP との接続に必要であり、C++ 利用者には一種類の API だけを提供する。

**ライフサイクルは保存場所と別に設計する。** qdata の destroy notify は finalize や値の置換時の解放に使えるが、dispose 時の停止・切断を代替しない。独自型の dispose、widget の destroy、controller の明示終了から、同じ冪等な close 操作を呼ぶ。

1. 構築中：store と slot を登録する。constructor は callback を発火させず、必要な property が揃った後に接続を有効にする。構築中の property 設定は明示した初期化経路で受ける。
2. 稼働中：通常操作を許可。slot の任意置換・削除は禁止。呼出し中の再入による終了にも備え、Impl 本体はその場で破棄しない。
3. 終了処理：close で世代を失効させ、接続を解除し、停止を要求し、他オブジェクトへの参照を解放。二度目以降の close は安全に戻る。ハンドルが GObject を保持していても稼働中とは限らないため、メソッドは終了状態を検査する。
4. 解放：finalize に伴う store 解放で Impl を破棄。実行中 job は独立した入力・結果・キャンセル状態だけを所有し、Impl の生ポインターを保持しない。完了は main context で owner の weak ref と世代を確認して反映する。close／destructor で待機 join をしない。

close 後も GObject instance は finalize まで有効であり、必要な読取りや親処理を可能にする。dispose で無条件に Impl を delete する方式にはしない。対象を操作する間は strong ref と必要な呼出し状態を保持し、signal 再入中にも破棄されない契約にする。job から最後の UI object 参照を worker thread 上で解放する設計は避ける。

| 移行段階 | 完了条件 |
|---|---|
| 1. 共通規約 | ObjectRef、BindingStore、typed slot、close、Connection の契約を確定。生成・参照・再入・終了を小さな対象で検証 |
| 2. 呼出し側 | 旧 ref 関数添字と Interface::cast を新メソッドへ移す。移行中の実装差は adapter 内に閉じる |
| 3. 実装側 | 型ごとに private Impl または自由な qdata 付加から store 所有へ移す。C 入口・vfunc・signal が同じ Impl を参照することを確認 |
| 4. 特殊制御 | CloneLayer の参照・Undo、FilterLayer の優先順位・チェックポイント・終了条件・非ブロッキング性を旧版と比較 |
| 5. 旧方式撤去 | 利用箇所を検索し、旧 Interface 取得・NewGClass 内の Impl 配置・自由な状態付加・旧呼出し糖衣を削除。必要な GType 登録機能は新 adapter に残す |

統一を理由に XCF 型や参照方式、保存済み引数、描き味を変更しない。Rust を将来導入する場合も Impl の内部へ限定して組み込み、この C++ API と GObject 境界は増やさない。

### 8.5 Rust を導入するなら置く場所

| 候補 | 効果 | 条件・優先判断 |
|---|---|---|
| 保存済みデータの検証・独自レコード解析 | 境界検査、型付きの解析結果、未知情報の保持を独立させやすい | XCF 全体を最初から再実装しない。旧版との fixture 比較を通る小さな decoder から検討 |
| FilterLayer の純粋な状態遷移モデル | job ID、世代、依存関係、状態の組合せを型で表現しやすい | GObject pointer を持たず、「入力イベント→起動／停止／反映要求」を返すモデルに限定。C++ の移植基準を得てから比較 |
| プリセット・ブラシメタデータ処理 | 構造検証、未知項目の往復を切り離せる | 型名や参照解決の旧規則を維持。外部 crate の既定の正規化で値を変えない |
| 新規の独立した画素演算 | 所有する buffer 上で実装を完結できる | 旧演算との数値比較と性能測定が必須。既存 MyPaint の移植を妨げない |
| GimpLayer 派生型全体 | Rust 側で所有モデルを作れる可能性 | 内部 binding と vfunc 接続の実装範囲が広く、初期採用しない |
| GTK UI 全体 | Rust の UI 記述・型を使用できる | 独自 GIMP widget と本体状態の binding が増えるため、初期採用しない |
| 既存 MyPaint エンジン全体 | 言語を統一できる | 全面再実装と筆跡再検証の負担が大きく、初期採用しない |

「限定 Rust を使う」ことも未実装の設計候補である。モジュールを採用する場合は、互換性・境界の複雑さ・性能・ビルド再現性が C++ 案より改善する具体的な根拠を要求する。小さな処理のためだけに全製品の必須 toolchain を増やすかも判断する。

### 8.6 C／C++／Rust の境界契約

- 共有境界は狭い C ABI とする。固定幅整数、明示した長さと stride、エラーコード、不透明 handle を使う。C++ STL、Rust Vec/String、Rust 固有の enum layout を直接公開しない。
- allocation と解放を同じ側で行う。借用 buffer の有効期間、読取り専用／書込み可、非同期処理へ保持できるかを明示する。
- callback の user data の寿命と解放時点を定義する。画像／レイヤーの消滅後は job が生存していても GObject を触らない。
- C++ exception と Rust panic を通常の C ABI 境界へ越境させない。Rust の Result をエラー値へ変換し、unwind 型 panic を捕捉する場合も捕捉後の状態を検証する。panic=abort やメモリ不足による中断まで catch_unwind で回復できるとは扱わない。
- Rust の unsafe は FFI と必要な低水準 buffer 操作へ局所化する。GIMP の生 pointer を便宜的に Send/Sync 扱いして worker へ送らない。
- GIMP オブジェクト操作は main context 側 adapter が所有する。worker は独立した入力と job 状態を扱い、結果だけを明示的にキューへ返す。
- UI thread の destructor／Drop に待機 join を入れない。キャンセル要求と失効を通知し、worker が参照する状態は完了まで別に所有する。安全に解放するために UI を無期限停止させる設計は不可。
- main context への通知は、即時再入してよいのか後で dispatch すべきかを明示する。ロックを保持したまま GObject の通知や callback を呼ばない。

C ABI の呼出し一回自体よりも、境界をまたぐ画像全体のコピーや押印ごとの再変換がコストになり得る。境界は一画素ごとではなく領域／job 単位を基本とし、共有 buffer の所有と同期が保証できる範囲でコピーを抑える。

### 8.7 ビルドと検証

既存の Meson を親ビルドに維持する。C++ モジュールは同じ配布物内で再ビルドし、C++ の公開 binary ABI を配布互換性の前提にしない。

Rust を採用する場合は C ABI を持つ staticlib 等としてリンクする案を基本とする。Meson は Rust を扱えるが、今回の GIMP 3.0 が宣言する最低 Meson は 0.61.0 である。最新文書にある API がその最低版で利用できるとは仮定しない。直接 Meson でビルドするか Cargo を呼ぶ専用ターゲットにするかを選び、同じ crate を二重にビルド・解決しない。

Rust toolchain、依存 crate、lockfile、必要なら vendor、対象 triple、panic 方針、静的リンク依存を固定する。C/C++ と Rust の対象 architecture・runtime を合わせる。Linux、Windows、macOS を対象とする場合は各環境でリンクと実行を確認し、ARM64 や universal 配布を採用する場合も architecture ごとに同じ互換性試験を通す。

| 検証 | 対象 |
|---|---|
| 旧仕様との比較 | XCF round-trip、CloneLayer 追従、FilterLayer のイベント順、筆跡と採色 |
| 寿命 | signal 切断、画像終了、レイヤー削除、Undo、job 中断・完了の競合 |
| C++ の低水準検証 | ASan/UBSan 等を対象環境で利用。競合が疑われる isolated core には TSan 等を適用 |
| Rust 境界 | handle の不正利用、buffer 長、error/panic、callback 失効。safe core の試験と FFI の試験を分ける |
| 非ブロッキング性 | 起動、入力準備、キャンセル、結果反映、破棄を含む main-thread の占有時間 |
| 性能 | 同じ作品・入力記録で遅延、peak memory、コピー量を比較。言語だけを根拠に優劣を付けない |

この言語方針は既存の 36 項目を置換・削減するものではなく、その実装方法を追加する。保存互換性、CloneLayer の旧仕様、FilterLayer の独自非ブロッキング制御、拡張 MyPaint の維持が引き続き上位要件である。


## 9. 実装順序と合格条件

| 段階 | 作業 | 合格条件 |
|---|---|---|
| 0 | 基点と旧実行環境を固定。保存項目台帳、代表作品、ブラシ、入力記録を用意 | 旧版での読込み・表示・編集動作を再取得できる |
| 1 | 必要な C++ bridge の適合、文書モデル、独自型、旧 XCF 識別・reader・writer | 型・値・階層・参照を失わず開いて保存できる |
| 2 | CloneLayer、独自合成、複合レイヤー構成 | 参照元を編集すると旧仕様どおりに追従し、再保存後も動く |
| 3 | FilterLayer の実行器と更新制御 | 依存順序・非ブロッキング更新・中止・反映・終了を検証できる |
| 4 | 拡張 MyPaint、Smudge、紙目、非累積、保存と editor | 代表ブラシの描画と再編集を維持できる |
| 5 | 回転スナップ、筆圧、定規、塗りつぶし、キャンバス UI | 旧制作操作を維持できる |
| 6 | 重複実装・周辺機能の整理 | 互換性の試験を維持したまま保守対象を減らせる |

ブラシの難所の試作は文書互換性の作業と並行できるが、CloneLayer／FilterLayer を最後の任意工程にはしない。

| 検証群 | 必須ケース |
|---|---|
| 読込み | 旧通常 XCF、独自 v4、属性 32/33、モード 23〜29、複合構造 |
| 往復 | 開く→編集→保存→再読込。型・値・参照・描画・動作を比較 |
| CloneLayer | レイヤー／グループ参照、同名・改名・複製・移動・削除・Undo、参照先の変更追従 |
| FilterLayer | 下層連続描画、連鎖したフィルター、処理中の設定変更・並べ替え・表示変更・削除・画像終了 |
| 実行制御 | 多重起動、古い結果の反映、待ち解除漏れ、自己更新ループ、失敗時の無限再試行がないこと |
| 非ブロッキング性 | 処理が重い状態でも入力・編集・表示が進むこと。準備・コピー・完了通知を含む UI 占有時間を測定 |
| 終了条件 | 編集が止まった後に最新状態へ収束し、未処理・依存待ち・実行中が残らないこと |
| 保存中の処理 | 実行中の保存、再読込み、再評価で世代の混同や構造欠落がないこと |
| ブラシ | x/y・時刻・筆圧・傾き・乱数 seed 等を固定し、採色形状、透明境界、動的紙目、非累積混色を比較 |
| 回転 | Ctrl 押下解除、追加キー、反転中、0/360°付近、半刻み境界 |
| 塗り | ブラシ外／選択外経由で連結する領域、半透明、小さい・ずれたレイヤー |
| UI/Undo | 複数画像、ペン操作、ポップアップ復帰、一筆／一操作の取消し |

必須要件はまだ達成済みではない。今後の実装と実ファイル・実機試験で確認する。旧版で正常に開けるファイルが開けない、独自型が焼き込まれる、参照や設定が失われる場合は、初期版であっても互換移植の合格としない。

## 10. 一次ソース

- [独自差分の比較](https://github.com/seagetch/gimp-painter/compare/a61915a8e62aad8855cd8c620bdb7195a25ffb90...afa43fae3e920210146abed514f136fd49f671b5)
- [FilterLayer](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/core/gimpfilterlayer.cpp)
- [CloneLayer](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/core/gimpclonelayer.cpp)
- [非同期実行器](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/pdb/pdb-cxx-utils.hpp)
- [旧投影処理](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/core/gimpprojection.c)
- [旧 XCF 属性](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/xcf/xcf-private.h)
- [旧 XCF writer](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/xcf/xcf-save.c)
- [旧 XCF reader](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/xcf/xcf-load.c)
- [現行 XCF 属性](https://github.com/GNOME/gimp/blob/6ac8031c9555667e080fef0324fca9f28a02e5ab/app/xcf/xcf-private.h)
- [アニメ塗り構成](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/data/layer-presets/new-anime-layer.json)
- [水彩構成](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/data/layer-presets/watercolor.json)
- [旧ブラシエンジン](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/paint/mypaintbrush-brush.hpp)
- [旧 MyPaint Surface](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/paint/gimpmypaintcore-surface.cpp)
- [旧ブラシ形状・採色処理](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/paint/gimpmypaintcore-brushfeature.hpp)
- [旧ブラシ writer](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/core/gimpmypaintbrush-save.cpp)
- [現行 MyPaint Surface](https://github.com/GNOME/gimp/blob/6ac8031c9555667e080fef0324fca9f28a02e5ab/app/paint/gimpmybrushsurface.c)
- [旧回転イベント](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/display/gimpdisplayshell-tool-events.c)
- [現行回転イベント](https://github.com/GNOME/gimp/blob/6ac8031c9555667e080fef0324fca9f28a02e5ab/app/display/gimpdisplayshell-tool-events.c)
- [現行回転計算](https://github.com/GNOME/gimp/blob/6ac8031c9555667e080fef0324fca9f28a02e5ab/app/display/gimpdisplayshell-rotate.c)
- [旧 C++ 型登録・vfunc bridge](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/glib-cxx-impl.hpp)
- [旧シグナル接続・寿命管理](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/delegators.hpp)
- [旧 DrawableFeature](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/paint/gimpmypaintcore-drawablefeature.hpp)
- [GIMP 3.0 の Meson 設定](https://github.com/GNOME/gimp/blob/6ac8031c9555667e080fef0324fca9f28a02e5ab/meson.build)
- [Rust の GObject subclass](https://gtk-rs.org/gtk-rs-core/stable/latest/docs/glib/subclass/index.html)
- [GTK3 Rust bindings](https://github.com/gtk-rs/gtk3-rs)
- [Rust FFI と unwind](https://doc.rust-lang.org/nomicon/ffi.html)
- [Meson Rust 対応](https://mesonbuild.com/Rust.html)
- [型宣言・Traits](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/glib-cxx-types.hpp)
- [参照・値・関数 wrapper](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/glib-cxx-utils.hpp)
- [C++ 所有権補助](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/scopeguard.hpp)
- [GTK 構築 DSL](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/base/glib-cxx-def-utils.hpp)
- [CloneLayer の C/C++ 二重入口](https://github.com/seagetch/gimp-painter/blob/afa43fae3e920210146abed514f136fd49f671b5/app/core/gimpclonelayer.h)
- [GObject qdata の解放契約](https://docs.gtk.org/gobject/method.Object.set_qdata_full.html)
- [GObject dispose の契約](https://docs.gtk.org/gobject/vfunc.Object.dispose.html)

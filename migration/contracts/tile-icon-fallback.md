# 29.008/icon-null — 有界依存の明示化

## 元の義務を保持する

元タスク: `29.008/icon-null`「tile icon 生成失敗時の NULL 経路を処理する」。
元の必須先行 ID: **29.002, 29.008, 01.007/cpp-ownership**。
元の受入条件（変更しない）:

> layer/tool 両 tile の icon 未登録・render 失敗時に NULL を unref せず、代替表示を確認

これは失敗分岐の所有権と代替表示という有限の義務である。旧UI全体の忠実性、
全tile表示、tool managerの全同期動作の完了へ言い換えない。元の義務を別の子へ
隠すための新タスクは作らず、同じ `29.008/icon-null` に実装と証拠を結び付ける。

## 現行コンポーネントの入力と根拠

直接の実装入力を `29.001/native-canvas-tiles`, `01.007/cpp-ownership`,
`07.008/untracked-signals` へ明示する。いずれも既存DONEである。ただし
`native-canvas-tiles` の古い合格は履歴であり、今回のコードの合格へ流用しない。
今回の対象経路は新しい native 5件と既存 layer70/canvas107件で別途実行し、
実canvasにtileとGtkImageが存在し、実drawと破棄を行えるという有限の入力を確認する。
試験・対象source・既存fixtureのcheckpoint・実画像は `migration/tests/tile-icon-fallback/`
へ固定する。依存の変更は、親の未完了義務を免除するものではない。

- layer: 現行 `Tiles::draw_icon` のNULL unrefガードを保持し、未登録名・loader NULL・
  pixbuf変換NULLで、themeや追加raster allocationに依存しない代替glyphを表示する。
  pixel判定、失敗cacheの反復draw、変換用pixbufの一回解放を検証する。
- tool: 旧手動pixbuf経路ではなく現行GtkImage委譲を試験する。未登録名で実GTKの
  fallback描画、NULL名でimage-missing描画、owner破棄後の保持imageの最終解放を検証する。
  手動unref経路が存在しないことと、実fallbackが可視であることを区別して確認する。
- 通常経路を変更せず、元の時間制限を保持する。ASan/UBSanは列挙した有界対象のみ。
  LSan・依存ライブラリ全体・全platform・全旧UI同等性の合格を主張しない。

## 元の親の全義務と逆リンク

以下は元のIDと所有先に残り、チェックを付けない。

| ID | 保持する元の受入条件 | 所有先・残存範囲 |
|---|---|---|
| 29.002 | 名前・preview・選択状態が一致 | tasks.md の29.002、既存子29.002/add-timeout-owner、および旧UI completion correction契約。今回のicon失敗経路は29.008/icon-nullへ逆参照し、残りの表示/寿命/旧UI同等性を消さない。 |
| 29.008 | tool manager の現在選択と同期 | tasks.md の29.008と旧UI completion correction契約。今回のGtkImage fallbackは29.008/icon-nullへ逆参照し、全選択同期/旧action分類/旧UI同等性を消さない。 |

29.002の先行29.001（overlay配置）と29.008の先行29.007（color UI）も変更しない。
両親の受入条件は正常表示・全体同期を含み、この有限の失敗経路の前提より広い。
したがって当該leafだけを既に存在するnative counterpartへ結び直す。
全UI親・Gateを完了扱いにしない。進捗台帳の両親行にも逆リンクを追加する。

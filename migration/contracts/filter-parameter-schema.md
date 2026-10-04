# FilterLayer 引数取得・検証・編集・互換仕様

採用日: 2026-10-04。`16.002/parameter-schema-contract` の設計契約。
**本変更は文書と WBS のみ。名前 binder、schema editor、取得/cache の新動作は未実装・未検証であり、§9 の A〜E は全て TODO。** 既存実装の確認結果と今後満たす要件を区別する。

ソース照合基準は `seagetch/gimp-painter` の `gimp-3-0-port`、commit `5d12add0ecf46ea78c588714dc7dbb9e5efe66e2`、tree `d25e04e59ae6d6d2a535d12fb11420e3f629120d`。§12 の現行ソース・既存契約を照合した。実行試験、GLib 2.70 での build、新しい資源上限の実測はこの設計採用には含まない。

## 1. 採用する方針

1. **GIMP 3.0 の既存引数メタ情報を使い、同梱・承認済み手順だけを名前で binding する。** 固定スロットの反復記述を減らすが、任意 PDB 呼出し基盤にはしない。
2. **保存定義・編集用値・実行用値を分離する。** 旧保存順序と型を、新 PDB の順序・型へ書き換えて保存しない。
3. **現在のメタ情報と旧処理の意味は別の契約とする。** 型・範囲の自動取得だけで、旧別名、BOOL の整数表現、行列、旧 float 演算等の互換性は決まらない。
4. **開く、Cancel、変更なしの OK は完全な no-op とする。** その操作を原因として、型、bit pattern、未知項目、raw、Undo、定義世代、実行回数を変えない。既に実行中の処理が独立に完了することまで禁止するものではない。
5. **通常値の共通 editor と、行列などの専用 editor を併用する。** 現在受入済みの lossless entry、signed flag、未知 enum 表示を退行させない。
6. **最初の実装は helper 内 binder のみ。** 保存、wire、GUI、scheduler の同時変更を避け、段階ごとに採否を検証する。

[移植方針](../design.md) §5、§7.1、§8.4.1 の制御・配置契約を維持する。独立 FilterLayer、下層依存 scheduler、世代別取消し・進捗、完成済み buffer の GEGL source への供給を維持する。全フィルターを GEGL Operator に書き直す作業ではなく、GEGL effect への置換でもない。初期4経路への分割はこの共通化の着手範囲であり、[WBS](../../tasks.md) の全移植範囲や未対応経路・精度・platform の到達条件を削減しない。

## 2. API 境界と版の前提

| 実装位置 | 取得するもの | 使用方針 |
|---|---|---|
| core/helper `app/pdb` | `procedure->args` / `num_args` が `GParamSpec` 群。core の `gimp_procedure_get_arguments(procedure)` は default 入り `GimpValueArray*` | 今回の binder はこの境界に実装する |
| plug-in `libgimp` | `gimp_procedure_get_arguments(procedure, &n)` は借用 `GParamSpec**`。`create_config()` は `GimpProcedureConfig` | 同梱 plugin 内の既存 Config を維持する |
| plug-in UI `libgimpui` | `GimpProcedureDialog` は ProcedureConfig 用の UI | core の独自 layer dialog にそのまま持ち込まない |
| DrawableFilter / GEGL effect | `DrawableFilter.get_config()` は既存 GEGL effect の設定 | 独立 FilterLayer の保存モデル・scheduler の代用にしない |

同名の core/libgimp API は引数と返却型が異なる。両方を一つの汎用ラッパーにキャストして統合しない。型固有の adapter は所属モジュールに置き、`app/painter` に core/GTK/libgimp 依存を追加しない。

公開資料の `/api/3.0/` は API series の表示で、現在のページには Library Version 3.2.x も含まれる。`gimp_gegl_operation_get_pspecs()` のような 3.2 追加 API は、固定した GIMP 3.0.9 development base では使用しない。3.2 対応は別の baseline 更新・build・意味比較 gate とし、本作業の前提にしない。

また、この base の GLib 最小要求は **2.70.0**。`g_param_value_is_valid()` は 2.74 追加なので必須依存にしない。以下の作業コピー検証は 2.70 で成立させ、実装段階で最低版 build を検証する。現環境の GLib 2.84.4 での既存結果を最低版の試験済み証拠とはしない。

## 3. 最小データモデル

### 3.1 既存保存定義を authoritative に保つ

既存の procedure 名、typed arguments、original raw、opaque metadata、参照 descriptor、cache/definition 世代を維持する。`FilterArguments` の immutable snapshot と weak reference を利用し、別の保存用 Config や二重所有ストアを作らない。

スキーマの「正規化」は取得元の違いを editor/binder が読める形に揃える意味であり、保存値の正規化ではない。nullable と missing、null array と empty array、unconverted model と空 model を区別する。

### 3.2 読み取り専用 `FilterParameterSchema`（仮称）

手順ごとに以下を持つ。初期対象は Blinds / Small Tiles / Retinex / Convolution の既存隔離経路。独自 kernel 経路は現在の互換契約を schema として明示できる範囲だけ後から接続する。

| 項目 | 内容・規則 |
|---|---|
| route identity | 保存 literal 名、実行 route enum、現行 PDB 名、互換 adapter ID / revision を分ける |
| provenance | bundled provider/build identity、PDB 登録世代、runtime signature fingerprint。XCF の文字列を実行先 identity として信用しない |
| parameter key | canonical property name と runtime slot。表示名/翻訳名は key にしない。重複・空 name は拒否 |
| role | `context` / `input` / `auxiliary` / `output`。run-mode/image/drawables は実行 context が供給し、ユーザー値として再利用しない |
| type | GParamSpec subtype、ローカル GType、wire に出す場合は固定 allowlisted type token。整数の符号・幅、float32/binary64、配列要素型を区別 |
| constraints | typed min/max/default、enum value/nick、choice nick/ID、flags mask、null policy、文字列 policy、配列長関係。取得できない制約は `unknown` として表す |
| array shape | element type / byte alignment / explicit expected count / count-source。25 要素など処理固有の長さは adapter 制約で補う。PDB metadata に無い長さを推測しない |
| ownership | borrowed pspec の所有者、コピーした default の destructor、boxed copy policy、参照 descriptor の解決期限。opaque pointer の汎用複写はしない |
| presentation | nick/blurb、単位、editor hint、読み取り専用理由。UI hint は検証や実行許可の根拠にはしない |
| compatibility overlay | 保存 slot→意味上の field→現在 name の明示対応、旧範囲/変換/ignored 項目/画像条件。runtime schema と混合して由来を消さない |

`GParamSpec` と enum/choice class の借用は runtime owner の生存範囲内。owner thread 外へは bounded な plain immutable descriptor だけ渡す。数値 GType ID、GParamSpec pointer、画像/描画対象の GObject pointer は process 間に送らない。

`GimpCoreObjectArray` の boxed copy は container だけを複製し、要素の `GObject` を ref しない。従って `GValue`/Config をコピーしても drawables の要素寿命は保証されない。Phase A では、各要素を container から独立して生存させる強参照を、検証・execute・plugin 終了確認・必要な出力回収が済むまで保持する。現行 `run_convolution()` / `run_filter_procedure()` の image/layer `ObjectRef` は既にこの期間を覆うため、それを維持するか同等の明示的 lease で保証すればよい。冗長な参照管理基盤の追加は要求しない。成功・失敗・取消しの各経路で container と強参照をそれぞれ正しく解放する。保存モデルの weak descriptor はこの一時実行 lease と別物であり、強参照を保存引数に埋め戻さない。

### 3.3 auxiliary は取得できる範囲を明示

libgimp の `ProcedureConfig` は通常引数と auxiliary を含む。一方、固定ソースの `_gimp_procedure_add_aux_argument()` は auxiliary が PDB には公開されず、`run()` の引数でもないと明記する。core metadata で見えないものを「無し」と断定しない。

初期 binder は **PDB 公開 input だけ**を扱う。aux が実行意味に必要な route は、同梱 provider の明示 descriptor/export と Config 初期化規則を別途定義するまでは実行非対応。aux を input 配列に勝手に追加したり、last-used config を読み込んだりしない。UI 専用の一時値は文書の filter 引数と区別する。

## 4. 名前 binder と検証パイプライン

### 4.1 名前 binder

1. 保存 literal と定義形を現在の allowlist で解決する。未知名・未知形は preserve-only。
2. 既存の trusted bundled path 解決で helper の provider を取得し、file/type/登録数/公開属性等の現行検査を維持する。
3. `procedure->args` から name→slot を作る。期待 name/type/role の集合と独立した制約 policy を照合する。追加・欠落・重複・未知必須引数は incompatible とする。GIMP 3 image-procedure ABI の context prefix は `run-mode` / `image` / `drawables` が slot 0/1/2 にあることも照合する。
4. core の `get_arguments()` で一時配列を作り、**name を使って**型付き setter で埋める。default があることを理由に必須値の設定漏れを許さず、assigned bitset で全 input/context を確認する。
5. 検証済み一時配列を既存の `gimp_procedure_execute()` へ渡す。戻り値/status/error は現行契約通り検査する。

context prefix 0/1/2 を維持し、その後の route 固有 input の順序だけが変わり、同一 name/type/制約集合・provider identity が成立する場合は name binder で扱える。context prefix の並替えや任意 rename/type/range 変更は受け入れない。`gimp_image_procedure_run()` は最初の3値を位置で取り出し、`gimp_image_procedure_create_config()` も `ARG_OFFSET=3` を使う。core の menu-path 検査にも位置依存があるため、全引数の任意 permutation は安全な API 利用ではない。context も setter は name map を使い、固定 prefix を実行前に検証する。旧保存 slot は旧 format の定義なので位置対応を残す。排除するのは新 PDB 側の route input を固定 index で記述する重複であり、context の位置 ABI や旧 wire の index 自体ではない。

Phase A で route input の順序変更を受け入れるため、setter だけでなく、現行 `validate_blinds()` / `validate_small_tiles()` / `validate_retinex()` / `validate_convolution()` の `names[i]` / `types[i]` / `args[index]` に基づく署名・制約検査も同じ name map に移す。route input の順序依存 validator を残したまま reorder 対応済みとしない。context prefix の固定位置検査は image-procedure ABI として残す。name ごとの期待型・default・範囲・choice/role と provider 制約は引き続き厳密照合し、旧保存順や対象外の内部手順の署名を緩和しない。

### 4.2 三層の検証

| 層 | 検証対象 | 失敗時 |
|---|---|---|
| 保存/復元 | wire 形、長さ、型、null、原記録、参照 descriptor、既存容量制限 | 読める不明情報は opaque 保持。元 raw/cache を失わない |
| 編集 | 変更入力の構文・型・範囲、変えた値に依存する関係制約 | 対象欄に理由を表示。dialog を閉じず、model/Undo/cache を変更しない |
| 実行 | adapter の旧意味、runtime pspec、配列長、対象 image/ROI/precision、参照寿命、resource admission | 未実行/失敗と診断。前の確定 cache と定義を保持し、無限自動 retry しない |

`g_param_value_validate()` は値を変更し得るため、保存値や editor の保持値には直接適用しない。exact type、長さ・割当予算、型ごとのコピー/寿命規則を確認して作業コピーを作り、検証前後を**許可済み型に限定した exact comparator**で比較する。戻り値が `TRUE`、または戻り値にかかわらずコピーに変更があれば **入力不正として拒否**する。`FALSE` と前後不変の両方を満たしても、下記の意味・参照・資源検査は省略しない。丸め値/置換 default は採用しない。

戻り値だけの判定は不可。固定ソースの `gimp_param_core_object_array_validate()` は、non-NULL の空配列を NULL に書き換えた後に `FALSE` を返す。比較では型、NULL/empty、整数、float/double bits、文字列の null/内容、数値配列の長さと全要素bits、object の identity、object配列の全要素identityと順序を検査する。`g_param_values_cmp()` は代用不可であり、固定ソースの数値配列 comparator は長さだけ、core-object-array comparator は loop 内で先頭要素だけを比較する。Phase A は既知4経路の型だけを対象とし、未知 boxed の汎用deep-copyや任意GObject内部の比較へ広げない。

さらに明示的に次を検査する。

- `g_value_transform()` や文字列→数値、float→int の暗黙変換を禁止。意味変換は版付き adapter のみ
- 整数は対応幅の整数型で parse/比較し、double を経由しない。enum/choice は stable value/nick を照合
- float/double は**入力文字列を保存対象の declared type へ parse する時**の NaN/Infinity、overflow、underflow-to-zero を拒否する。declared type で表現できる有限 subnormal は保持する。これは既存互換 adapter の明示的な double→legacy-float narrowing を追加禁止する規則ではない。tiny finite double が旧演算用floatで0になる場合も、実行時の可否は既存契約・旧oracleによる個別policyに従い、generic validatorで勝手に厳格化しない
- 配列は byte 長が要素幅の倍数か、count×element-size の overflow、null/data の整合、期待 count を確認してから読む
- `divisor != 0`、legacy float narrowing、image precision/ROI、失効参照などは pspec では完結しないため既存 adapter/executor 検査を残す
- GIMP の独自 validation flags が緩和を指定していても、境界・有限性・allowlist・資源検査は省略しない

保存できる値と実行できる値は同じ集合ではない。既存の未知 enum や NaN bit pattern を no-op で拒絶・修復しない。認識済み形の一部を編集する場合、未変更の不明値は保ち、実行不能理由を表示する。新規作成/明示手順置換では全候補を検証する。形自体が不明な定義の部分編集は認めず「保存定義を保持」のままにする。

## 5. 明示 adapter が必要なケース

| 対象 | 保存意味と現行への変換 |
|---|---|
| Edge | 5/6引数形と amount の float/double を保持。5引数形の edge mode=0 はこの adapter 固有の既存規則であり、未知手順の欠落引数へ一般化しない |
| Gaussian aliases | `plug-in-gauss` / `-iir` / `-rle` / `-iir2` / `-rle2` の形・半径・方向 flag・method を別 mapping として維持。負半径、両軸 off、identity の現行受入結果を generic range で消さない |
| float と double | 元 wire の float32 bits、保存 model の G_TYPE_FLOAT/DOUBLE と、旧演算時 narrowing を区別。現在の double default で旧 float の情報を上書きしない |
| legacy Boolean | signed INT32 の非ゼロを true と解釈する旧 flag は、未変更なら元整数を保存。意味を変更した欄だけ 0/1 にする |
| Blinds | direction `1` のみ vertical、他整数は horizontal。保存透明 flag は非ゼロ true。旧 segments `1..100` と現行 public metadata `1..1024` を区別 |
| Small Tiles | 保存 `plug-in-small-tiles` を hidden `plug-in-painter-small-tiles` に実行時解決。旧/hidden `0..6` と public `2..6` を混同しない |
| Retinex | 保存 `plug-in-retinex` を hidden `plug-in-painter-retinex` へ解決。scale の旧/hidden 上限 256、public 上限250との差を維持。distribution INT→choice nick、cvar 旧 float 演算は明示変換 |
| Convolution | 旧11/12 slots→hidden9 inputs。counts25/5を検査し matrix/channels を名前で渡す。第12値は任意の既存 typed 値を保持し実行には渡さない。x-major `x*5+y`、byte 単位 offset、legacy の無効 alpha weighting と alpha 無し時 Extend を維持 |
| context IDs | 保存 run-mode/image/drawable の古い値は provenance として保持。現在の helper の非対話 mode と私有 image/drawable を実行時に設定。保存 ID で別画像を探索しない |
| aux | 見えない aux を推定復元しない。入力の省略と UI 用 auxiliary を同じ default 埋めにしない |

adapter ID/revision は trusted code の定義である。XCF に識別情報が保存される場合も自己申告だけで route を認可しない。将来の adapter revision が同じ保存値に別の結果を出す場合、既存契約を黙って差し替えず移行 gate を作る。

現時点では保存形式に schema/cache を追加しない。識別に曖昧さが無い既存 route は保存 literal＋型/形＋既存 provenance から解決する。将来、新しい意味を文書に保存する必要が出た時だけ versioned semantic field を設計し、旧 reader の未知 field 保持・書換え拒否規約まで試験する。

## 6. UI と lossless 編集

### 6.1 表示・編集

- schema 取得前にも既存定義の名前・型・状態と保持 preview は表示する。取得失敗で既存定義を default 化しない
- context fields は通常非表示。詳細表示しても実行用 ID は編集可能にしない
- integer entry は幅に応じた decimal 文字列。float/double は round-trip 可能な ASCII 表示を使い、科学表記を許す。GtkSpinButton の display round/clamp を保存経路に使わない
- boolean は checkbox、enum/choice は named combo。ただし unknown value を「保存値: ...」として表示し、未選択 default に置き換えない
- nullable string は NULL と空文字を別操作/状態にする。invalid UTF-8 と embedded data は勝手に置換して保存せず bounded read-only preview
- matrix は現行5×5 UIを維持し、表示 row/column と旧 x-major index を明示対応させる。channels は5個の意味付き checkbox。無効 alpha weighting は有効な option として宣伝しない
- generic property widget は値域・型・ownership が適合し、no-op 保持が実証できた小さな型に限る。core から ProcedureDialog を起動して plugin interactive/last-values lifecycle に入らない
- 重い live preview は初期 scope 外。変更確定後の既存 scheduler 更新を使う

### 6.2 dirty 判定と transaction

dialog は開始時の definition revision、immutable snapshot、schema generation、初期表示、field ごとの typed 値を持つ。changed field だけを patch し、1回の既存 definition transaction/Undo にまとめる。

exact equality は型、null、整数/enum値、浮動小数 bit pattern（`+0`/`-0` を区別）、配列長/全要素、参照 descriptor、raw を比較する。`g_param_values_cmp()`、pspec の近似/長さだけの比較、表示文字列の一致を代用にしない。文字列表記だけ変更して同じ typed bits に戻った場合、変更なしとする。

legacy checkbox を toggle して元の意味に戻した場合も元 signed integer を維持する。未知 enum、ignored tail、未変更の配列要素を reconstruction で捨てない。Reset は明示操作で、その route の**文書用互換 default**を設定する。現行 public plugin の default と区別する。

OK 時に definition/schema/image 寿命を再検査する。別 dialog・Undo・外部編集・schema 更新があったら stale とし、最新値の再読込を求める。古い状態で新しい定義を上書きしない。Cancel/Close は schema request と binding を解除し、未確定入力を破棄する。

## 7. 取得・cache・寿命・資源

### 7.1 最小先行実装

最初の binder は **既存 worker/helper の初期化中に取得済み core metadata を利用**する。query/execute は現在も helper 内で行うため、owner/UI thread に新しい plugin query を入れない。この段階では GUI の schema 通信、wire 改版、永続 cache は不要。

### 7.2 runtime schema を UI に返す段階

UI の runtime metadata 取得方法は Phase C で次の順に比較して決める。**describe-only IPC の新設は必須ではなく、Phase A には含めない。**

1. owner PDB に対象が既に登録済みで、trusted provider と必要な hidden/public signature を確認できる場合、procedure/pspec の有効期間を確保し、owner thread で仕事量を制限して metadata を借用・snapshot する。このために新たな plugin query、scan、同期child wait、未検証providerの探索は行わない。
2. 上記の条件を満たせず、UI に runtime metadata を渡す必要がある場合だけ、同じ allowlisted route enum と trusted helper に bounded な describe-only request を追加する。任意 procedure 名・path・script・plugin scan を受け取らない。query/待機は owner thread 外で行い、owner側のdescriptor処理も有界にする。追加する場合は専用versioned control envelopeまたは明示的wire改版を定義し、現行GPF6に未知messageを紛れ込ませない。

以下は採用した取得/cache方式に必要な契約であり、不要な protocol や cache framework を先に作る要求ではない。

- 状態は loading/ready/unavailable/incompatible/cancelled。取得中は Cancel と「保存定義を保持」を使える。新規値の Apply は ready まで無効
- helper取得を採用する場合は独立 worker/child で実行し、結果は bounded descriptor と request token のみ。owner の GObject は渡さない。登録済みmetadataを借用する場合はownerの有効期間とgenerationをその場で検査する
- token は dialog generation、route、definition revision、provider/registry generation。close、route 切替、画像終了、新要求のいずれでも旧返信を破棄
- schema query の進捗は indeterminate「設定情報を取得中」。filter 計算の generation progress/cancel と混同しない。query 取消しだけで既に走る計算を誤取消ししない
- child hang/crash、protocol 不正、path/metadata mismatch は既存 process 境界で終端化。deadline は trusted process policy に置き、保存値から変更不可。初期上限30秒を候補とし、実測で短縮する
- session cache を設ける場合は route/provider identity＋登録 epoch＋signature＋adapter revision で key 化。表示翻訳は別 key。name だけの cache は禁止
- 再登録/削除、provider identity 変更、compatible metadata 更新、helper 再起動で失効。取れない変更通知に依存せず、**各実行時にも helper が現在の signature を照合**する
- owner 内 metadata が同じ名前で置換された場合も pointer の再利用を同一性としない。古い immutable descriptor は参照中だけ生存し、新実行に再利用しない
- incompatible になっても既存確定画像を消さない。既存 cache を「現在の再計算成功」と誤表示しない。provider 変更を検出した進行中 job は再検証/破棄し、silent fallback はしない

ホットプラグ対応の汎用 registry framework は作らない。初期対象は同梱固定4経路で、process ごとの signature 検査が最終 authority になる。

### 7.3 既存制限と追加予算の設計候補

以下で「既存」とした制限は現行契約を維持する。新しい UI metadata / preview 全体 / materialization / query の数値は実装・測定前の候補であり、達成済みの上限保証ではない。Phase C で方式の必要性と実測を確認して確定する。資源上限は「編集/実行 admission」と「読める既存記録の保存」を分離する。巨大・未知引数を UI に出せないために、既存の multipart 保存や raw 保持を破壊してはならない。

- core importer の既存 depth32、aggregate65,536 slots/references を維持
- UI用metadata snapshot は候補64 KiB、最大512 specs。describe通信を採用する場合も同じ上限とし、文字列・choice数・整数加算を parse 前に検査する。超過時は preserve-only
- UI preview の追加候補は全体64 KiB、1文字列4 KiB。配列最大256要素と raw preview4 KiB/argument preview512件の既存表示制限を維持する。UTF-8 境界と省略表示を守る
- 大きな string/STRV/boxed を `g_strdup_value_contents()` で丸ごと作ってから切り詰めない。長さ付き borrow/iterator で preview し、raw byte数と表示省略を区別する
- 初期編集用の追加 materialization 予算候補は1 MiB。既知4経路は小さな scalar/25+5要素で収まる。未知/巨大配列を開くだけで深い copy をしない。超過項目は読み取り専用で保存保持し、汎用巨大 array editor は作らない
- query queue/cache を設ける場合は route 数4に制限し、同一路線の重複要求はまとめる。Cancel/Close 後に hidden job や無制限 cache を残さない
- 128 KiB pixel transport、既存 memory/spill admission、process cleanup の範囲は変更しない。UI追加予算は total RSS 保証ではない

## 8. XCF・旧文書互換

[xcf-painter-v1.md](xcf-painter-v1.md) と [xcf-multipart-transport.md](xcf-multipart-transport.md) が保存形式の authority。schema/Config はその代替ではない。

1. 元 `PROP_FILTER_SPEC` の raw と、現在の typed arguments は別に保持する。編集した typed 値で raw の歴史的記録を再生成しない。
2. 認識できない procedure/shape/type/version は preserve-only。一般 PDB lookup→execute、eval、file procedure、外部 image 再検索をしない。
3. 旧 writer が保存しなかった matrix/channel配列・参照等は推測復元しない。読める部分と実行可能性を分けて説明する。
4. 未知 namespace/version/duplicate key の保持または書換え拒否、保存前 preflight、destination 原子的置換の既存規則を維持する。
5. Save 中は確定定義と committed cache を snapshot し、入力途中 UI、query結果、未完成 worker pixel、PDB default を紛れ込ませない。
6. Save→Close→Open→再編集、Duplicate、Undo/Redo で unknown field、null/empty、signed flag、float bits、11/12 slots、ignored tail、失効参照を保持する。

「開ける」「保存できる」「編集できる」「現行で再実行できる」「旧画素と一致する」は別の結果として報告する。schema 適合だけで画像演算の互換性まで合格にしない。

## 9. 段階別 WBS

[WBS](../../tasks.md) に設計採用 `16.002/parameter-schema-contract`（DONE）と以下の5子タスク（TODO）を登録する。設計子の完了はソース照合と契約・依存の確定のみ。既存 parent、全移植、実装受入の完了を意味しない。

| 段階 / ID | 変更対象 | 完了条件 | 必須先行 ID |
|---|---|---|---|
| A `16.002/parameter-name-binder` | helper 内の4経路だけ。name→slot / type / assigned と型付き setter を小さな共通部品へ移し、既存署名・制約検査を name で照合 | context prefix 0/1/2 を固定した input reorder、missing/duplicate/type/constraint 不正拒否、コピー検証の FALSE mutation 検知、既存 object 強参照寿命、4経路 pixel/status/progress/cancel regression。GUI・wire・保存・scheduler 無変更 | `16.002/parameter-schema-contract`, `16.003/isolated-blinds-route`, `16.003/isolated-small-tiles-route`, `16.003/isolated-retinex-route`, `16.003/isolated-convolution-route`, `15.006/native-owner-progress` |
| B `16.010/parameter-semantic-policy` | runtime schema と旧意味 adapter を分離。現在の range/default/alias/count/flag policy の重複を整理 | コピー validation、型別 exact 比較、境界/配列/float/error tests。旧 corpus 不変。GLib 2.70 / GIMP 3.0 build | `16.002/parameter-name-binder` |
| C `30.001/parameter-schema-editor` | 既存 field を schema keys に接続。共通 scalar entry・専用 matrix・bounded preview。trusted 既登録 metadata の bounded 借用を先に比較し、必要な場合だけ describe IPC/cache | no-op 完全一致、stale 拒否、取得寿命/取消し、64bit 精度、未知保持、native GTK 操作。通信/cache を加える場合だけ版・不正入力・失効試験。追加予算を実測確定 | `16.010/parameter-semantic-policy`, `30.001/isolated-filter-editors`, `15.006/native-owner-progress` |
| D `12.015/parameter-schema-roundtrip` | 保存仕様を変えず統合の保持試験を追加。拡張の必要性が判明した場合だけ別の versioned 設計 | ordinary Save/Open/再編集、Duplicate/Undo、巨大 opaque、実行中 Save、失敗時元 file 保持 | `30.001/parameter-schema-editor`, `12.015/multipart-storage` |
| E `16.023/parameter-schema-acceptance` | focused sanitizer、実アプリ、再配置 runtime、source/binary hash 付き証跡 | 新旧 pixel corpus、4経路 native 実行、実 editor/取消/終了/再読込、OOM/malformed/schema drift。未検証 platform・全 port 残課題を明記 | `12.015/parameter-schema-roundtrip` |

最初の実装は **A のみ**。B 以降は A の重複削減と境界検査を実証してから進める。新しい汎用 plugin framework、全アルゴリズム再実装、baseline 3.2 化、新しい動的 GObject 型生成、永続 schema DB、汎用巨大配列 editor はこの段階化には含めない。未対応フィルターを含む移植全体の要件と既存の未完了 gate は維持する。

独立した GTK AT-SPI の依存欠陥 `34.003/gtk-atk-menu-guards` は未完了のまま。A〜E の合格で修正済みにしない。[原因と残作業](../tests/gtk-dialog-diagnostics/README.md) に従い、通常入力の結果と accessibility 経路の未解決事項を分けて報告する。

## 10. 実装時に実施する受入テスト

| ID | 刺激 | 合格条件 |
|---|---|---|
| P01 | context prefix 0/1/2 を固定し、以後の route input だけを同じ名前集合の別順で登録。署名/制約 validator から実行まで通す | name ごとの型/範囲/default を検査し、値は正しい name へ入る。route input の順序依存 validator を残さず、結果と旧保存順は不変 |
| P02 | context prefix の並替え、missing/duplicate/extra/name-change/type-change/default/range変更 | policyで許していない差分をincompatibleとして拒否。古いcache保持、任意実行なし |
| P03 | INT32境界、将来対応時のINT64/UINT64境界・2^53±1 | parse/表示/比較/保存でdouble経由の丸め無し |
| P04 | declared型への文字列parseのoverflow/underflow、double nextafter/±0/subnormal、tiny doubleの旧float narrowing | declared型parse不正は拒否。表現可能値はexact保存し、旧floatで0となる実行は既存oracle/policy通り。NaN/Infの新規入力拒否と保存特殊値no-op保持を分離 |
| P05 | 範囲外値とnon-NULL空CoreObjectArrayをpspec validateに渡す | 戻りTRUEまたはtyped before/after差分で拒否。空配列→NULLでFALSEを返す実validatorも検知。original/UI/model不変、silent clamp/NULL化無し |
| P06 | 配列null/empty/count mismatch/partial element/overflow/巨大length | 読込み前検査、OOM回避。定義を勝手にemptyやidentityにしない |
| P07 | 旧flag -7、direction2、unknown enum、Convolution第12値 | no-op/toggle-return/1field editで未変更値とtail保持 |
| P08 | Small Tiles0/1、Retinex256、Blinds100/101、Gaussian負半径/両flag off | 公開現行範囲と互換範囲を混同せず、既存意味と実行可否を再現 |
| P09 | XCFのscript/eval/file名、path風文字列、不正route enum | provider/query/execute先に使われず、preserve-only |
| P10 | schema query中Close/Cancel/route切替/画像終了、結果到着中再入 | stale callback無視、解放後アクセス/二重release/無期限child無し |
| P11 | provider再登録/削除、同名別signature、実行前差替え | cache失効と実行時再検証。保存定義不変、古い結果を新しい成功にしない |
| P12 | editor中の別編集/Undo/Redo、同時dialog | stale OKが最新定義を上書きしない |
| P13 | 変更なしOK、Cancel、textだけ等値変更、flag往復 | settled条件ではdefinition/raw/typed/cache世代/Undo depth/run countが不変。実行中条件では独立した完了以外の再起動や変更が無い |
| P14 | 全4editorの1field編集、matrix1要素、未知他field付き | 変更したfieldだけ変わり、1Undo。画像制約/失敗理由を表示 |
| P15 | Save/Open/再編集、Duplicate、Undo/Redo、処理中Save | 型・値・原記録・無効参照・cache lineageの既存契約通り |
| P16 | 大string/STRV/array/raw、choice過多、query返答過多、OOM注入 | 採用して実測確定した preview/descriptor/queue/cache の上限遵守、保存保持とresourceエラーを区別 |
| P17 | 完了/取消し/child crash/timeout/遅延progress | 現行generation progress、cancel、EOF/reap/cleanup、前cache保持に退行なし |
| P18 | 最新実装でfocused ASan/UBSan、実app key/menu操作、relocation | 各source/binary hash/command/exit/raw log保存。known GTK診断と新規不具合を分離 |
| P19 | 同長・内容違いの数値配列、同じ先頭/違う後続object配列 | 全要素のbits/identity/順序で差を検知。pspec comparatorの結果を流用しない |
| P20 | CoreObjectArrayをGValueにコピー後、元containerと外部参照を解放。正常/失敗/取消しを通す | container から独立した既存 ObjectRef または同等 lease により全要素が検証・実行・必要な完了処理まで生存。image 等の他 owner も解放した条件で最後の lease 解放後の弱 finalize 通知を確認し、漏れ/二重解放/UAF 無し |
| P21 | trusted登録済みmetadataが利用可能な場合と、未登録/不一致の場合 | 前者で新規query/IPC無しのbounded借用が可能。後者はowner同期queryせず、採用した非同期経路またはunavailableへ。hidden/publicのすり替え無し |

性能はまず正確な観測値を残す。query を owner thread で行わない構造を必須とし、GTK heartbeat、最大callback時間、preview追加割当、child/cache/FD数を旧版と比較する。共有hostの単発時間を全platform保証や固定p99 gateの代用にしない。

## 11. 採用理由と残す判断

- **name binder を先行**: 実APIは既に使われているため、新しい呼出し系を増やすより、重複したslot/type確認を小さく共通化する効果が大きい。
- **メタ情報＋互換overlay**: 同じfilter名でもpublic/hiddenの範囲や旧演算の意味が違う。metadataだけの自動移行は保存値と再現性を壊す。
- **保存にschemaを加えない**: 今回の初期4経路は既存literal/型/形で識別可能。runtimeの派生情報を保存authorityにするとversion driftと二重管理が増える。
- **no-opを保存表現で比較**: 表示丸めやBOOLのcanonical化は、再実行して見た目が同じでも保存互換の違反になる。
- **UIを全面自動生成しない**: 行列の並び、単位、ignored項目、precision制約はsemanticであり、pspecだけでは適切な編集体験にならない。
- **3.0維持**: 現APIで必要な取得とbindingはできる。新API名だけを理由に対象baselineを更新しない。

実装時に決める項目は、新部品のファイル名、UIの既登録metadata借用とdescribe-only IPCの比較・必要性、cacheを持つ必要性、query を導入した場合の30秒 deadline、64 KiB/512 specs metadata、64 KiB 全体 preview と1 MiB editor 予算候補の実測調整である。これらは保存互換やprovider許可範囲を広げる理由にはしない。新しい semantic route/aux を加える場合は別途、旧実行のreferenceとimage-level受入を必須とする。

## 12. 根拠

### 照合した現行ソース・既存契約

行番号は冒頭 commit のもの。以後は併記した関数名で再照合する。

| 根拠 | 確認した契約 |
|---|---|
| [core `gimp_procedure_get_arguments()`](../../app/pdb/gimpprocedure.c#L620)、[宣言](../../app/pdb/gimpprocedure.h#L150) | core の pspec 群から default 入り `GimpValueArray*` を作る |
| [libgimp `gimp_procedure_get_arguments()`](../../libgimp/gimpprocedure.c#L1646)、[`_gimp_procedure_add_aux_argument()`](../../libgimp/gimpprocedure.c#L1448)、[ProcedureConfig](../../libgimp/gimpprocedureconfig.c) | libgimp は借用 `GParamSpec**`。aux は Config に含むが PDB/run input には出ない |
| [`gimp_image_procedure_run()`](../../libgimp/gimpimageprocedure.c#L143)、[`gimp_image_procedure_create_config()`](../../libgimp/gimpimageprocedure.c#L202)、[`gimp_plug_in_procedure_add_menu_path()`](../../app/plug-in/gimppluginprocedure.c#L631) | context prefix は位置 ABI。image run は 0/1/2 を読み、Config は先頭3個を除く。menu の種類別検査にも位置条件がある |
| [`gimp_param_array_values_cmp()`](../../libgimpbase/gimpparamspecs.c#L614)、[`gimp_param_core_object_array_values_cmp()`](../../libgimpbase/gimpparamspecs.c#L1433) | 数値 array 比較は長さだけ、object array は loop 中も先頭だけ。exact equality に流用不可 |
| [`gimp_core_object_array_copy()`](../../libgimpbase/gimpparamspecs.c#L1312)、[`gimp_param_core_object_array_validate()`](../../libgimpbase/gimpparamspecs.c#L1402) | container-only copy。validator が空配列を NULL に変更して FALSE を返す場合がある |
| [helper `validate_blinds()` / `query_*` / `validate_*`](../../app/core/gimpfilterprocedure.cpp#L355)、[`run_convolution()`](../../app/core/gimpfilterprocedure.cpp#L778)、[`run_filter_procedure()`](../../app/core/gimpfilterprocedure.cpp#L929) | 現在は固定 index の署名/制約/setter。image/layer の独立した `ObjectRef` は execute、plugin 終了確認、出力回収まで保持 |
| [FilterLayer adapter](../../app/core/gimpfilterlayer.cpp#L502)、[FilterArguments](../../app/core/gimpfilterlayer-arguments.hpp#L89)、[lossless editor](../../app/dialogs/painter-layer-dialog.cpp#L872) | 型付き旧形、11/12 slots、weak 参照、depth/aggregate 制限、lossless entry、signed flags、no-op/stale 検査 |
| [Blinds](../../plug-ins/common/blinds.c#L152)、[Small Tiles](../../plug-ins/common/tile-small.c#L289)、[Retinex](../../plug-ins/common/contrast-retinex.c#L209)、[Convolution](../../plug-ins/common/convolution-matrix.c#L104) | public/hidden metadata と旧互換 adapter の範囲・意味を区別 |
| [GLib 最小版](../../meson.build#L473) | 宣言は 2.70.0。新実装の最低版 build は未実施 |

保存・実行・editor の authority は [移植方針](../design.md) §5/§7.1/§8.4.1 と以下の既存契約にある。

- [C++ foundation](cpp-foundation.md)、[FilterLayer port](filter-layer-port.md)
- [process bridge](filter-process-bridge.md)、[context](filter-context.md)、[Convolution](filter-convolution.md)
- [isolated editors](filter-isolated-editors.md)、[progress](filter-progress.md)
- [XCF v1](xcf-painter-v1.md)、[multipart transport](xcf-multipart-transport.md)

### 公式資料（2026-10-04確認）

- [libgimp Procedure.get_arguments](https://developer.gimp.org/api/3.0/libgimp/method.Procedure.get_arguments.html): 3.0追加、順序付きpspec配列はinstance所有
- [libgimp ProcedureConfig](https://developer.gimp.org/api/3.0/libgimp/class.ProcedureConfig.html): procedureの引数/auxと対応する設定object
- [libgimpui ProcedureDialog](https://developer.gimp.org/api/3.0/libgimpui/class.ProcedureDialog.html): pluginのprocedure/configを基にしたdialog
- [DrawableFilter.operation_get_pspecs](https://developer.gimp.org/api/3.0/libgimp/type_func.DrawableFilter.operation_get_pspecs.html): 3.2追加のGEGL操作metadata取得。固定3.0.9には採用しない
- [GObject.param_value_validate](https://docs.gtk.org/gobject/func.param_value_validate.html): 不適合値を変更し、変更が必要だったか返す
- [GObject.param_value_is_valid](https://docs.gtk.org/gobject/func.param_value_is_valid.html): 非変更照会は2.74追加

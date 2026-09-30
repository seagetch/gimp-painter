# 01.008 signal / idle / timeout 静的棚卸し

対象を旧差分947ファイル内の C/C++ 呼出しに固定した。`signal-call-spans.tsv` はコメントを除外し、呼出しの閉括弧まで追う614件。実際の追加行が引数を含む呼出し全体と交差する139件を `signal-span-contracts.tsv` に接続先・データ・解除証跡・優先度・再入リスク・移植先として記録した。残る475件は、開始行だけでなく呼出し全体に追加行がない文脈である。既存C台帳517件には文脈が含まれ、これを変更件数とは数えない。

Cの通常接続57件に加え、container handler登録2件・解除3件を確認した。複数行の引数のみの変更による、追加のC登録はなかった。`c-added-signal-contracts.tsv` と `signal-teardown-by-file.tsv` は終了経路を記録する。connect_objectの9件は監視GObjectの終了、closure接続2件は closure notifier の config/data 解放、通常接続は emitter終了または明示解除を使う。brush popupは以前のブラシではなく現在のブラシから接続を外す欠陥を持つ。palette popupはeditorのdisposeから閉じられず、context解除後に生のeditorを参照する。修正先はそれぞれ `24.015/brush-popup-signal-owner`、`29.007/palette-popup-teardown`。

C++の111件（signal107、source4）は `signal-source-inventory.tsv`。15件の明示接続と22件のwrapper接続はConnectionを保持する。34件はConnection返却を捨て、25件はconnect_noretでhandleがない。借用thisをemitterより先に破棄する順序は `07.008/untracked-signals` で修正・試験する。汎用helperは `delegators.hpp:121-191` のConnection/closure、`glib-cxx-utils.hpp:1034-1115` の転送を経由する。終了中callbackを保護する責務は `06.017` と `07.007-07.009` に残す。widgetに結び付くImplとclosureの所有者は `indirect-callback-owner-review.tsv`、CXXPointerの破棄は `cpp-scoped-owner-contracts.tsv` に対応する。

source4件はFilterLayerの300ms timeout（IDを捨てる）、LayerPreview idle（保持したEventSource）、tile scroll timeout、tile長押し500ms timeout。いずれも既定優先度。後者はcallback内で自身のownerを消すので、実行中delegatorの自己破棄を防ぐ必要がある。解除・再予約・close順は各行の修正先に追跡した。`EventSource` の登録・remove・destructorは `glib-cxx-utils.hpp:1185-1218`、destroy notifierはclosure/delegator所有台帳に記録済み。

`TRACED_STATIC` は解除経路または解除欠陥と移植先を記録した意味で、寿命安全や実機試験の合格ではない。全signalは同期発火中に選択切替・closeが再入し得る。台帳の優先度はsourceのスケジューリングとsignal同期発火を区別している。旧XCF等を含む実際の終了試験は後続タスクで行う。

再生成・照合: `audit_signal_call_spans.py` → `audit_signal_source_inventory.py` → `audit_c_added_signal_contracts.py` → `audit_signal_span_contracts.py`。対象行数614/139/111/57、全変更行の解除証跡と追跡先、WBS依存を検査する。

# 01.007/cpp-ownership raw pointer 対応の進捗

`cpp-raw-owner-coverage.tsv` は旧 C++ の raw pointer 候補 168 行を同じ順序で保持し、`MAPPED` は旧コードの所有者・通常の解放経路・移植先 WBS が記録されたこと、`PENDING` は個別の経路照合が残ることを示す。現時点で168行すべてが MAPPED。再生成スクリプトは候補に未対応行があれば失敗する。`MAPPED` は GIMP 3 への実装や異常経路の試験完了を意味しない。

今回は MyPaint ブラシ private 16 行、paint core 14 行、Surface 14 行の既存記録を統合し、FilterLayer、定規、HTTP、Undo、MyPaint 設定、preset の19行を追加した。FilterLayer の Rectangle は `updates` list が所有し、消化または reset 時に解放する。`ProcedureRunner* r` は `runner` への代入後は `CXXPointer` が所有するが、`set_arg` 中に例外が出ると代入前の局所 pointer を失う。REST factory の派生オブジェクトは `RESTResource::handle` の `delete this` で解放されるが、旧 `RESTResource` の destructor は virtual ではない。NavigationGuide の callback context は `g_new0` されるが、callback 内に対応する `g_free` がなく、`arg_conf` の削除も webhook URI があるときに限られる。これらは `16.019` と `31.008` の実装・異常経路テストで扱う。

共通基盤と HTTP の36行を追加した。Delegator は closure または CXXPointer が解放し、Decorator は widget data-full または steal 後の明示削除が所有する。二世代の router は Rule と TokenRule を保持するが、基底 `TokenRule` に仮想デストラクタがない。`Defferred` の callback に handler がないまま返る経路では chain の解除が見えない。Mapping の `new[]` と `delete` の不一致も表に結び付けた。

UI と表示の残り69行も追加した。overlay の多くは widget data-full が decorator を所有するが、`FgBgEditorDecorator` は生成後の保持と delete が見えない。HTTP factory は複数 route が同じ生ポインターを捕捉し、明示解放がない。MyPaint 設定 widget の弱参照解除漏れ、tile view の source callback が捕捉する生の `this` も表に残した。こうした旧実装の不備を GIMP 3 側へ引き継がず、対応する実装・検証タスクで修正する。

次は既存の hold/ref・GValue・CXXPointer の表とこの168行を突き合わせ、参照の取得と例外時 cleanup の契約を閉じてから `01.007/cpp-ownership` を DONE にする。

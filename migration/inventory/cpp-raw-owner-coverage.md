# 01.007/cpp-ownership raw pointer 対応の進捗

`cpp-raw-owner-coverage.tsv` は旧 C++ の raw pointer 候補 168 行を同じ順序で保持し、`MAPPED` は旧コードの所有者・通常の解放経路・移植先 WBS が記録されたこと、`PENDING` は個別の経路照合が残ることを示す。現時点で 99 行が MAPPED、69 行が PENDING。`MAPPED` は GIMP 3 への実装や異常経路の試験完了を意味しない。

今回は MyPaint ブラシ private 16 行、paint core 14 行、Surface 14 行の既存記録を統合し、FilterLayer、定規、HTTP、Undo、MyPaint 設定、preset の19行を追加した。FilterLayer の Rectangle は `updates` list が所有し、消化または reset 時に解放する。`ProcedureRunner* r` は `runner` への代入後は `CXXPointer` が所有するが、`set_arg` 中に例外が出ると代入前の局所 pointer を失う。REST factory の派生オブジェクトは `RESTResource::handle` の `delete this` で解放されるが、旧 `RESTResource` の destructor は virtual ではない。NavigationGuide の callback context は `g_new0` されるが、callback 内に対応する `g_free` がなく、`arg_conf` の削除も webhook URI があるときに限られる。これらは `16.019` と `31.008` の実装・異常経路テストで扱う。

共通基盤と HTTP の36行を追加した。Delegator は closure または CXXPointer が解放し、Decorator は widget data-full または steal 後の明示削除が所有する。二世代の router は Rule と TokenRule を保持するが、基底 `TokenRule` に仮想デストラクタがない。`Defferred` の callback に handler がないまま返る経路では chain の解除が見えない。Mapping の `new[]` と `delete` の不一致も表に結び付けた。

残る69行は WBS の新しい行を一件ずつ増やさず、同じ表に所有者と解放先を追記する。全168行の照合と既存の hold/ref・GValue・CXXPointer の表を突き合わせてから `01.007/cpp-ownership` を DONE にする。

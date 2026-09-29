# 01.007 CXXPointer 所有者と破棄経路

旧 tree `afa43fae` の `CXXPointer` 候補54件を `cpp-scoped-owner-contracts.tsv` で所有者、破棄関数、移植先WBSへ対応付けた。内訳は signal connection 38、C++ instance 9、delegator 3、idle/timeout source 3、helper 定義1。`python3 -B tools/audit_cpp_scoped_owner_contracts.py` が54箇所の重複と未対応を検査する。これは旧コードの静的契約であり、GIMP 3 の解放試験は移植先タスクに残す。

`Connection` は C++ owner から `delete` され、destructor が対象 GObject の signal を解除する。対象への強参照を持たないため、先に GObject が破棄される順序は `07.008` の実装・試験対象。signal 名の `g_strdup` 分の解放漏れは `07.008/connection-name` に記録済み。`ProcedureRunner` は `runner` の保有前に引数設定で例外が起こり得るため、`16.019` で確保直後の所有を確立する。

`EventSource` は source ID と delegator を所有し、非 disposable の destructor が `g_source_remove` する。`LayerPreview.update_idle` は再予約時に旧 source を取消し、widget 終了時はメンバー破棄で取消すが、lambda は `this` と `viewable` を生で捕捉する。`29.019/preview-idle-owner` と `29.020/preview-idle-teardown` に対象の寿命と先行破棄試験がある。

layer tile の長押し `add_timeout_handler` は callback 内から `add_timeout_handler = NULL` を実行し、実行中の `EventSource` とその delegator を破棄する経路がある。`g_source_remove` による destroy notify と自然終了が重なる順序も検証が必要。`scroll_timeout_handler` は drag 終了時に取消すが、callback は tile と scrolled window を参照する。`29.002/add-timeout-owner` で callback 実行中の所有を維持し、押下・離上・drag・終了の順序を試験する。

表中の追跡先は個々の C++ メンバーを GIMP 3 へ移すタスクを示す。検証未済の source target、closure、C++ lambda の寿命を完了扱いにしない。

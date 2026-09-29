# 01.007 旧 C++ の明示的な GObject 参照

旧 tree `afa43fae` の `g_object_ref/unref` 28 箇所について、保持先、参照を取得した場所、解放と条件分岐を `cpp-explicit-ref-contracts.tsv` に記録した。`python3 -B tools/audit_cpp_explicit_refs.py` は既存候補集合との差と重複を検出する。ここでの `TRACED` は旧コードの取得・解放契約を特定したという意味で、GIMP 3 実装済みという意味ではない。

overlay widget の ref/unref は `gtk_container_remove` を挟む一時参照。MyPaint options の brush は dirty 時に history へ所有権を移し、clean 時に unref する。popup container は factory から借用し、create 時に ref して destroy 時に unref する。tile pixbuf の縮小時は旧 pixbuf を解放して新しい pixbuf に置換する。

不備・危険経路は次の通り。

- `gimpmypaintcore-surface.cpp:415,436,454`: brushmark に対する `begin_use()` は Surface 破棄時に `end_use()` されない。同じ brushmark または texture を setter に渡すと旧参照を解放してから引数を使い直す。`21.002/resource-owner` で自分自身への設定、NULL 切替、先行破棄を検証する。
- `gimpcellrendererpopup.cpp:92,217`: `pixbuf` メンバーが constructor で初期化されず、最初の `if (pixbuf)` と destructor の分岐が未定義値を読む。生成失敗後の `get_size()` も NULL を使用する。`08.016/pixbuf-owner` で修正する。
- `gimplayertileview.cpp:155` と `gimptooltileview.cpp:139`: `gtk_widget_render_icon` が NULL の場合にも `g_object_unref(pixbuf)` を呼ぶ。`29.008/icon-null` で両方を試験する。

これで明示的な GObject 参照28件と返却参照 `hold` 12件の契約を追跡した。`GValue` 136件と `CXXPointer` 54件、残りの `hold` の採用箇所の例外・異常経路は引き続き `01.007/cpp-ownership` で照合する。

# 01.007/mypaint-brush-private

`mypaint-brush-raw-owners.tsv` は旧 `app/core/gimpmypaintbrush.cpp` の active raw 候補 16 箇所を所有者と解放経路に対応付ける。`GimpMypaintBrush.p` は GObject init (`:147`) で private を作り、finalize (`:151-161`) が削除する。duplicate (`:350-359`) は新オブジェクトの初期 private を削除して複製 private に交換する。private destructor (`:455-475`) は文字列、各 Mapping、Cairo surface を解放する。設定の setter (`:522-568`) は旧文字列を解放して入力を複製するため、入力が旧文字列と同じポインターなら解放後参照になる。

preview は `Brush*` と `GimpMypaintSurface*` を局所確保 (`:627-628`) し、通常経路では `:733-734` で削除する。途中の例外や後続移植で早期 return を追加する場合は scope owner が必要となる。`standard_mypaint_brush` の weak pointer (`:375`) は静的スロットに NULL を書く。`Mapping` の値コピーは `19.006/mapping-value-owner`、アイコン画素は `19.013/icon-buffer` に引き継ぐ。16 箇所の `DONE` は旧ソースの静的な所有権対応であり、GIMP 3 の再実装やランタイム保証ではない。

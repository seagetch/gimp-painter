# 拡張 MyPaint ブラシ資源の class callback

旧 `app/core/gimpmypaintbrush.cpp` の直接 class callback 15 件を、
旧 C++ 定義と旧 C slot の署名を突き合わせた上で、GIMP 3 の slot と照合した。
対応する定義行・型・後続タスクは
[`gimp3-brush-callback-review.tsv`](gimp3-brush-callback-review.tsv) にある。
旧定義は 15 件すべてで旧 slot と一致する。現行の宣言と同じ 9 件、
変更 2 件、独自型の再作成が必要な 4 件に分かれる。
`DONE` は静的な照合の完了を示し、実装・筆跡の検証は示さない。

| 境界 | 照合結果と移植時の条件 |
|---|---|
| `GimpViewableClass.get_new_preview` | 戻り値が `TempBuf*` から `GimpTempBuf*` へ変化し、`GeglColor*` を追加。`19.013/preview-contract` で色・サイズ・buffer 所有権を確認する |
| `GimpViewableClass.get_size` | 署名は同じだが、旧 `gimpmypaintbrush.cpp:211` は true を返す経路で width/height の代入がコメントアウトされている。成功なら出力 2 値を必ず設定する |
| `GimpDataClass.save` | 旧 `gimpmypaintbrush-save.cpp:101` の `(GimpData*, GError**)` から現行 `(GimpData*, GOutputStream*, GError**)` に変化。旧 writer はファイル名を取り出し `json_generator_to_file` で別途書き、成功・失敗を確認せず true を返す。現行 `gimp_data_save` は `GOutputStream` を開き、callback 後に close する。`19.011/output-stream-save` で渡された stream への書込みと失敗時の通知、アイコン保存との整合を確認する |
| 拡張ブラシ独自 slot | `begin_use`、`end_use`、`select_mypaint_brush`、`want_null_motion` は旧 `GimpMypaintBrushClass` にのみある。現行の `GimpMybrush` に同名 slot があるとは仮定せず、`08.005` で拡張型を作り、後続の描画・保存との接続を定義する |
| GObject / その他 | finalize、property、メモリー見積り、説明、dirty、extension、checksum などは C の slot 宣言上は一致する。親処理・所有権と callback から C++ 例外が漏れないことを実装時に確かめる |

GLib の外部 ABI は GNOME 公開ソースの
[2.32.4](https://raw.githubusercontent.com/GNOME/glib/2.32.4/gobject/gobject.h) と
[2.80.0](https://raw.githubusercontent.com/GNOME/glib/2.80.0/gobject/gobject.h)
を固定して照合した。比較用の版であり、ビルド環境の導入版を示さない。
実際の GIMP 3 の `GimpMybrush` は資源型の参考先だが、旧拡張設定と
カスタム slot が保存・復元されるかは機能タスクで別途検証する。

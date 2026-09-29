# 旧 tool options toolbar の参照経路

旧版 `afa43fae3e920210146abed514f136fd49f671b5` で、`app/tools/gimp-tools.c:403-407` は `toolbar_gui` に `g_object_ref_sink` した参照を `tool_options` の data-full に格納し、破棄 callback を `g_object_unref` にする。`app/widgets/gimptooloptionstoolbar.c:264,279` はこの widget を借用して表示を切り替える。

`gimptooloptionstoolbar.c:286-294` は親 container から取り外す直前に一時参照を取得し、次の container に追加した後に解除する。このペアは移動中の widget の寿命を守る。対して `:327-339` の `hide_toolbar` は子 widget ごとに `g_object_ref` してから container から取り外すが、当該関数で対応する `g_object_unref` がない。`tool_options` の data-full は別に参照を持っているため、移植時には余剰参照を増やさない所有モデルにし、表示・非表示を繰り返した後と終了時の finalization を検証する。

同じ C 差分の全参照操作がこれと同じ対称性を持つとは限らない。`01.007/c-state` は残りの 74 行の分岐を確認するまで継続する。

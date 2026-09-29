# 01.007/mypaint-core-raw

旧 `app/paint/gimpmypaintcore.cpp` の raw 寿命候補14件のうち9件は実際の確保・解放、5件 (`:129`, `:133`, `:135` のログ文字列、`:223` のコメント、`:239` の `#if 0` 内) は非 active。`GimpMypaintCore` は `surface`、`brush`、`stroke`、`option_changed_handler` を保持する。`cleanup` (`:82-104`) が接続を切り、stroke の session 終了後に stroke、brush、surface を削除して NULL にする。`stroke_to` は drawable が替わったとき surface を削除して再生成 (`:130-137`) し、新規 stroke を `:141` で保持する。`split_stroke` (`:208-222`) は stroke を停止・削除する。`update_resource` は brush を一度生成 (`:245`) し、cleanup で削除する。

旧 header `app/paint/gimpmypaintcore.hpp` は `GimpMypaintOptions* options` と `GimpMypaintBrush* mypaint_brush` をメンバーに持つが、constructor (`gimpmypaintcore.cpp:64-75`) は双方を初期化しない。`stroke_to` は最初の呼出しで `options != this->options` と比較し、結果次第で `this->options` も条件に読む (`:114-120`)。したがって未初期化値の読み取りと signal 接続の省略または誤判断が起こり得る。`08.006/core-options-init` で NULL 初期化と借用参照の先行破棄を扱う。`08.008/tool-core-lifetime` でも tool 側からの所有と cleanup 順を検証する。旧ソースの静的追跡であり、GIMP 3 の型移植と動作試験は未着手。

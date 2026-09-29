# MyPaint Mapping 配列の旧所有権

旧 `app/core/mypaintbrush-mapping.hpp:40-60` では `pointsList` を `new ControlPoints[inputs]` で確保するが、destructor と代入演算子では `delete pointsList` を呼ぶ。配列確保と単体解放の組合せは不正である。また、自作の代入演算子は存在する一方、コピー構築は宣言されていない。既定のコピー構築で `pointsList` が共有されると複数の `Mapping` が同じ配列を解放する。

GIMP 3 の曲線 decoder と brush 設定には、所有する点列を値として複製する設計を採用する。配列確保・解放の対称性、コピー/移動、空点列、再代入を確認する。旧版の未定義動作を再現する必要はない。

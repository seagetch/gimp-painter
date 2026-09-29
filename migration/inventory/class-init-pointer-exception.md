# 旧 class 初期化の pointer 例外

旧 `app/base/glib-cxx-impl.hpp:274-284` の `GClassWrapper::init` は、既存 `klass` と異なる `class_struct` を受けると `new InvalidClass` したポインターを `throw` する。旧コード内に対応する `InvalidClass*` の catch と delete は見つからない。`catch (...)` では型や参照先の情報が失われ、例外が C ABI の class init へ到達する経路もある。

移植先ではポインター例外を使わず、C++ 内で値として扱うか明示的なエラー結果を返す。GType/class init の C callback 境界では例外を捕捉し、未登録または一部登録済みの property 状態を検査して失敗を報告する。異なる class pointer を渡す試験と、その後の登録可能性を検証する。

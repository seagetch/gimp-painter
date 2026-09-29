# 01.007/router-tree 旧 HTTP router 規則木

旧 `app/base/route.hpp:35-164` の `Route` は `Rule*` を vector に保持し、`Route::~Route` が各 Rule を削除する。`Rule` は `Match`、`Name`、`Select` を `TokenRule*` の vector に確保し、`Rule::~Rule` が各要素と委譲 handler を削除する。`Select` の候補文字列配列は `g_strdupv` で複製し `~Select` が `g_strfreev` する。

新しい旧ソース `app/base/soup-cxx-utils.hpp:154-379` の `Soup::Router` も `Rule*` を `GLib::Array` に格納し、Router の destructor が各 Rule を削除する。Rule の `handler` は `CXXPointer<rule_delegator>` に格納され、Rule の破棄で `delete` される。子規則は `_rules` に格納し、`Rule::~Rule` が要素を削除する。`Match::token`、`Name::name`、`Select::name/candidates` は GLib wrapper に格納される。

両版の `TokenRule` には仮想 `test` があるが仮想デストラクタがない。`TokenRule*` 経由で `delete` すると派生型の destructor が保証されない。旧 `route.hpp` の `Select::candidates` の解放もこの経路に依存する。新しい router でも GLib wrapper を持つ派生型の解放が必要である。`31.008/router-lifetime` で仮想デストラクタか値所有の代替を実装し、複数規則の追加・破棄と handler の破棄回数を試験する。これは旧ソースの静的追跡であり、HTTP の移植実装ではない。

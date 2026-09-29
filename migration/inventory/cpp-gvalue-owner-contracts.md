# 01.007 旧 C++ の GValue 保有契約

`cpp-gvalue-owner-contracts.tsv` は旧 tree `afa43fae` の136構文候補を一対一で含む。86件は共通 wrapper/trait 定義、17件は値コピーまたは ABI 署名、15件は借用 `GValue*`、11件は GArray の参照 wrapper、7件は値初期化。`python3 -B tools/audit_cpp_gvalue_owners.py` は候補の重複と未対応を検査する。wrapper/trait 定義を使う呼出し元がすべて安全だと証明するものではない。

`IArray<GValue>` は GArray に一時参照を加えるが、要素の深いコピーはしない。`FilterLayer::get_procedure_args()` は `GValueArray` の要素を `g_array_append_vals` で byte copy し、元配列も解放しない。`29.006/proc-args-owner` の popup は `gimp_procedure_get_arguments()` が返した要素を同じ方法で byte copy した直後、元を `g_value_array_free` するため、文字列や object 等の所有ペイロードがコピー先で無効になり得る。`15.001/gvalue-deep-copy` とともに値ごとの `g_value_copy`、元と先の独立破棄を移植先で試験する。

PDB の `setup_args()` と `get_arg()` は文字列への変換用に stack `GValue` を初期化しているが `g_value_unset` がない。`CopyValue` の代入では既存値を破棄せず上書きし、移動元の確保領域も残す。追跡先は `15.001/pdb-temporary-gvalue` と `06.020/value-assignment`。GObject property callback に渡される `GValue*`、param spec の既定値、local wrapper の `ptr()` は借用として扱い、呼出し元の破棄を引き受けない。

この台帳で明示的な参照操作、`CXXPointer`、GValue の候補を所有者・追跡先へ対応付けた。その他の `hold` に対する取得元と失敗経路の照合が `01.007/cpp-ownership` に残る。GIMP 3 上の実装や実行時検証は各 WBS タスクで行う。

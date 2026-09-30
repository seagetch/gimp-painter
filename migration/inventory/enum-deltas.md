# 01.010 enum と生成元

`enum-deltas.tsv` は変更対象全C/C++ファイルのenum memberとMyPaint定数を、旧上流基点と比較した351件・42ファイルの台帳。追加に伴う既存memberの番号移動も含む。各行には定義箇所、型、旧基点値、移植元値、生成元、成果物、保存上の契約、追跡先がある。外部定数に依存するcomposite内部enumは式のまま記録し、実数値を推測しない。

XCFのPROP_FILTER_SPEC=32、PROP_CLONE_SPEC=33とXcfFilterArgType=0〜10は手書きで、generatorはない。layer mode23〜29は保存値として別途10.005で変換表とfixtureを作る。UndoTypeやsignal indexの番号移動は内部状態で、旧XCFに保存するmodeとは区別する。ContextPropMaskのbit移動も直接ファイル数値へ流用しない。

base/core/libgimpbase等のenum定義はheaderが正本で、各Makefile.amが `tools/gimp-mkenums` を呼びGType・nick・説明のCを生成する。公開PDBのenumは `tools/pdbgen/enumgen.pl` → `enums.pl` → `enumcode.pl` を通る。HTTP/presets enum用の生成規則はあるが、headerに実enumがないため追加enumとしては数えない。生成CとPerl表は正本の独立手編集で維持しない。

`mypaintbrush-enum-settings.h` の103定数はINPUT、BRUSH_MAPPING、BRUSH_BOOL、BRUSH_TEXT、STATEの領域。BRUSH_STROKE_OPACITY=42、TEXTURE_GRAIN=43、TEXTURE_CONTRAST=44、BOOL領域45〜49、TEXT領域50〜51を含む。mybで保存されるのは設定の内部名とinput名であり、これらordinalそのものではない。optionsはindex+1をproperty IDとして使う。headerはgenerate.py生成と宣言するが、固定旧treeにその生成スクリプトはない。設定名・型・既定値は `mypaintbrush-brushsettings.c` に残る。生成元復元を `04.013/brush-setting-generator` に明示した。

検査: `python3 -B tools/inventory_enum_deltas.py`。コメントを除いたenumを比較し、数値・shift・aliasを評価する。生成元の復元・再生成のビルド試験は04.013、保存数値やnickの互換性は10/19/30章で行う。

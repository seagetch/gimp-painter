# 01.011 旧依存と配布経路

`legacy-dependencies.tsv` はconfigureのpkg-config、GLib/GTK、AC_CHECK_LIB、FreeType/WMF config-toolと、同梱brushlibの56レコード。API/probe、旧要求版、リンク、配布対象、license宣言・証跡・範囲・追跡先を記録する。最低版が指定されないprobeは、版番号を推測せずprobeそのものを残す。`legacy-flatpak-pins.tsv` の89行は実配布manifestの固定URL/hash/版/optionを別に記録し、コメント化されたWebKit等は無効な構成として区別する。

旧独自差分はBablを0.1.12、GEGLを0.3.0/ABI gegl-0.3へ上げ、JSON-GLib >=1.0とSoup2.4 >=2.46を追加する。GIMP3へ旧GEGL ABIを持ち込まず、現行の依存と画素基盤に接続する。JSON APIはreader/writerとpreset、Soupは任意HTTPサーバーに使う。旧Makefile.amの最終exeにはSOUP_LIBSがある一方、JSON_LIBS/JSON_CFLAGSの使用は旧tree内に確認できない。現行Mesonでは直接依存を明記し、暗黙の推移依存に頼らない。

旧拡張MyPaintは外部libmypaintへのリンクではなく、brush/mapping/stroke/surfaceが同梱され内部static archiveからexeへ入る。brushsettings.c/brush.hppの本文はISC形式の許諾、mypaint-brushmodes.hppはGPL-2.0-or-later。ファイル間でlicenseが違うため一括して「MyPaintはGPL」とは記録しない。GIMP/追加モジュール自身の宣言とも別に扱う。

外部ライブラリーの旧archiveにはライセンス本文が同梱されていない。台帳は確認できた現在のpackage notice（hash付き）または公式宣言と、未取得の旧noticeを明示する。現在のnoticeに含むtest/build/補助ファイルのlicense集合を、本体の単一licenseへ誤って縮約しない。JSON-GLibの公式宣言はLGPL-2.1-or-later、BablはLGPL-3.0-or-later。これは固定旧archiveや今後のバンドルのlicense検証完了を意味しない。配布時のexact-source notice収集は `34.005/license-manifest` とOS別packageタスクへ追跡する。

公式宣言参照（2026-09-30確認）: https://gnome.pages.gitlab.gnome.org/json-glib/ 、https://gegl.org/babl/ 。旧APIと版の正本は `afa43fae:configure.ac`、リンク先は旧 `app/Makefile.am` と各subdir Makefile.am、実配布pinは旧Flatpak yml。`python3 -B tools/inventory_legacy_dependencies.py` で再現する。

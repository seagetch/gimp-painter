#!/usr/bin/env python3
"""Consolidate legacy C++ raw ownership contracts and report coverage."""

import csv
from collections import Counter
from pathlib import Path

ROOT = Path("migration/inventory")
CONTRACTS = {
    "app/core/gimpfilterlayer.cpp:525": ("FilterLayer.updates Rectangle", "delete after removing list link", "17.007"),
    "app/core/gimpfilterlayer.cpp:640": ("FilterLayer.runner", "CXXPointer deletes on replacement/finalize; local r can escape cleanup if set_arg fails", "16.019"),
    "app/core/gimpfilterlayer.cpp:875": ("FilterLayer.updates Rectangle", "delete when projected or free list on reset", "17.007"),
    "app/core/gimpperspectiveguide.cpp:117": ("PerspectiveGuide.vanish_points element", "destructor deletes Point; List frees nodes", "08.009"),
    "app/core/gimpperspectiveguide.cpp:159": ("PerspectiveGuide.vanish_points element", "destructor deletes Point; List frees nodes", "08.009"),
    "app/httpd/httpd.cpp:100": ("RESTResource factory result", "handle deletes this through base type without virtual destructor", "31.008"),
    "app/httpd/navigation-guide.cpp:202": ("NavigationGuideContext.arg_conf", "callback deletes only when webhook_uri exists; context itself not freed here", "31.008"),
    "app/httpd/navigation-guide.cpp:224": ("RESTResource factory result", "RESTResource::handle deletes base pointer", "31.008"),
    "app/httpd/navigation-guide.cpp:253": ("NavigationGuideContext.arg_conf", "callback deletes conditionally; callback registration can fail", "31.008"),
    "app/httpd/rest-image-tree.cpp:269": ("RESTResource factory result", "RESTResource::handle deletes base pointer", "31.008"),
    "app/paint/gimpmypaintcoreundo.cpp:159": ("GimpMypaintCoreUndo.stroke", "undo free deletes and nulls stroke", "09.011/mypaint-undo-stroke"),
    "app/paint/gimpmypaintoptions-history.cpp:85": ("process history singleton", "no explicit singleton deletion found; destructor frees brush list", "24.010"),
    "app/paint/mypaintbrush-brush.hpp:161": ("Brush.settings[i]", "Brush destructor deletes Mapping for each setting", "20.008"),
    "app/paint/mypaintbrush-brush.hpp:178": ("Brush.settings[i]", "delete corresponding constructor Mapping", "20.008"),
    "app/presets/layer-preset-gui.cpp:255": ("process LayerPresetGuiConfig singleton", "no explicit singleton deletion found", "28.014"),
    "app/presets/layer-preset.cpp:691": ("caller-owned ILayerPresetApplier", "returned new instance adopted by caller hold wrapper", "28.010"),
    "app/presets/layer-preset.cpp:749": ("process LayerPresetConfig singleton", "no explicit singleton deletion found", "28.003"),
    "app/presets/preset-factory-gui.cpp:89": ("process PresetGuiFactory singleton", "no explicit singleton deletion found", "28.014"),
    "app/presets/preset-factory.cpp:153": ("process PresetFactory singleton", "no explicit singleton deletion found", "28.003"),
}
CONTRACTS.update({
    "app/base/delegators.hpp:13": ("GObject data or closure payload", "destroy notify deletes typed C++ value", "06.010"),
    "app/base/delegators.hpp:94": ("delegator factory caller", "closure notify or CXXPointer deletes ObjectDelegator", "06.017"),
    "app/base/delegators.hpp:101": ("delegator factory caller", "closure notify or CXXPointer deletes Delegator", "06.017"),
    "app/base/delegators.hpp:107": ("delegator factory caller", "closure notify or CXXPointer deletes Delegator", "06.017"),
    "app/base/delegators.hpp:177": ("Connection factory caller", "caller deletes Connection and disconnects signal", "06.017"),
    "app/base/glib-cxx-impl.hpp:279": ("temporary GParamSpec pointer array", "g_free after property count; GParamSpecs borrowed", "05.008"),
    "app/base/glib-cxx-impl.hpp:283": ("thrown InvalidClass pointer", "no matching pointer catch/delete found", "05.013/class-init-error"),
    "app/base/glib-cxx-utils.hpp:45": ("CString.obj", "g_free previous string on pointer assignment", "06.021"),
    "app/base/glib-cxx-utils.hpp:656": ("owned GValue allocation", "g_value_unset then g_free when IsOwner", "06.020"),
    "app/base/glib-cxx-utils.hpp:1132": ("Decorator.obj", "Decorator destructor deletes wrapped C++ object", "06.023"),
    "app/base/glib-cxx-utils.hpp:1162": ("DelegatorProxy factory caller", "caller must delete proxy; widget and delegate pointers borrowed", "06.023"),
    "app/base/glib-cxx-utils.hpp:1165": ("widget data-full Decorator", "widget destroy or undecorate deletes Decorator and wrapped object", "06.023"),
    "app/base/glib-cxx-utils.hpp:1172": ("widget data-full Decorator", "steal data before explicit delete to avoid double deletion", "06.023"),
    "app/base/glib-cxx-utils.hpp:1191": ("disposable EventSource callback owner", "source destroy notify deletes disposable instance; other instances clear id", "06.019"),
    "app/base/route.hpp:95": ("Route.Rule.rules Select", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/route.hpp:98": ("Route.Rule.rules Name", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/route.hpp:104": ("Route.Rule.rules Match", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/route.hpp:118": ("Route.Rule.handler", "Rule destructor deletes owned delegator", "31.008/router-lifetime"),
    "app/base/route.hpp:158": ("Route.rules Rule", "Route destructor deletes each Rule and handler", "31.008/router-lifetime"),
    "app/base/scopeguard.hpp:65": ("CXXPointer<T> object", "ScopeGuard destroy_instance deletes T once", "06.010"),
    "app/base/soup-cxx-utils.hpp:50": ("Defferred.next", "linked decRef cascade deletes when refCount reaches zero", "31.008"),
    "app/base/soup-cxx-utils.hpp:64": ("Defferred callback chain", "decRef deletes this and decrements prev; no-handler callback can return without decRef", "31.008"),
    "app/base/soup-cxx-utils.hpp:96": ("queued message Defferred", "queue callback should release chain on completion/cancel", "31.008"),
    "app/base/soup-cxx-utils.hpp:251": ("router Matched ** value", "free replaced string after hash insertion; other values require owner cleanup", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:292": ("Soup.Router.Rule.rules Select", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:296": ("Soup.Router.Rule.rules Name", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:305": ("Soup.Router.Rule.rules AnyLoop", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:308": ("Soup.Router.Rule.rules Any", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:313": ("Soup.Router.Rule.rules Match", "Rule destructor deletes TokenRule base pointer without virtual destructor", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:322": ("Soup.Router.Rule.rules children", "delete via TokenRule pointer; derived cleanup requires virtual destructor", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:372": ("Soup.Router.rules Rule", "Router destructor deletes each Rule and handler", "31.008/router-lifetime"),
    "app/base/soup-cxx-utils.hpp:376": ("Soup.Router.rules Rule", "Router owns new Rule and passed handler", "31.008/router-lifetime"),
    "app/core/mypaintbrush-mapping.hpp:42": ("Mapping.control_points", "new[] must match delete[]; old destructor uses delete", "19.006/mapping-value-owner"),
    "app/core/mypaintbrush-mapping.hpp:48": ("Mapping.control_points", "old scalar delete mismatches array allocation", "19.006/mapping-value-owner"),
    "app/core/mypaintbrush-mapping.hpp:52": ("Mapping.control_points", "old scalar delete in resize mismatches array allocation", "19.006/mapping-value-owner"),
    "app/core/mypaintbrush-mapping.hpp:56": ("Mapping.control_points", "replacement new[] paired with value owner on resize", "19.006/mapping-value-owner"),
})
SURFACE = "app/paint/gimpmypaintcore-surface.cpp"
for line, slot in {
    327: "src1PR old region", 331: "destPR old region", 335: "brushPR old region",
    345: "src1PR final region", 346: "destPR final region", 347: "brushPR final region",
    348: "maskPR final region", 349: "texturePR final region",
    400: "src1PR sampled region", 401: "brushPR sampled region",
    402: "maskPR sampled region", 403: "texturePR sampled region",
}.items():
    CONTRACTS[f"{SURFACE}:{line}"] = ("draw/get_color " + slot, "g_free region struct after use or before replacement; backing storage belongs to feature", "09.003/region-lifetime")
CONTRACTS[f"{SURFACE}:614"] = ("GimpMypaintCore.surface", "core cleanup or drawable replacement deletes factory result", "08.006")
CONTRACTS[f"{SURFACE}:618"] = ("brush preview local surface", "preview deletes factory result after session", "19.013/preview-contract")


def main():
    candidates = list(csv.DictReader((ROOT / "cpp-raw-lifetime-review.tsv").open(encoding="utf-8"), delimiter="\t"))
    by_site = {row["legacy_site"]: row for row in candidates}
    assert len(by_site) == len(candidates) == 168
    for path, column in (("mypaint-brush-raw-owners.tsv", "migration_followup"),
                         ("mypaint-core-raw-owners.tsv", "release_contract")):
        for row in csv.DictReader((ROOT / path).open(encoding="utf-8"), delimiter="\t"):
            site = row["legacy_site"]
            assert site in by_site and site not in CONTRACTS
            followup = row[column] if column == "migration_followup" else "08.006"
            CONTRACTS[site] = (row["owner"], row["release_contract"], followup)
    assert set(CONTRACTS) <= set(by_site)
    with (ROOT / "cpp-raw-owner-coverage.tsv").open("w", newline="", encoding="utf-8") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("legacy_site", "operation", "syntax_role", "owner", "release_contract", "followup", "coverage"))
        for row in candidates:
            site = row["legacy_site"]
            owner, release, followup = CONTRACTS.get(site, ("", "", ""))
            writer.writerow((site, row["kind"], row["syntax_role"], owner, release, followup,
                             "MAPPED" if site in CONTRACTS else "PENDING"))
    counts = Counter("MAPPED" if row["legacy_site"] in CONTRACTS else "PENDING" for row in candidates)
    print(f"raw owner coverage: {dict(counts)}")


if __name__ == "__main__":
    main()

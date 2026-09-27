#!/usr/bin/env python3
"""Run in GIMP's python-fu-eval batch process, not as standalone Python."""

import os

destination = Gio.File.new_for_path(os.environ["GIMP_BASELINE_XCF"])
image = Gimp.Image.new(64, 64, Gimp.ImageBaseType.RGB)
assert image is not None
layer = Gimp.Layer.new(image, "baseline", 64, 64,
                       Gimp.ImageType.RGBA_IMAGE, 100.0,
                       Gimp.LayerMode.NORMAL)
assert image.insert_layer(layer, None, 0)
assert Gimp.context_set_foreground(Gegl.Color.new("rgb(1.0,0.0,0.0)"))
assert layer.edit_fill(Gimp.FillType.FOREGROUND)
assert Gimp.file_save(Gimp.RunMode.NONINTERACTIVE, image, destination, None)
assert destination.query_exists(None)
reopened = Gimp.file_load(Gimp.RunMode.NONINTERACTIVE, destination)
assert reopened is not None
assert len(reopened.get_layers()) == 1
assert reopened.get_layers()[0].get_name() == "baseline"
print("BASELINE_CREATE_FILL_SAVE_REOPEN_OK")
reopened.delete()
image.delete()

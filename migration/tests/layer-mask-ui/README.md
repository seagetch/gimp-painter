# Actual layer-mask portrait evidence

The opt-in native MyPaint tool fixture creates a real layer with an independent
half-width mask. Applied, disabled and re-applied states retain identical raw
layer/mask bytes. Each state asserts native projection alpha at independent
left/right probes, the actual apply flag, and painter-canvas-ui=true.

PNG files are unedited GDK captures of the entire actual Xvfb root window
(1080×1920), not reconstructed images or composites. The applied capture records
the final re-applied state. The display's ordinary native projection and oncanvas
controls render the fixture. The final original24 tool cases and Surface18
are rerun without the opt-in flag. Earlier58-case acceptance evidence remains
historical; this checkpoint adds actual rendered-window evidence.

Run `run.sh SOURCE BUILD OUTPUT_DIRECTORY` under the selected native build lock,
after sourcing the pinned environment. The explicit environment opt-in is
registered only when an output directory is supplied; unconditional tests and
the historical layer-mask include are unchanged.

Visual inspection confirmed the oncanvas tool column, color wheel, layer strip
and bottom MyPaint controls in both captures. The applied state visibly hides
the right half behind the transparency checkerboard; disabled shows continuous
blue/yellow bands. No pixels were edited, cropped or combined for delivery.

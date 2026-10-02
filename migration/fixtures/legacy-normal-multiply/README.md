# Historical Normal/Multiply arithmetic references

Sixteen real old-runtime scenes complement the seven custom-mode fixtures.
They use the same distinct 8×8 RGBA inputs and eight opacity/mask combinations
for raw modes 0 (Normal) and 3 (Multiply). Each has an actual direct projection
and a separately merged output. The capture process exited 0 and every expected
completion marker appeared. PNG decoding made no numerical transformation.

The pinned executable and unchanged source-file hashes, arguments, script/log,
raw bytes and output checksums are recorded in capture-report.json and the
manifest. The old normal path uses byte alpha multiplication and truncated
float color interpolation; multiply first performs integer RGB multiplication
and takes the minimum source alpha. Replacing these with GIMP3's upstream
float legacy modes produces small but real differences in old artworks.

These are executable observations, not expected buffers synthesized from the
replacement. Broader precision, offset/group and platform gates remain separate.
The generic legacy CPU path was selected with --no-cpu-accel.

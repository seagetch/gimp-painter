# Original 13.012: six input-storage formats, explicit float32 arithmetic

162 actual GEGL graphs cover u8/u16/u32/half/float/double storage x nine opt-in
float modes x three opacities. Each compares16 RGBA pixels with an independent
Python double oracle after explicit Babl input-storage roundtrip, plus20 outside
source pixels bit-for-bit with the converted backdrop. ROIs have negative origins.
The source rectangle is smaller than backdrop; masks are independent Y float.
Integer storage naturally clamps/quantizes input; half rounds input; double
storage still feeds the expressly float32 operator, not lossless-double math.
This is GEGL input-storage proof, not all native image-precision conversion UI.

All nine native app tests and the focused six-C-unit ASan/UBSan pass. GIMP/XCF/
C++/Babl/GEGL dependencies beyond those listed by sanitizer.json are uninstrumented;
LSan is off. No screenshots or user interaction are claimed by this numerical test.
The deterministic generator is generate-storage-vectors.py in painter-float-kernel;
its expected values are a continuous extension oracle, not fabricated old captures.
Original criterion/dependencies remain unchanged,13.012 stays DOING.

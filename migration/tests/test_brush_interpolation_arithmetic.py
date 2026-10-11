#!/usr/bin/env python3
"""UBSan-check the interpolation expressions extracted from production source.

This tests the arithmetic kernels, not the complete instrumented application.
Use --source with an unchanged gimpbrush-transform.cc to reproduce the old UB.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def kernel(source, name, destination):
    start = source.index(name + " (")
    text = source[start:]
    if not re.search(r"const gint fraction_bits\s*=\s*12;", text):
        raise ValueError("production fractional precision changed: " + name)
    declarations = re.search(
        r"(g(?:u)?int\s+opposite_x, distance_from_true_x;\s*"
        r"g(?:u)?int\s+opposite_y, distance_from_true_y;)", text)
    expression = re.search(re.escape(destination) + r"\s*=\s*(\(\(src_walker\[0\].*?);", text, re.S)
    if declarations is None or expression is None:
        raise ValueError("production interpolation layout changed: " + name)
    if destination == "dest[0]":
        normalized = re.sub(r"\s+", "", expression.group(1))
        for channel in (1, 2):
            other = re.search(r"dest\[" + str(channel) + r"\]\s*=\s*(\(\(src_walker.*?);", text, re.S)
            if other is None or re.sub(r"\s+", "", other.group(1)).replace(
                    "[" + str(channel) + "]", "[0]") != normalized:
                raise ValueError("RGB interpolation channels differ")
    return declarations.group(1), expression.group(1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2] /
                        "app/core/gimpbrush-transform.cc")
    args = parser.parse_args()
    source = args.source.read_text()
    kernels = [kernel(source, "gimp_brush_real_transform_mask", "*dest"),
               kernel(source, "gimp_brush_real_transform_pixmap", "dest[0]")]
    functions = []
    for index, (declarations, expression) in enumerate(kernels):
        functions.append("""
static unsigned interpolate_%d(unsigned x, unsigned y,
                              unsigned a, unsigned b, unsigned c, unsigned d) {
  using gint = int; using guint = unsigned int;
  static_assert(sizeof(guint) == 4, "GIMP guint must have 32 bits here");
  const unsigned char aa[] = {(unsigned char)a}, bb[] = {(unsigned char)b};
  const unsigned char cc[] = {(unsigned char)c}, dd[] = {(unsigned char)d};
  const auto *src_walker=aa, *pixel_next=bb, *pixel_below=cc, *pixel_below_next=dd;
  const int int_multiple=4096, recovery_bits=24;
  %s
  distance_from_true_x=x; distance_from_true_y=y;
  opposite_x=int_multiple-distance_from_true_x;
  opposite_y=int_multiple-distance_from_true_y;
  return %s;
}
""" % (index, declarations, expression))
    program = "#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n" + "\n".join(functions) + r"""
static unsigned reference(unsigned x,unsigned y,unsigned a,unsigned b,unsigned c,unsigned d) {
  return ((std::uint64_t(a)*(4096-x)+std::uint64_t(b)*x)*(4096-y)+
          (std::uint64_t(c)*(4096-x)+std::uint64_t(d)*x)*y)>>24;
}
int main() {
  std::uint64_t checked=0;
  for (auto fn : {interpolate_0,interpolate_1}) {
    // Reproduce the reported 2304 * 1044480 intermediate before the sweep.
    if(fn(1792,1792,255,255,255,255)!=255) return 1;
    for(unsigned y=0;y<4096;++y) for(unsigned x=0;x<4096;++x) {
      // Every 12-bit fractional pair, saturation and opposing extreme corners.
      if(fn(x,y,255,255,255,255)!=255 ||
         fn(x,y,255,0,0,255)!=reference(x,y,255,0,0,255) ||
         fn(x,y,0,255,255,0)!=reference(x,y,0,255,255,0)) return 2;
      checked+=3;
    }
    std::uint32_t state=0x731bc921;
    for(unsigned n=0;n<1000000;++n) {
      auto next=[&](){state=state*1664525u+1013904223u;return state;};
      unsigned x=next()&4095,y=next()&4095,a=next()>>24,b=next()>>24,c=next()>>24,d=next()>>24;
      if(fn(x,y,a,b,c,d)!=reference(x,y,a,b,c,d)) return 3;
      ++checked;
    }
  }
  std::printf("%llu production-expression comparisons passed\n",(unsigned long long)checked);
}
"""
    program = "#include <initializer_list>\n" + program
    with tempfile.TemporaryDirectory(prefix="brush-interpolation-") as directory:
        cpp = Path(directory) / "check.cpp"
        binary = Path(directory) / "check"
        cpp.write_text(program)
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        subprocess.run(compiler + ["-std=c++14", "-O2", "-fsanitize=undefined",
                       "-fno-sanitize-recover=all", str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()

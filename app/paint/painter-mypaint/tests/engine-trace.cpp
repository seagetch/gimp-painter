/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared stimulus/recorder. PAINTER_LEGACY_ORACLE compiles the actual, unedited
 * pinned brush header and calls the pinned installed GIMP 2.8 color library.
 * Resource parsing is the port in both runs: this compares the evaluator only. */
#include <cmath>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include "resource.hpp"
#ifdef PAINTER_LEGACY_ORACLE
extern "C" {
#include "libgimpmath/gimpmath.h"
}
#include "paint/mypaintbrush-surface.hpp"
using std::isfinite;
#include "paint/mypaintbrush-brush.hpp"
using TraceSurfaceBase = Surface;
#else
#include "engine.hpp"
using TraceSurfaceBase = GimpPainter::MyPaint::Surface;
#endif
namespace GP = GimpPainter::MyPaint;
static unsigned long dabs, samples;
struct TraceSurface : TraceSurfaceBase {
  unsigned local_samples = 0;
  bool draw_dab (float x, float y, float radius, float r, float g, float b,
                 float opacity, float hardness, float alpha, float aspect,
                 float angle, float lock_alpha, float colorize, float grain, float contrast) override
  {
    ++dabs; std::printf ("D");
    for (auto v : {x,y,radius,r,g,b,opacity,hardness,alpha,aspect,angle,lock_alpha,colorize,grain,contrast}) std::printf (" %a", double (v));
    std::puts (""); return opacity > 0 && hardness > 0;
  }
  void get_color (float x, float y, float radius, float *r, float *g, float *b, float *a,
                  float hardness, float aspect, float angle, float grain, float contrast) override
  {
    ++samples; ++local_samples; std::printf ("C");
    for (auto v : {x,y,radius,hardness,aspect,angle,grain,contrast}) std::printf (" %a", double (v));
    std::puts ("");
    *r = .25f; *g = .6f; *b = .75f; *a = local_samples % 3 ? .8f : 0.f;
  }
  void begin_session () override {} void end_session () override {}
};
static std::vector<std::string> paths;
static void walk (const std::string& dir)
{
  auto *handle = g_dir_open (dir.c_str (), 0, nullptr); if (!handle) throw std::runtime_error (dir);
  while (auto *name = g_dir_read_name (handle)) {
    auto path = dir+"/"+name;
    if (g_file_test (path.c_str (), G_FILE_TEST_IS_DIR)) walk (path);
    else if (g_str_has_suffix (name, ".myb")) paths.push_back (path);
  }
  g_dir_close (handle);
}
static void trace (const std::string& name, const GP::Resource& resource)
{
  std::printf ("B %s\n", name.c_str ());
#ifdef PAINTER_LEGACY_ORACLE
  Brush engine;
  for (int id = 0; id < BRUSH_MAPPING_COUNT; ++id) {
    Mapping mapping (INPUT_COUNT); mapping.base_value = resource.base_value (id);
    for (int input = 0; input < INPUT_COUNT; ++input) {
      auto points = resource.curve (id, input); mapping.set_n (input, points.size ());
      for (unsigned i = 0; i < points.size (); ++i) mapping.set_point (input, i, points[i].x, points[i].y);
    }
    engine.copy_mapping (id, &mapping);
  }
#else
  GP::Engine engine (resource);
#endif
  TraceSurface surface;
  // Includes stationary and tiny-motion pressure changes, all tilt quadrants,
  // equal/backward timestamps, pen-up, a long idle and a new press.
  const double dt[] = {.01,.008,.008,0,-.002,.017,.010,.009,.010,.014,.01,.010,.030,.01,6,.012};
  for (int i = 0; i < 16; ++i) {
    const float x = i < 4 ? 21.f : i == 4 ? 21.00001f : 21.f + (i-4)*5.5f;
    const float y = i < 5 ? 31.f : 31.f + std::sin (i*.6f)*8.f;
    const float p = i == 0 || i == 13 || i == 14 ? 0.f : .2f + (i%5)*.16f;
    const float xt = std::sin (i*.25f)*.45f, yt = std::cos (i*.6f)*.55f;
#ifdef PAINTER_LEGACY_ORACLE
    bool split = engine.stroke_to (&surface, x,y,p,xt,yt,dt[i]);
#else
    bool split = engine.stroke_to (surface, x,y,p,xt,yt,dt[i]);
#endif
    std::printf ("S %d %d", i, split);
    for (int state = 0; state < STATE_COUNT; ++state) {
#ifdef PAINTER_LEGACY_ORACLE
      float value = engine.get_state (state);
#else
      float value = engine.state (state);
#endif
      std::printf (" %a", double (value));
    }
    std::puts ("");
  }
}
int main (int argc, char **argv)
{
  if (argc != 2) return 2;
  walk (argv[1]); std::sort (paths.begin (), paths.end ());
  for (const auto& path : paths) {
    std::ifstream file (path); std::string content {std::istreambuf_iterator<char> (file), {}};
    trace (path.substr (std::string (argv[1]).size ()+1), GP::Resource::decode (content));
  }
  GP::Resource r;
  r.set_base_value (BRUSH_SMUDGE, .7); r.set_base_value (BRUSH_SMUDGE_LENGTH, .1);
  r.set_base_value (BRUSH_DABS_PER_SECOND, 40); r.set_base_value (BRUSH_COLOR_S, .2); r.set_base_value (BRUSH_COLOR_V, .6);
  r.set_curve (BRUSH_TEXTURE_GRAIN, INPUT_PRESSURE, {{0,-.3},{1,.3}});
  r.set_curve (BRUSH_TEXTURE_CONTRAST, INPUT_TILT_DECLINATION, {{0,.25},{90,1.2}});
  r.set_curve (BRUSH_ELLIPTICAL_DAB_ANGLE, INPUT_DIRECTION, {{0,0},{360,180}});
  r.set_base_value (BRUSH_ELLIPTICAL_DAB_RATIO, 3);
  r.set_base_value (BRUSH_STROKE_OPACITY, .35); r.set_switch (BRUSH_NON_INCREMENTAL, true);
  trace ("synthetic/extended-smudge-texture-stationary", r);
  std::fprintf (stderr, "%zu brushes + 1 synthetic; %lu dabs, %lu samples\n", paths.size (), dabs, samples);
}

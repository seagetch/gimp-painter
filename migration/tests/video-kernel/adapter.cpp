#include "filter-video-kernel.hpp"
#include <exception>
extern "C" int video_old_oracle(int x,int y,const std::uint8_t *in,std::uint8_t *out,int channels,int p,int additive,int rotated)
{
  try {
    /* Divide each absolute coordinate between origin and local position, so
     * every phase test exercises immutable origin addition, not just zero. */
    const std::int64_t ox=x>=0?x/2:0,oy=y>=0?y/3:0;
    GimpPainter::FilterVideoKernel kernel({p,additive,rotated},ox,oy);
    kernel.pixel(std::int64_t(x)-ox,std::int64_t(y)-oy,in,out,std::size_t(channels));
    return 1;
  } catch(const std::invalid_argument&) {return 0;}
}

/* SPDX-License-Identifier: GPL-3.0-or-later
 * Deterministic synthetic inputs shared by the old runtime capture and port test.
 * No search or coverage implementation is shared with the oracle.
 */
#ifndef PAINTER_FILL_STIMULI_H
#define PAINTER_FILL_STIMULI_H
#include <stdint.h>
#define FILL_WIDTH 130
#define FILL_HEIGHT 70
#define FILL_CASES 128
struct FillStimulus {
  int bytes, alpha, threshold, antialias, transparent, use_mask;
  int x1,y1,x2,y2,seed_x,seed_y;
  uint8_t color[4];
};
static struct FillStimulus fill_stimulus(int id, uint8_t *pixels, uint8_t *mask)
{
  struct FillStimulus s;
  int format=id%4, shape=(id/4)%4, threshold=(id/16)%4;
  const int thresholds[]={0,30,100,255};
  s.bytes=format+1;s.alpha=(format==1||format==3);s.threshold=thresholds[threshold];
  s.antialias=id/64;s.transparent=1;s.use_mask=shape!=0;
  s.x1=shape==3?60:0;s.y1=shape==3?3:0;s.x2=shape==3?73:FILL_WIDTH;s.y2=shape==3?67:FILL_HEIGHT;
  s.seed_x=64;s.seed_y=35;
  s.color[0]=30;s.color[1]=s.bytes==2?0:60;s.color[2]=90;s.color[3]=0;
  for(int y=0;y<FILL_HEIGHT;++y)for(int x=0;x<FILL_WIDTH;++x){
    int i=y*FILL_WIDTH+x;
    int delta=(x<61||x>68)?((x+y)%5)*17:0;
    if(x==66&&y>7&&y<63)delta=150;
    for(int c=0;c<s.bytes;++c)pixels[i*s.bytes+c]=(uint8_t)(s.color[c]+delta);
    if(s.alpha)pixels[i*s.bytes+s.bytes-1]=(uint8_t)((shape&1)?255:((x+y)%9?0:70));
    mask[i]=255;
    if(shape==1&&x==65)mask[i]=(y==0||y==69)?255:0;
    if(shape==2)mask[i]=(uint8_t)(x<64?230:x>66?0:255);
    if(shape==3)mask[i]=(uint8_t)(x==65?200:255);
  }
  return s;
}
#endif

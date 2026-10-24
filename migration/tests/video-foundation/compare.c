#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include "reference-patterns.h"
int video_old_oracle(int,int,const uint8_t*,uint8_t*,int,int,int,int);
static unsigned integer_remainder(unsigned n,unsigned divisor){return n-(n/divisor)*divisor;}
static void reference(unsigned x,unsigned y,const uint8_t *in,uint8_t *out,int bpp,unsigned p,int add,int rot){
 unsigned row=integer_remainder(rot?x:y,heights[p]);
 unsigned col=integer_remainder(rot?y:x,widths[p]);
 unsigned selected=(unsigned)(rows[p][row][col]-'0');
 for(int c=0;c<bpp;c++){
  if(c==3)out[c]=in[c];
  else if((unsigned)c!=selected)out[c]=add?in[c]:0;
  else if(!add)out[c]=in[c];
  else out[c]=in[c]>127?255:(uint8_t)(in[c]*2u);
 }
}
static uint64_t count;
static void one(int x,int y,int p,int add,int rot,int bpp,unsigned value,unsigned channel){
 uint8_t in[4]={17,129,253,0},actual[4]={71,73,79,83},want[4]={71,73,79,83};
 in[channel]=(uint8_t)value;in[3]=(uint8_t)(value*73+channel);
 reference((unsigned)x,(unsigned)y,in,want,bpp,(unsigned)p,add,rot);
 if(!video_old_oracle(x,y,in,actual,bpp,p,add,rot)||memcmp(actual,want,sizeof(want))){
  fprintf(stderr,"Video mismatch p%d x%d y%d add%d rot%d bpp%d c%u v%u: %u,%u,%u,%u != %u,%u,%u,%u\n",p,x,y,add,rot,bpp,channel,value,actual[0],actual[1],actual[2],actual[3],want[0],want[1],want[2],want[3]);exit(1);
 }
 count++;
}
int main(void){
 const int flags[]={0,1,-1,INT_MIN,INT_MAX};
 for(int p=0;p<9;p++)for(unsigned a=0;a<5;a++)for(unsigned r=0;r<5;r++){
  unsigned w=flags[r]?heights[p]:widths[p],h=flags[r]?widths[p]:heights[p];
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)for(int bpp=3;bpp<=4;bpp++)for(unsigned c=0;c<3;c++)for(unsigned v=0;v<256;v++){
   /* Nonzero drawable-local ROI origin, not cropped zero-relative phases. */
   one((int)(x+37*w+1),(int)(y+19*h+2),p,flags[a],flags[r],bpp,v,c);
  }
  const int coordinates[][2]={{0,0},{1,2},{INT_MAX,INT_MAX},{INT_MAX-1,INT_MAX-2}};
  for(unsigned i=0;i<4;i++)for(int bpp=3;bpp<=4;bpp++)for(unsigned c=0;c<3;c++)for(unsigned v=0;v<256;v++)one(coordinates[i][0],coordinates[i][1],p,flags[a],flags[r],bpp,v,c);
 }
 unsigned rejected=0;uint8_t in[4]={11,23,47,89},out[4],original[4]={5,7,13,17};
 const int invalid[][4]={{-1,0,0,4},{0,-1,0,4},{INT_MIN,0,0,4},{0,INT_MIN,0,4},{0,0,-1,4},{0,0,9,4},{0,0,INT_MIN,4},{0,0,INT_MAX,4},{0,0,0,0},{0,0,0,1},{0,0,0,2},{0,0,0,5}};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){memcpy(out,original,4);if(video_old_oracle(invalid[i][0],invalid[i][1],in,out,invalid[i][3],invalid[i][2],0,0)||memcmp(out,original,4))return 2;rejected++;}
 memcpy(out,original,4);if(video_old_oracle(0,0,NULL,out,4,0,0,0)||memcmp(out,original,4))return 2;rejected++;
 if(video_old_oracle(0,0,in,NULL,4,0,0,0))return 2;
 rejected++;
 printf("Video %llu independent table/coordinate/channel comparisons;9 patterns,25 signed-flag pairs,RGB/RGBA,all256 component/alpha bytes,nonzero ROI origins and INT_MAX coordinates PASS\n",(unsigned long long)count);
 printf("Harness defined-domain guard rejected%u invalid inputs without calling old callback;not historical/native validation\n",rejected);
 return 0;
}

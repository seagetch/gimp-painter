#define main predecessor_main
#include "test-oracle.c"
#undef main
void old_gray(const unsigned char *src,unsigned char *dst,int n,int has_alpha,int begin,int end) {
 count=n; first=begin; limit=end; memcpy(input,src,n*(1+has_alpha)); check(1+has_alpha,has_alpha); memcpy(dst,output,n*(1+has_alpha));
}

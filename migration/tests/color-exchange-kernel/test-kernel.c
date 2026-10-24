/* SPDX-License-Identifier: GPL-3.0-or-later
 * Reuse immutable old C fixture, wrapping every independent oracle call with
 * an additional production-kernel comparison. No old body is modified.
 */
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t kernel_calls;
void color_exchange_kernel(const unsigned char*,unsigned char*,const unsigned char*,int);
static void checked_oracle(const unsigned char*,unsigned char*,const unsigned char[9],int);
/* Redirect source oracle's callers through checked_oracle while its definition
 * is renamed separately by generated tiny include described in run.py. */
#define main fixture_main
#include "checked-fixture.inc"
#undef main
static void checked_oracle(const unsigned char*p,unsigned char*out,const unsigned char args[9],int bpp){
 ++kernel_calls;independent_oracle(p,out,args,bpp);unsigned char actual[4]={0xa5,0xa5,0xa5,0xa5};color_exchange_kernel(p,actual,args,bpp);assert(!memcmp(actual,out,bpp));if(bpp==3)assert(actual[3]==0xa5);
}

int main(void){int result=fixture_main();assert(kernel_calls==989864);printf("KERNEL_COMPARISONS %llu\n",(unsigned long long)kernel_calls);return result;}

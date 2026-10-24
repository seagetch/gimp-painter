/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main original_point_shadow_main
#include "test-filter-point-shadow.cpp"
#undef main
#include "filter-video-kernel.hpp"
namespace VideoTest {
#include "../../../migration/tests/video-foundation/reference-patterns.h"
using Options=FilterVideoOptions;using Kernel=FilterVideoKernel;
Bytes expected(const Bytes&in,std::size_t w,const FilterSelectionRegion&r,Options options,unsigned ox=0,unsigned oy=0){
 Bytes out(in.size(),0);if(!r.intersects)return out;
 for(int y=r.y1;y<r.y2;y++)for(int x=r.x1;x<r.x2;x++){
  unsigned ax=unsigned(x)+ox,ay=unsigned(y)+oy;
  unsigned yy=options.rotated?ax:ay,xx=options.rotated?ay:ax,p=options.pattern;
  unsigned row=yy-(yy/heights[p])*heights[p],col=xx-(xx/widths[p])*widths[p];
  unsigned selected=rows[p][row][col]-'0';auto i=(std::size_t(y)*w+x)*4;
  for(unsigned c=0;c<3;c++)out[i+c]=c!=selected?(options.additive?in[i+c]:0):!options.additive?in[i+c]:in[i+c]>127?255:in[i+c]*2;
  out[i+3]=in[i+3];
 }
 return out;
}
Outcome audited(const Bytes&in,Options options,Bytes&out,Audit&a){Outcome result=Outcome::cancelled;bool ok=filter_native_vector(in,a.cancel,out,[&](FilterRaster&i,FilterRaster&o,std::atomic<bool>&stop,const FilterRasterFactory&){Observed source(i,a,true),dest(o,a,false);result=filter_point_shadow_raster(source,a.width,a.height,a.region,options,stop,dest,a.progress);return result!=Outcome::cancelled;});return ok?result:Outcome::cancelled;}
void geometry(){unsigned scenes=0;
 for(auto shape:{std::make_pair(1u,1u),std::make_pair(1u,17u),std::make_pair(19u,1u),std::make_pair(67u,66u),std::make_pair(8205u,3u)})for(bool rgb:{false,true})for(int p=0;p<9;p++)for(int add:{0,INT_MIN})for(int rot:{0,INT_MAX}){
  Options options{p,add,rot};auto in=fixture(shape.first,shape.second);if(rgb)for(std::size_t i=3;i<in.size();i+=4)in[i]=255;
  for(auto r:{full_region(shape.first,shape.second),FilterSelectionRegion{true,true,0,0,std::int32_t((shape.first+1)/2),std::int32_t((shape.second+1)/2)},FilterSelectionRegion{true,true,std::int32_t(std::min(3u,shape.first-1)),std::int32_t(std::min(1u,shape.second-1)),std::int32_t(shape.first),std::int32_t(shape.second)},FilterSelectionRegion{true,false,0,0,0,0}}){
   auto want=expected(in,shape.first,r,options);std::atomic<bool>stop{false};Bytes out{17};require(filter_point_shadow_vector(in,shape.first,shape.second,r,options,stop,out)==(r.intersects?Outcome::shadow:Outcome::no_merge)&&out==want,"Video vector phase equation");Bytes alias=in;require(filter_point_shadow_vector(alias,shape.first,shape.second,r,options,stop,alias)==(r.intersects?Outcome::shadow:Outcome::no_merge)&&alias==want,"Video vector alias");
   TemporaryFilterRaster src(directory,in.size()),dst(directory,in.size());src.write(0,in.size(),in.data());auto progress=std::make_shared<FilterProgress>();Audit a(shape.first,shape.second,r,stop,progress);Observed source(src,a,true),dest(dst,a,false);require(filter_point_shadow_raster(source,shape.first,shape.second,r,options,stop,dest,progress)==(r.intersects?Outcome::shadow:Outcome::no_merge),"Video spill outcome");Bytes actual(in.size()),unchanged(in.size());dst.read(0,actual.size(),actual.data());src.read(0,unchanged.size(),unchanged.data());require(actual==want&&unchanged==in,"Video spill/source mismatch");a.observe();require(a.last_progress==1&&a.flushes==1,"Video progress/flush");scenes++;
  }
 }
 require(scenes==1440,"Video scene count");std::cout<<"Video1440 geometry/pattern/flag/layout scenes:vector,alias,real spill,nonzero ROI/chunk/row phase passed\n";
}
void raw_origins(){unsigned scenes=0;for(int p=0;p<9;p++)for(bool edge:{false,true}){
 const unsigned w=edge?9:8205,h=3,ox=edge?unsigned(INT_MAX)-8:17,oy=edge?unsigned(INT_MAX)-2:29;
 Options options{p,INT_MIN,INT_MAX};Kernel kernel(options,ox,oy);auto input=fixture(w,h);auto want=expected(input,w,full_region(w,h),options,ox,oy);Memory in(input),out(input.size());std::atomic<bool>stop{false};require(filter_video_raw_raster(in,out,w,h,kernel,stop)&&out.data==want&&in.data==input,"Video raw nonzero/high origin phase");scenes++;
 }require(scenes==18,"Video raw origin scenes");std::cout<<"Video18 raw chunk-crossing/nonzero/near-INTMAX origin scenes passed\n";}
void boundaries(){std::atomic<bool>stop{true};Options options{0,0,0};
 for(auto shape:{std::make_pair(std::size_t(INT32_MAX),std::size_t(1)),std::make_pair(std::size_t(46340),std::size_t(46340))}){auto r=full_region(shape.first,shape.second);Sized in(std::uint64_t(shape.first)*shape.second*4),out(in.size());require(filter_point_shadow_raster(in,shape.first,shape.second,r,options,stop,out)==Outcome::cancelled&&in.calls==0&&out.calls==0,"Video valid area refused");}
 {auto r=full_region(46341,46341);Sized in(UINT64_C(46341)*46341*4),out(in.size());rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,46341,46341,r,options,stop,out);},"Video old area overflow accepted");require(in.calls==0&&out.calls==0,"Video area preflight I/O");}
 {Sized in(16),out(16);Kernel edge(options,INT_MAX,INT_MAX);rejects<std::invalid_argument>([&]{filter_video_raw_raster(in,out,2,2,edge,stop);},"Video raw origin extent overflow accepted");require(in.calls==0&&out.calls==0,"Video origin preflight I/O");}
 {Sized in(4),out(4);Kernel edge(options,INT_MAX,INT_MAX);require(!filter_video_raw_raster(in,out,1,1,edge,stop)&&in.calls==0&&out.calls==0,"Video raw max coordinate pre-cancel refused");}
 {Sized in(256),out(256);auto r=full_region(8,8);for(int p:{-1,9,INT_MIN,INT_MAX}){Options bad{p,0,0};rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,8,8,r,bad,stop,out);},"Video invalid pattern accepted before zero-fill");}require(in.calls==0&&out.calls==0,"Video pattern rejection I/O");}
 {Sized in(256),out(256),shorter(255);auto r=full_region(8,8);Kernel kernel(options,0,0);stop=false;
 rejects<std::invalid_argument>([&]{filter_video_raw_raster(in,in,8,8,kernel,stop);},"Video raw alias accepted");
 rejects<std::invalid_argument>([&]{filter_video_raw_raster(in,shorter,8,8,kernel,stop);},"Video raw extent accepted");
 rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,8,8,r,options,stop,in);},"Video shadow alias accepted");
 rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,8,8,r,options,stop,shorter);},"Video shadow extent accepted");
 require(in.calls==0&&out.calls==0&&shorter.calls==0,"Video alias/extent preflight I/O");}
 std::cout<<"Video original signed-area/raw origin+extent/pre-cancel bounds reject before I/O\n";
}
void faults(){const std::size_t w=8205,h=3;auto input=fixture(w,h);Options o{7,1,-1};Kernel raw(o,0,0);unsigned faults=0;for(auto r:{FilterSelectionRegion{true,true,3,1,8202,3},FilterSelectionRegion{true,false,0,1,0,2}}){std::atomic<bool>stop{false};auto progress=std::make_shared<FilterProgress>();Audit base(w,h,r,stop,progress);Bytes output;require(audited(input,o,output,base)==(r.intersects?Outcome::shadow:Outcome::no_merge),"Video fault baseline");require(output==expected(input,w,r,o),"Video fault baseline bytes");for(bool spill:{false,true})for(auto operation:{Operation::read,Operation::write,Operation::flush})for(bool fail:{false,true})for(std::size_t at=1;at<=(operation==Operation::read?base.reads:operation==Operation::write?base.writes:base.flushes);at++){std::atomic<bool>cancel{false};auto p=std::make_shared<FilterProgress>();Audit a(w,h,r,cancel,p);if(fail){a.fail_on=operation;a.fail_at=at;}else{a.cancel_on=operation;a.cancel_at=at;}Bytes published{17,29,43};const auto old=published;auto run=[&]{if(!spill)return audited(input,o,published,a);TemporaryFilterRaster in(directory,input.size()),out(directory,input.size());in.write(0,input.size(),input.data());Observed source(in,a,true),dest(out,a,false);return filter_point_shadow_raster(source,w,h,r,o,cancel,dest,p);};if(fail)rejects<StorageFailure>(run,"Video storage failure ignored");else require(run()==Outcome::cancelled,"Video cancellation ignored");require(published==old,"Video published partial output");faults++;}}
 const auto chunks=(input.size()+32767)/32768;for(auto op:{Operation::read,Operation::write,Operation::flush})for(bool fail:{false,true})for(std::size_t at=1;at<=(op==Operation::flush?1:chunks);at++){std::atomic<bool>cancel{false};auto progress=std::make_shared<FilterProgress>();RawAudit audit(cancel,progress);audit.trigger=op;audit.at=at;audit.fail=fail;Bytes published{17,29,43};const auto old=published;auto run=[&]{return filter_native_vector(input,cancel,published,[&](FilterRaster&in,FilterRaster&out,std::atomic<bool>&c,const FilterRasterFactory&){RawObserved source(in,audit,true),dest(out,audit,false);return filter_video_raw_raster(source,dest,w,h,raw,c,progress);});};if(fail)rejects<StorageFailure>(run,"Video raw fault ignored");else require(!run(),"Video raw cancel ignored");require(published==old,"Video raw partial publication");faults++;}std::cout<<"Video "<<faults<<" raw/shadow read-write-flush cancellation/error points preserve publication\n";}

class MutateOptions final:public FilterRaster {
 FilterRaster& base;Options& options;FilterSelectionRegion& region;bool& changed;
 void change(){if(!changed){options={-1,0,0};region={true,false,0,0,0,0};changed=true;}}
 public:MutateOptions(FilterRaster& b,Options& o,FilterSelectionRegion&r,bool& c):base(b),options(o),region(r),changed(c){}
 std::uint64_t size()const noexcept override{return base.size();}
 void read(std::uint64_t x,std::size_t n,std::uint8_t*p)override{change();base.read(x,n,p);}
 void write(std::uint64_t x,std::size_t n,const std::uint8_t*p)override{change();base.write(x,n,p);}
 void flush()override{change();base.flush();}
};
void stability(){Options options{7,-1,1};auto input=fixture(67,66);FilterSelectionRegion region{true,true,3,5,66,65};auto want=expected(input,67,region,options);Memory in(input),out(input.size());bool changed=false;MutateOptions source(in,options,region,changed),dest(out,options,region,changed);std::atomic<bool>stop{false};require(filter_point_shadow_raster(source,67,66,region,options,stop,dest)==Outcome::shadow,"Video immutable snapshot failed");require(changed&&out.data==want&&in.data==input,"Video retained caller options/region alias");std::cout<<"Video options and ROI snapshots survive caller mutation at first zero-write\n";}
}
int main(int argc,char**argv){GError*error=nullptr;auto*path=g_dir_make_tmp("video-shadow-XXXXXX",&error);require(path&&!error,"Video private directory");directory=path;g_free(path);try{VideoTest::geometry();VideoTest::raw_origins();VideoTest::boundaries();VideoTest::faults();VideoTest::stability();}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}require(g_rmdir(directory.c_str())==0,"Video spill cleanup");return original_point_shadow_main(argc,argv);}

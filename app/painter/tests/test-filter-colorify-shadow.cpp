/* SPDX-License-Identifier: GPL-3.0-or-later */
#define main original_point_shadow_main
#include "test-filter-point-shadow.cpp"
#undef main
#include "filter-colorify-kernel.hpp"
namespace ColorifyTest {
using Kernel=FilterColorifyKernel;using Color=FilterColorifyColor;
static unsigned scale(unsigned i,double c){
 uint64_t bits;memcpy(&bits,&c,8);unsigned e=(bits>>52)&2047;
 uint64_t m=bits&UINT64_C(0xfffffffffffff);int exp;
 if(!i||!bits)return 0;
 if(e){m|=UINT64_C(1)<<52;exp=(int)e-1023-52;}else exp=-1074;
 uint64_t product=m*i;if(!product)return 0;
 unsigned top=63-(unsigned)__builtin_clzll(product);
 if(top>52){unsigned shift=top-52;uint64_t q=product>>shift,rem=product&((UINT64_C(1)<<shift)-1),half=UINT64_C(1)<<(shift-1);
  if(rem>half||(rem==half&&(q&1)))q++;
  product=q;exp+=(int)shift;
 }
 if(exp>=0)return (unsigned)(product<<exp);
 return -exp>=64?0:(unsigned)(product>>-exp);
}
Bytes expected(const Bytes&in,std::size_t w,const FilterSelectionRegion&r,const Color&color){
 Bytes out(in.size(),0);if(!r.intersects)return out;unsigned lut[3][256];const double tint[]={color.r,color.g,color.b};
 for(unsigned c=0;c<3;c++)for(unsigned i=0;i<256;i++)lut[c][i]=scale(i,tint[c]);
 for(int y=r.y1;y<r.y2;y++)for(int x=r.x1;x<r.x2;x++){auto p=(std::size_t(y)*w+x)*4;auto l=in[p]*2126/10000+in[p+1]*7152/10000+in[p+2]*722/10000;for(unsigned c=0;c<3;c++)out[p+c]=lut[c][l];out[p+3]=in[p+3];}
 return out;
}
Outcome audited(const Bytes&in,const Kernel&kernel,Bytes&out,Audit&a){Outcome result=Outcome::cancelled;bool ok=filter_native_vector(in,a.cancel,out,[&](FilterRaster&i,FilterRaster&o,std::atomic<bool>&stop,const FilterRasterFactory&){Observed source(i,a,true),dest(o,a,false);result=filter_point_shadow_raster(source,a.width,a.height,a.region,kernel,stop,dest,a.progress);return result!=Outcome::cancelled;});return ok?result:Outcome::cancelled;}
void geometry(){unsigned scenes=0;const Color colors[]={{0,0,0,0},{1,1,1,1},{.1,.2,.3,.7},{.5,.25,.75,-0.0}};
 for(auto shape:{std::make_pair(1u,1u),std::make_pair(1u,17u),std::make_pair(19u,1u),std::make_pair(67u,66u),std::make_pair(8205u,3u)})for(bool promoted_rgb:{false,true})for(auto color:colors){Kernel kernel(color);auto in=fixture(shape.first,shape.second);if(promoted_rgb)for(std::size_t i=3;i<in.size();i+=4)in[i]=255;
  for(auto r:{full_region(shape.first,shape.second),FilterSelectionRegion{true,true,0,0,std::int32_t((shape.first+1)/2),std::int32_t((shape.second+1)/2)},FilterSelectionRegion{true,false,0,0,0,0}}){auto want=expected(in,shape.first,r,color);std::atomic<bool>stop{false};Bytes out{17};require(filter_point_shadow_vector(in,shape.first,shape.second,r,kernel,stop,out)==(r.intersects?Outcome::shadow:Outcome::no_merge)&&out==want,"Colorify vector equation");Bytes alias=in;require(filter_point_shadow_vector(alias,shape.first,shape.second,r,kernel,stop,alias)==(r.intersects?Outcome::shadow:Outcome::no_merge)&&alias==want,"Colorify vector alias");
   TemporaryFilterRaster src(directory,in.size()),dst(directory,in.size());src.write(0,in.size(),in.data());auto progress=std::make_shared<FilterProgress>();Audit a(shape.first,shape.second,r,stop,progress);Observed source(src,a,true),dest(dst,a,false);require(filter_point_shadow_raster(source,shape.first,shape.second,r,kernel,stop,dest,progress)==(r.intersects?Outcome::shadow:Outcome::no_merge),"Colorify spill outcome");Bytes actual(in.size()),unchanged(in.size());dst.read(0,actual.size(),actual.data());src.read(0,unchanged.size(),unchanged.data());require(actual==want&&unchanged==in,"Colorify spill/source mismatch");a.observe();require(a.last_progress==1&&a.flushes==1,"Colorify progress/flush");scenes++;
  }
 }
 require(scenes==120,"Colorify scene count");std::cout<<"Colorify120 geometry/tint/promoted-alpha scenes:vector,alias,real spill,ROI/progress passed\n";
}
void exhaustive(){std::uint64_t count=0;std::atomic<bool>stop{false};const Color colors[]={{0,0,0,0},{1,1,1,1},{1,0,0,.3},{0,1,0,.5},{0,0,1,0},{.5,.25,.75,1},{.1,.2,.3,.7},{std::nextafter(1.0,0.0),std::numeric_limits<double>::min(),std::numeric_limits<double>::denorm_min(),1}};
 for(unsigned red=0;red<256;red++){Bytes in(256*256*4);for(unsigned g=0;g<256;g++)for(unsigned b=0;b<256;b++){auto p=(g*256+b)*4;in[p]=red;in[p+1]=g;in[p+2]=b;in[p+3]=(red+g+b)&255;}
  for(auto color:colors){Kernel kernel(color);Memory source(in),dest(in.size());require(filter_colorify_raw_raster(source,dest,256,256,kernel,stop),"Colorify raw failed");require(dest.data==expected(in,256,full_region(256,256),color),"Colorify exhaustive raw equation");count+=256*256;}}
 require(count==134217728,"Colorify raw count");std::cout<<"Colorify134217728 independent rational/IEEE raw RGB comparisons with varying alpha passed\n";
}
void boundaries(){Kernel k({.1,.2,.3,.4});std::atomic<bool>stop{true};for(auto shape:{std::make_pair(std::size_t(INT32_MAX),std::size_t(1)),std::make_pair(std::size_t(46340),std::size_t(46340))}){auto r=full_region(shape.first,shape.second);Sized in(std::uint64_t(shape.first)*shape.second*4),out(in.size());require(filter_point_shadow_raster(in,shape.first,shape.second,r,k,stop,out)==Outcome::cancelled&&in.calls==0&&out.calls==0,"Colorify valid area refused");}
 {auto r=full_region(46341,46341);Sized in(UINT64_C(46341)*46341*4),out(in.size());rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,46341,46341,r,k,stop,out);},"Colorify old area overflow accepted");require(in.calls==0&&out.calls==0,"Colorify area preflight I/O");}
 auto r=full_region(8,8);Sized in(256),out(256),shorter(255);stop=false;
 rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,8,8,r,k,stop,in);},"Colorify alias accepted");rejects<std::invalid_argument>([&]{filter_point_shadow_raster(in,8,8,r,k,stop,shorter);},"Colorify wrong extent");rejects<std::invalid_argument>([&]{filter_colorify_raw_raster(in,in,8,8,k,stop);},"Colorify raw alias");require(in.calls==0&&out.calls==0&&shorter.calls==0,"Colorify invalid preflight I/O");std::cout<<"Colorify original signed-area and alias/extent preflight passed without I/O\n";
}
void faults(){const std::size_t w=8205,h=3;auto input=fixture(w,h);Kernel o({.1,.2,.3,.7});unsigned faults=0;for(auto r:{FilterSelectionRegion{true,true,3,1,8202,3},FilterSelectionRegion{true,false,0,1,0,2}}){std::atomic<bool>stop{false};auto progress=std::make_shared<FilterProgress>();Audit base(w,h,r,stop,progress);Bytes output;require(audited(input,o,output,base)==(r.intersects?Outcome::shadow:Outcome::no_merge),"Colorify fault baseline");require(output==expected(input,w,r,o.color()),"Colorify fault baseline bytes");for(bool spill:{false,true})for(auto operation:{Operation::read,Operation::write,Operation::flush})for(bool fail:{false,true})for(std::size_t at=1;at<=(operation==Operation::read?base.reads:operation==Operation::write?base.writes:base.flushes);at++){std::atomic<bool>cancel{false};auto p=std::make_shared<FilterProgress>();Audit a(w,h,r,cancel,p);if(fail){a.fail_on=operation;a.fail_at=at;}else{a.cancel_on=operation;a.cancel_at=at;}Bytes published{17,29,43};const auto old=published;auto run=[&]{if(!spill)return audited(input,o,published,a);TemporaryFilterRaster in(directory,input.size()),out(directory,input.size());in.write(0,input.size(),input.data());Observed source(in,a,true),dest(out,a,false);return filter_point_shadow_raster(source,w,h,r,o,cancel,dest,p);};if(fail)rejects<StorageFailure>(run,"Colorify storage failure ignored");else require(run()==Outcome::cancelled,"Colorify cancellation ignored");require(published==old,"Colorify published partial output");faults++;}}
 const auto chunks=(input.size()+32767)/32768;for(auto op:{Operation::read,Operation::write,Operation::flush})for(bool fail:{false,true})for(std::size_t at=1;at<=(op==Operation::flush?1:chunks);at++){std::atomic<bool>cancel{false};auto progress=std::make_shared<FilterProgress>();RawAudit audit(cancel,progress);audit.trigger=op;audit.at=at;audit.fail=fail;Bytes published{17,29,43};const auto old=published;auto run=[&]{return filter_native_vector(input,cancel,published,[&](FilterRaster&in,FilterRaster&out,std::atomic<bool>&c,const FilterRasterFactory&){RawObserved source(in,audit,true),dest(out,audit,false);return filter_colorify_raw_raster(source,dest,w,h,o,c,progress);});};if(fail)rejects<StorageFailure>(run,"Colorify raw fault ignored");else require(!run(),"Colorify raw cancel ignored");require(published==old,"Colorify raw partial publication");faults++;}std::cout<<"Colorify "<<faults<<" raw/shadow read-write-flush cancellation/error points preserve publication\n";}
class ReleaseKernel final:public FilterRaster {
 FilterRaster& base;std::unique_ptr<Kernel>& owner;bool& released;
 void release(){if(owner){owner.reset();released=true;}}
 public:ReleaseKernel(FilterRaster& b,std::unique_ptr<Kernel>& o,bool& r):base(b),owner(o),released(r){}
 std::uint64_t size()const noexcept override{return base.size();}
 void read(std::uint64_t x,std::size_t n,std::uint8_t*p)override{release();base.read(x,n,p);}
 void write(std::uint64_t x,std::size_t n,const std::uint8_t*p)override{release();base.write(x,n,p);}
 void flush()override{release();base.flush();}
};
void stability(){for(bool shadow:{false,true}){Color color{.1,.2,.3,.4};std::unique_ptr<Kernel> owner(new Kernel(color));auto input=fixture(67,66);auto want=expected(input,67,full_region(67,66),color);Memory in(input),out(input.size());bool released=false;ReleaseKernel source(in,owner,released),dest(out,owner,released);std::atomic<bool>stop{false};if(shadow)require(filter_point_shadow_raster(source,67,66,full_region(67,66),*owner,stop,dest)==Outcome::shadow,"Colorify shadow snapshot failed");else require(filter_colorify_raw_raster(source,dest,67,66,*owner,stop),"Colorify raw snapshot failed");require(released&&!owner&&out.data==want&&in.data==input,"Colorify retained caller kernel reference");}std::cout<<"Colorify immutable snapshot survives caller kernel deletion at first raw read/shadow zero-write\n";}
}
int main(int argc,char**argv){GError*error=nullptr;auto*path=g_dir_make_tmp("colorify-shadow-XXXXXX",&error);require(path&&!error,"Colorify private directory");directory=path;g_free(path);try{ColorifyTest::geometry();ColorifyTest::exhaustive();ColorifyTest::boundaries();ColorifyTest::faults();ColorifyTest::stability();}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}require(g_rmdir(directory.c_str())==0,"Colorify spill cleanup");return original_point_shadow_main(argc,argv);}

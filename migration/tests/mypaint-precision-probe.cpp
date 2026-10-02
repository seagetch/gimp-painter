#include <gegl.h>
#include <lcms2.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
int main(int argc,char**argv){gegl_init(&argc,&argv);
 for(const char *t:{"u8","u16","u32","half","float","double"})for(const char*m:{"RGB","RGBA","R'G'B'","R'G'B'A","R~G~B~","R~G~B~A","Y","YA","Y'","Y'A","Y~","Y~A"}){auto name=std::string(m)+" "+t;auto*f=babl_format(name.c_str());std::printf("format %s model=%s components=%d bytes=%d space=%s\n",name.c_str(),babl_get_name(babl_format_get_model(f)),babl_format_get_n_components(f),babl_format_get_bytes_per_pixel(f),babl_get_name(babl_format_get_space(f)));}
 {uint32_t x[]={0x80000000u,0x80000001u,0xffffffffu};double y[3];uint32_t z[3];babl_process(babl_fish(babl_format("RGB u32"),babl_format("RGB double")),x,y,1);babl_process(babl_fish(babl_format("RGB double"),babl_format("RGB u32")),y,z,1);for(int i=0;i<3;i++)std::printf("u32 %u -> %.17g -> %u nativeExpected=%.17g\n",x[i],y[i],z[i],double(x[i])/4294967295.);}
 {double x[]={.5,std::nextafter(.5,1.),-.1,2};double y[4],z[4];babl_process(babl_fish(babl_format("RGBA double"),babl_format("R'G'B'A double")),x,y,1);babl_process(babl_fish(babl_format("R'G'B'A double"),babl_format("RGBA double")),y,z,1);for(int i=0;i<4;i++)std::printf("double %.17g -> %.17g -> %.17g\n",x[i],y[i],z[i]);}
 {cmsToneCurve*c=cmsBuildGamma(nullptr,1.8);cmsHPROFILE p=cmsCreateGrayProfile(cmsD50_xyY(),c);cmsUInt32Number len=0;cmsSaveProfileToMem(p,nullptr,&len);std::vector<char>data(len);cmsSaveProfileToMem(p,data.data(),&len);const char*error=nullptr;auto*space=babl_space_from_icc(data.data(),len,BABL_ICC_INTENT_RELATIVE_COLORIMETRIC,&error);std::printf("Gray space %s error=%s\n",space?babl_get_name(space):"null",error?error:"none");if(space){double x[]={.2,.5,.8,1},y[2],z[4];auto*eval=babl_format_with_space("R'G'B'A double",space);auto*work=babl_format_with_space("YA double",space);babl_process(babl_fish(eval,work),x,y,1);babl_process(babl_fish(work,eval),y,z,1);std::printf("Gray convert %.17g %.17g -> %.17g %.17g %.17g\n",y[0],y[1],z[0],z[1],z[2]);}cmsCloseProfile(p);cmsFreeToneCurve(c);}
 gegl_exit();}

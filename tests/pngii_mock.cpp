#include "../third_party/PNGII_API.h"
#include <cstring>
#include <string>
namespace {
bool ready=false;
unsigned calls=0;
PNGII::API::Result resolve(std::uint32_t form,const char* label,std::int32_t soul,char* output,std::uint32_t capacity,std::uint32_t* required)noexcept {
    using Result=PNGII::API::Result;++calls;*required=0;if(capacity)output[0]=0;
    if(!ready)return Result::kNotReady;
    if(soul!=-1||form==2)return Result::kNoMatch;
    const char* file=form==3?"missing.png":form==4?"bad.png":form==5?"icon.svg":form==6?"throws.png":
        form==0&&std::strcmp(label,"armor_hands")==0?"icon.png":form==1&&std::strcmp(label,"weapon_custom")==0?"icon.png":nullptr;
    if(!file)return Result::kNoMatch;
    const auto path=std::string("Interface/PNGIconInjector/Test/")+file;*required=unsigned(path.size()+1);
    if(capacity<*required)return Result::kBufferTooSmall;
    std::memcpy(output,path.c_str(),*required);return Result::kSuccess;
}
}
extern "C" __declspec(dllexport) void PNGII_TestReady(bool value){ready=value;}
extern "C" __declspec(dllexport) unsigned PNGII_TestCalls(){return calls;}
#ifndef PNGII_NO_API
extern "C" __declspec(dllexport) const void* PNGII_RequestAPI(std::uint32_t version)noexcept {
#ifdef PNGII_BAD_API
    static const PNGII::API::InterfaceV1 api{2,sizeof(api),resolve};
#else
    static const PNGII::API::InterfaceV1 api{1,sizeof(api),resolve};
#endif
    return version==1?&api:nullptr;
}
#endif

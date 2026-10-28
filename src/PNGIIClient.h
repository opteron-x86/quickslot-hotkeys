#pragma once
#include "../third_party/PNGII_API.h"
#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quickslot {
inline std::optional<std::string> pngiiResourcePath(std::string path) {
    if(path.empty()||path.size()>4095)return {};
    std::replace(path.begin(),path.end(),'\\','/');
    std::string lower=path;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c+32):char(c);});
    if(!lower.starts_with("interface/pngiconinjector/")||(!lower.ends_with(".png")&&!lower.ends_with(".svg"))||path.find(':')!=std::string::npos)return {};
    for(unsigned char c:path)if(c<32||c==127)return {};
    for(std::size_t start=0;start<path.size();) {
        const auto end=path.find('/',start);const auto part=std::string_view(path).substr(start,end==std::string::npos?end:end-start);
        if(part.empty()||part=="."||part=="..")return {};
        if(end==std::string::npos)break;start=end+1;
    }
    return path;
}
inline bool pngiiAPIValid(const PNGII::API::InterfaceV1* api) {
    return api&&api->version==PNGII::API::kVersion1&&api->structSize>=sizeof(PNGII::API::InterfaceV1)&&api->resolveIcon;
}
struct PNGIIQuery {
    PNGII::API::Result result=PNGII::API::Result::kNoMatch;
    std::string path;
};
inline PNGIIQuery queryPNGII(const PNGII::API::InterfaceV1* api,std::uint32_t form,const std::string& label,std::int32_t soul=-1) {
    using Result=PNGII::API::Result;
    if(!pngiiAPIValid(api))return {};
    std::array<char,260> small{};std::uint32_t required=0;
    auto status=api->resolveIcon(form,label.c_str(),soul,small.data(),unsigned(small.size()),&required);
    std::vector<char> large;char* data=small.data();std::size_t capacity=small.size();
    if(status==Result::kBufferTooSmall) {
        if(required<=small.size()||required>4096)return {Result::kError,{}};
        large.resize(required);data=large.data();capacity=large.size();
        status=api->resolveIcon(form,label.c_str(),soul,data,unsigned(capacity),&required);
    }
    if(status!=Result::kSuccess)return {status,{}};
    if(required<2||required>capacity||data[required-1]!='\0'||std::find(data,data+required-1,'\0')!=data+required-1)return {Result::kError,{}};
    const auto path=pngiiResourcePath(std::string(data,required-1));
    return path?PNGIIQuery{Result::kSuccess,*path}:PNGIIQuery{Result::kError,{}};
}
}

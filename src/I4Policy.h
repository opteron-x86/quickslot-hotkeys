#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace quickslot {
inline std::optional<std::string> i4SourcePath(std::string source) {
    if(source.empty())source="skyui/icons_item_psychosteve.swf";
    if(source.size()>240)return {};
    std::replace(source.begin(),source.end(),'\\','/');
    std::string lower=source;
    std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c+32):char(c);});
    if(lower.starts_with("interface/")){source.erase(0,10);lower.erase(0,10);}
    if(!lower.ends_with(".swf")||source.front()=='/'||source.find(':')!=std::string::npos)return {};
    for(unsigned char c:source)if(c<32)return {};
    for(std::size_t start=0;start<source.size();) {
        const auto end=source.find('/',start);const auto part=std::string_view(source).substr(start,end==std::string::npos?end:end-start);
        if(part.empty()||part=="."||part=="..")return {};
        if(end==std::string::npos)break;start=end+1;
    }
    return "Interface/"+source;
}
struct IconFit {double x,y,scale;};
inline std::optional<IconFit> fitI4Icon(double left,double top,double right,double bottom,double size) {
    const double width=right-left,height=bottom-top;
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(width)||!std::isfinite(height)||
       !std::isfinite(size)||width<=0||height<=0||size<4)return {};
    const double scale=(size*216/256)/std::max(width,height);
    return IconFit{(size-width*scale)*.5-left*scale,(size-height*scale)*.5-top*scale,scale};
}
// Scaleform render textures contain premultiplied color; ImGui consumes straight RGBA.
inline bool straightenI4Pixels(std::span<std::uint8_t> rgba) {
    bool visible=false;
    for(std::size_t i=0;i+3<rgba.size();i+=4) {
        const unsigned alpha=rgba[i+3];visible|=alpha!=0;
        for(unsigned c=0;c<3;++c)rgba[i+c]=alpha?std::uint8_t(std::min(255u,(unsigned(rgba[i+c])*255+alpha/2)/alpha)):0;
    }
    return visible;
}
}

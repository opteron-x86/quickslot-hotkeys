#include "PNGIIImages.h"
#include <lunasvg.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace quickslot {
namespace {
constexpr unsigned side=256,extent=216;
ImageAsset canvas(){return {side,side,std::vector<unsigned char>(side*side*4)};}
ImageAsset fitPNG(const ImageAsset& source) {
    if(!source)return {};
    auto target=canvas();const float scale=float(extent)/std::max(source.width,source.height);
    const unsigned width=std::max(1,int(std::lround(source.width*scale))),height=std::max(1,int(std::lround(source.height*scale)));
    const unsigned left=(side-width)/2,top=(side-height)/2;
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        const float sx=std::clamp((x+.5f)*source.width/width-.5f,0.f,float(source.width-1));
        const float sy=std::clamp((y+.5f)*source.height/height-.5f,0.f,float(source.height-1));
        const int x0=int(sx),y0=int(sy),x1=std::min(x0+1,source.width-1),y1=std::min(y0+1,source.height-1);
        const float dx=sx-x0,dy=sy-y0;
        const unsigned offsets[]{unsigned(y0*source.width+x0)*4,unsigned(y0*source.width+x1)*4,unsigned(y1*source.width+x0)*4,unsigned(y1*source.width+x1)*4};
        const float weights[]{(1-dx)*(1-dy),dx*(1-dy),(1-dx)*dy,dx*dy};
        float alpha=0,color[3]{};
        for(unsigned p=0;p<4;++p){const auto* rgba=source.pixels.data()+offsets[p];const float a=weights[p]*rgba[3];alpha+=a;for(unsigned c=0;c<3;++c)color[c]+=a*rgba[c];}
        auto* rgba=target.pixels.data()+((top+y)*side+left+x)*4;
        rgba[3]=static_cast<unsigned char>(std::clamp(std::lround(alpha),0l,255l));
        for(unsigned c=0;c<3;++c)rgba[c]=alpha>0?static_cast<unsigned char>(std::clamp(std::lround(color[c]/alpha),0l,255l)):0;
    }
    return target;
}
}
ImageAsset decodePNGIIImage(std::string_view path,std::span<const unsigned char> bytes) {
    if(bytes.empty()||bytes.size()>16*1024*1024)return {};
    std::string extension(path.substr(path.find_last_of('.')+1));
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return c>='A'&&c<='Z'?char(c+32):char(c);});
    if(extension=="png")return fitPNG(decodeImageAsset(bytes,4096));
    if(extension!="svg")return {};
    auto doc=lunasvg::Document::loadFromData(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!doc)return {};
    const float width=doc->width(),height=doc->height();
    if(!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return {};
    const float scale=float(extent)/std::max(width,height);
    if(!std::isfinite(scale))return {};
    lunasvg::Bitmap bitmap(side,side);if(bitmap.isNull())return {};
    bitmap.clear(0);
    doc->render(bitmap,lunasvg::Matrix(scale,0,0,scale,(side-width*scale)/2,(side-height*scale)/2));
    bitmap.convertToRGBA();auto target=canvas();
    for(unsigned y=0;y<side;++y)std::memcpy(target.pixels.data()+y*side*4,bitmap.data()+y*bitmap.stride(),side*4);
    return target;
}
}

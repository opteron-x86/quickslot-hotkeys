#include "Assets.h"
#include <lunasvg.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../third_party/stb_image.h"

namespace quickslot {
namespace {
std::vector<unsigned char> readBinaryFile(const std::filesystem::path& path,std::size_t maxBytes=16*1024*1024) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream)return {};
    const auto length=stream.tellg();
    if(length<=0||static_cast<std::size_t>(length)>maxBytes)return {};
    std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
    stream.seekg(0);
    if(!stream.read(reinterpret_cast<char*>(bytes.data()),length))return {};
    return bytes;
}
}
ImageAsset decodeImageAsset(std::span<const unsigned char> bytes,int maxDimension) {
    ImageAsset result;
    if(bytes.empty()||bytes.size()>16*1024*1024)return result;
    int w=0,h=0,channels=0;
    if(!stbi_info_from_memory(bytes.data(),int(bytes.size()),&w,&h,&channels)||w<1||h<1||w>maxDimension||h>maxDimension)return result;
    auto* decoded=stbi_load_from_memory(bytes.data(),int(bytes.size()),&w,&h,&channels,4);
    if(!decoded)return result;
    result.width=w;result.height=h;result.pixels.assign(decoded,decoded+std::size_t(w)*std::size_t(h)*4);
    stbi_image_free(decoded);return result;
}
ImageAsset loadImageAsset(const std::filesystem::path& path) {
    const auto bytes=readBinaryFile(path);
    return bytes.empty()?ImageAsset{}:decodeImageAsset(bytes);
}
ImageAsset decodeThemeImage(std::string_view extension,std::span<const unsigned char> bytes,unsigned svgMaxDimension,int maxDimension) {
    if(bytes.empty()||bytes.size()>16*1024*1024||maxDimension<1)return {};
    std::string ext(extension);
    if(!ext.empty()&&ext.front()=='.')ext.erase(ext.begin());
    for(auto& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));
    if(ext=="png")return decodeImageAsset(bytes,maxDimension);
    if(ext!="svg")return {};
    auto doc=lunasvg::Document::loadFromData(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    if(!doc)return {};
    const float srcW=doc->width(),srcH=doc->height();
    if(!std::isfinite(srcW)||!std::isfinite(srcH)||srcW<=0||srcH<=0)return {};
    const unsigned cap=std::clamp(svgMaxDimension,64u,static_cast<unsigned>(maxDimension));
    const float scale=float(cap)/std::max(srcW,srcH);
    if(!std::isfinite(scale)||scale<=0)return {};
    const int width=std::clamp(int(std::lround(srcW*scale)),1,maxDimension);
    const int height=std::clamp(int(std::lround(srcH*scale)),1,maxDimension);
    lunasvg::Bitmap bitmap(width,height);
    if(bitmap.isNull())return {};
    bitmap.clear(0);
    doc->render(bitmap,lunasvg::Matrix(float(width)/srcW,0,0,float(height)/srcH,0,0));
    bitmap.convertToRGBA();
    ImageAsset out;out.width=width;out.height=height;
    out.pixels.resize(std::size_t(width)*std::size_t(height)*4);
    for(int y=0;y<height;++y)std::memcpy(out.pixels.data()+std::size_t(y)*std::size_t(width)*4,bitmap.data()+std::size_t(y)*std::size_t(bitmap.stride()),std::size_t(width)*4);
    return out;
}
ImageAsset loadThemeImage(const std::filesystem::path& path,unsigned svgMaxDimension,int maxDimension) {
    const auto bytes=readBinaryFile(path);
    return bytes.empty()?ImageAsset{}:decodeThemeImage(path.extension().string(),bytes,svgMaxDimension,maxDimension);
}
}

namespace quickslot {
KeyAtlas loadKeyAtlas(const std::filesystem::path& directory) {

    constexpr int cellW=128,cellH=64,columns=8,width=columns*cellW,height=36*cellH;
    KeyAtlas atlas;atlas.image.width=width;atlas.image.height=height;
    atlas.image.pixels.resize(width*height*4);
    for(unsigned scan=0;scan<atlas.glyphs.size();++scan) {
        const auto name=keyGlyphName(scan);if(name.empty())continue;
        const auto source=loadImageAsset(directory/(name+".png"));if(!source)continue;
        const float factor=std::min({1.f,float(cellW-2)/source.width,float(cellH-2)/source.height});
        const int w=std::max(1,int(source.width*factor)),h=std::max(1,int(source.height*factor));
        const int left=int(scan%columns)*cellW+1,top=int(scan/columns)*cellH+1;
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            const int sx=std::min(source.width-1,int((x+.5f)*source.width/w)),sy=std::min(source.height-1,int((y+.5f)*source.height/h));
            for(int channel=0;channel<4;++channel)atlas.image.pixels[((top+y)*width+left+x)*4+channel]=source.pixels[(sy*source.width+sx)*4+channel];
        }
        atlas.glyphs[scan]={float(left)/width,float(top)/height,float(left+w)/width,float(top+h)/height,float(source.width)/source.height};
        ++atlas.loaded;
    }
    if(!atlas.loaded)atlas.image={};
    return atlas;
}
}

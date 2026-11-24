#include "Assets.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace quickslot;
namespace {
void appendBe32(std::vector<unsigned char>& out,std::uint32_t v) {
    out.push_back(static_cast<unsigned char>((v>>24)&0xff));
    out.push_back(static_cast<unsigned char>((v>>16)&0xff));
    out.push_back(static_cast<unsigned char>((v>>8)&0xff));
    out.push_back(static_cast<unsigned char>(v&0xff));
}
std::uint32_t crc32(const unsigned char* data,std::size_t len) {
    std::uint32_t crc=0xffffffffu;
    for(std::size_t i=0;i<len;++i) {
        crc^=data[i];
        for(int k=0;k<8;++k)crc=(crc>>1)^(0xedb88320u&-(crc&1u));
    }
    return ~crc;
}
void appendChunk(std::vector<unsigned char>& out,const char type[4],const std::vector<unsigned char>& payload) {
    appendBe32(out,static_cast<std::uint32_t>(payload.size()));
    const std::size_t start=out.size();
    out.insert(out.end(),type,type+4);
    out.insert(out.end(),payload.begin(),payload.end());
    appendBe32(out,crc32(out.data()+start,payload.size()+4));
}
std::vector<unsigned char> encodeRgbaPng(unsigned w,unsigned h,const std::vector<unsigned char>& rgba) {
    std::vector<unsigned char> out{137,80,78,71,13,10,26,10};
    std::vector<unsigned char> ihdr;
    appendBe32(ihdr,w);appendBe32(ihdr,h);
    ihdr.insert(ihdr.end(),{8,6,0,0,0});
    appendChunk(out,"IHDR",ihdr);
    std::vector<unsigned char> raw;
    raw.reserve(std::size_t(h)*(std::size_t(w)*4+1));
    for(unsigned y=0;y<h;++y) {
        raw.push_back(0);
        const auto row=rgba.begin()+std::ptrdiff_t(y)*std::ptrdiff_t(w)*4;
        raw.insert(raw.end(),row,row+std::ptrdiff_t(w)*4);
    }
    std::vector<unsigned char> z{0x78,0x01};
    std::uint32_t s1=1,s2=0;
    for(const unsigned char b:raw){s1=(s1+b)%65521u;s2=(s2+s1)%65521u;}
    for(std::size_t pos=0;pos<raw.size();) {
        const std::size_t block=std::min<std::size_t>(65535,raw.size()-pos);
        const bool last=(pos+block)==raw.size();
        z.push_back(last?1:0);
        const auto len=static_cast<std::uint16_t>(block),nlen=static_cast<std::uint16_t>(~len);
        z.push_back(len&0xff);z.push_back((len>>8)&0xff);
        z.push_back(nlen&0xff);z.push_back((nlen>>8)&0xff);
        z.insert(z.end(),raw.begin()+std::ptrdiff_t(pos),raw.begin()+std::ptrdiff_t(pos+block));
        pos+=block;
    }
    appendBe32(z,(s2<<16)|s1);
    appendChunk(out,"IDAT",z);
    appendChunk(out,"IEND",{});
    return out;
}
}
int main(int argc,char** argv)try {
    if(argc!=2)return 2;
    const auto root=std::filesystem::path(argv[1]);
    unsigned checks=0;
    if(loadImageAsset(root/"assets/missing.png")||loadImageAsset(root/"assets/fonts/Jost-Regular.ttf"))throw std::runtime_error("Invalid asset did not fail cleanly");
    checks+=2;
    {
        std::vector<unsigned char> pixels(1024*320*4,0);
        for(unsigned cell=0;cell<4;++cell)for(unsigned y=6;y<314;++y)for(unsigned x=4;x<252;++x) {
            if(x<28||x>=228||y<30||y>=290) {
                const std::size_t at=(std::size_t(y)*1024+cell*256+x)*4;
                pixels[at]=180;pixels[at+1]=140;pixels[at+2]=96;pixels[at+3]=230;
            }
        }
        const auto pngBytes=encodeRgbaPng(1024,320,pixels);
        const auto tempAtlas=std::filesystem::temp_directory_path()/"quickslot_test_atlas_1024x320.png";
        {std::ofstream f(tempAtlas,std::ios::binary);f.write(reinterpret_cast<const char*>(pngBytes.data()),std::streamsize(pngBytes.size()));}
        const auto frames=loadImageAsset(tempAtlas);
        if(!frames||frames.width!=1024||frames.height!=320)throw std::runtime_error("Legacy 1024x320 frame atlas load failed");++checks;
        for(unsigned cell=0;cell<4;++cell) {
            unsigned filled=0,empty=0;
            for(unsigned y=0;y<320;++y)for(unsigned x=0;x<256;++x) {
                const auto alpha=frames.pixels[(y*1024+cell*256+x)*4+3];if(alpha)++filled;else ++empty;
                if((x<4||x>=252||y<6||y>=314)&&alpha)throw std::runtime_error("Theme frame bleeds out of cell");
            }
            if(filled<20000||empty<4000)throw std::runtime_error("Frame art missing or transparency lost");++checks;
        }
        const auto png=loadThemeImage(tempAtlas,512,2048);
        std::filesystem::remove(tempAtlas);
        if(!png||png.width!=1024||png.height!=320)throw std::runtime_error("PNG theme image load failed");++checks;
    }
    const auto keys=loadKeyAtlas(root/"tests/fixtures/keys");
    if(keys.loaded!=3||!keys.image||keys.glyphs[2].aspect!=1||keys.glyphs[68].aspect!=1||keys.glyphs[57].aspect<=1.4f)throw std::runtime_error("Key atlas mapping or wide-key aspect ratio incorrect");++checks;
    if(keys.glyphs[30].aspect||keys.glyphs[211].aspect||loadKeyAtlas(root/"tests/fixtures/absent").image)throw std::runtime_error("Missing/corrupt key glyph failed to fall back");++checks;
    for(const auto scan:{2u,57u,68u}) {
        const auto& uv=keys.glyphs[scan];const int x=int(uv.u0*keys.image.width),y=int(uv.v0*keys.image.height);
        if(uv.u1<=uv.u0||uv.v1<=uv.v0||uv.u1>1||uv.v1>1)throw std::runtime_error("Key UV outside atlas");
        for(int row=y;row<int(uv.v1*keys.image.height);++row)if(keys.image.pixels[(row*keys.image.width+x-1)*4+3])throw std::runtime_error("Key artwork lost transparent gutter");++checks;
    }
    if(keyGlyphName(71)!="Keypad7"||keyGlyphName(200)!="Up"||keyGlyphName(87)!="F11"||!keyGlyphName(255).empty())throw std::runtime_error("Special-key names differ from pack contract");++checks;
    {
        const std::string svg=R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 100"><rect x="10" y="10" width="180" height="80" fill="#ff4020" fill-opacity="0.5"/></svg>)";
        const auto raw=std::span<const unsigned char>(reinterpret_cast<const unsigned char*>(svg.data()),svg.size());
        const auto img=decodeThemeImage(".svg",raw,256);
        if(!img||img.width!=256||img.height!=128)throw std::runtime_error("SVG theme image did not preserve aspect ratio");++checks;
        const std::size_t mid=(std::size_t(64)*256+128)*4;
        if(img.pixels[mid+3]<110||img.pixels[mid+3]>145||img.pixels[mid]<240||img.pixels[mid+2]>50)throw std::runtime_error("SVG straight RGBA conversion mismatch");++checks;
        if(img.pixels[3]!=0)throw std::runtime_error("SVG transparent margin lost");++checks;
        if(decodeThemeImage(".bmp",raw,256)||decodeThemeImage(".svg",std::span<const unsigned char>(reinterpret_cast<const unsigned char*>("not-svg"),7),256))
            throw std::runtime_error("Unsupported or corrupt theme image did not fail cleanly");++checks;
        for(const char* rel:{"assets/themes/oathvein/diamond_border.png","assets/themes/oathvein/panel_border.png","assets/themes/dwemer-aetherium/wheel_outer_rim.png","assets/themes/daedric-oblivion/hexagon_border.png","assets/themes/crimson-filigree/card_border.png","assets/themes/amethyst/wheel_ring_bg.png","docs/starter-theme/card_border.svg","docs/starter-theme/wheel_outer_rim.svg"}) {
            const auto tex=loadThemeImage(root/rel,256,2048);
            if(!tex||tex.width<256||tex.height<128)throw std::runtime_error(std::string("Bundled theme asset failed to load: ")+rel);++checks;
        }
    }
    std::cout<<"PASS: "<<checks<<" native image checks (theme frames, SVG/PNG theme assets, keycaps, transparent padding, missing/corrupt fallback).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

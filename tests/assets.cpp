#include "Assets.h"
#include "Icons.h"
#include <iostream>
#include <stdexcept>
using namespace quickslot;
int main(int argc,char** argv)try {
    if(argc!=2)return 2;
    const auto root=std::filesystem::path(argv[1]);
    unsigned checks=0;
    const auto atlas=loadItemAtlas(root/"assets/icons/items.png");
    if(!atlas||atlas.width!=int(iconColumns*iconCell)||atlas.height!=int(iconRows*iconCell))throw std::runtime_error("Missing or incompatible item atlas");
    ++checks;
    for(unsigned icon=1;icon<unsigned(Icon::count);++icon) {
        const auto cell=icon-1;unsigned filled=0,empty=0;
        for(unsigned y=0;y<iconCell;++y)for(unsigned x=0;x<iconCell;++x) {
            const auto px=(cell%iconColumns)*iconCell+x,py=(cell/iconColumns)*iconCell+y;
            const auto alpha=atlas.pixels[(std::size_t(py)*atlas.width+px)*4+3];
            if(alpha)++filled;else ++empty;
            if(alpha)for(unsigned channel=0;channel<3;++channel)
                if(atlas.pixels[(std::size_t(py)*atlas.width+px)*4+channel]!=255)throw std::runtime_error("Item icon has baked shading or a colored edge");
            if((x<10||y<10||x>=iconCell-10||y>=iconCell-10)&&alpha)throw std::runtime_error("Item icon bleeds into neighboring cell");
        }
        if(filled<300||empty<1000)throw std::runtime_error("Missing silhouette or opaque background");
        ++checks;
    }
    if(loadItemAtlas(root/"tests/fixtures/keys/1.png")||loadItemAtlas(root/"assets/icons/missing.png")||loadItemAtlas(root/"tests/fixtures/keys/A.png"))throw std::runtime_error("Invalid item atlas accepted");
    ++checks;
    if(loadImageAsset(root/"assets/icons/missing.png")||loadImageAsset(root/"assets/fonts/Jost-Regular.ttf"))throw std::runtime_error("Invalid asset did not fail cleanly");
    checks+=2;
    const auto frames=loadImageAsset(root/"assets/themes/oathvein/frames.png");
    if(!frames||frames.width!=1024||frames.height!=320)throw std::runtime_error("Oathvein frame atlas missing");++checks;
    for(unsigned cell=0;cell<4;++cell) {
        unsigned filled=0,empty=0;
        for(unsigned y=0;y<320;++y)for(unsigned x=0;x<256;++x) {
            const auto alpha=frames.pixels[(y*1024+cell*256+x)*4+3];if(alpha)++filled;else ++empty;
            if((x<4||x>=252||y<6||y>=314)&&alpha)throw std::runtime_error("Theme frame bleeds out of cell");
        }
        if(filled<20000||empty<4000)throw std::runtime_error("Frame art missing or transparency lost");++checks;
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
    std::cout<<"PASS: "<<checks<<" native image checks (item atlas, theme frames, keycaps, transparent padding, missing/corrupt fallback).\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

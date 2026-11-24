#include "Hud.h"
#include <fstream>
#include "Layout.h"
#include "IconPolicy.h"
#include "EquippedState.h"
#include "Outfit.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace quickslot {
namespace {
// Palette roles. These zero-alpha tokens stand for the active theme's colors; Paint::color resolves them.
constexpr ImU32 ivory=1,silver=2,muted=3,blood=4,dark=5,selectionMark=6,equippedMark=7,poisonMark=8,depletedMark=9,chargeMark=10;
// Atlas cells hold 248 x 308 artwork; this factor matches the size of a square tile.
constexpr float atlasScale=82.f/248;
constexpr float pi=3.141592654f;
Icon iconFor(const Entry& e) {
    return baseIcon(e);
}
ImU32 iconColor(const Entry& e) {
    switch(iconFor(e)) {
    case Icon::fire:return IM_COL32(217,160,113,255);
    case Icon::frost:return IM_COL32(172,207,216,255);
    case Icon::shock:return IM_COL32(193,185,224,255);
    case Icon::restoration:return IM_COL32(220,204,154,255);
    case Icon::conjuration:return IM_COL32(190,175,207,255);
    case Icon::alteration:return IM_COL32(178,193,170,255);
    case Icon::health:return IM_COL32(222,147,137,255);
    case Icon::magicka:return IM_COL32(151,184,219,255);
    case Icon::stamina:return IM_COL32(172,198,137,255);
    case Icon::poison:return IM_COL32(179,190,139,255);
    default:return ivory;
    }
}
struct Paint {
    ImDrawList* d;ImVec2 origin;float scale,alpha;ImFont* medium;ImFont* sans;ImFont* numbers;const SkinTextures& textures;ImTextureID keyTexture;const KeyGlyphs& keyGlyphs;const Options& options;const Theme& theme;ThemeAdjustments tweaks;const ItemIconProvider& itemIcons;
    // Text sizes passed to label() are multiplied by the text size of the HUD part being drawn.
    float textFactor=1;
    // A group with its own color replaces the accent while that group is drawn.
    std::optional<ImU32> groupAccent;
    static ImU32 solid(const std::array<float,3>& c){return rgba(unsigned(c[0]*255),unsigned(c[1]*255),unsigned(c[2]*255));}
    ImU32 accent()const{return groupAccent?*groupAccent:themeAccent(theme,options);}
    ImU32 selection()const{return options.customSelectionColor?solid(options.selectionColor):theme.selection;}
    ImU32 itemColor(const Entry& e)const{return options.coloredMagic?iconColor(e):ivory;}
    ImVec2 p(float x,float y)const{return {origin.x+x*scale,origin.y+y*scale};}
    ImU32 color(ImU32 c,float opacity=1)const {
        switch(c) {
        case ivory:c=options.customTextColor?solid(options.textColor):theme.text;break;case silver:c=theme.secondary;break;case muted:c=theme.muted;break;case dark:c=theme.background;break;
        case blood:c=theme.warning;break;case selectionMark:c=selection();break;case equippedMark:c=options.customEquippedColor?ImU32(rgba(unsigned(options.equippedColor[0]*255),unsigned(options.equippedColor[1]*255),unsigned(options.equippedColor[2]*255))):theme.equipped;break;
        case poisonMark:c=theme.poison;break;case depletedMark:c=theme.depleted;break;case chargeMark:c=theme.charge;break;default:break;
        }
        return (c&0xffffff)|(ImU32(float(c>>24)*std::clamp(alpha*opacity,0.f,1.f))<<24);
    }
    ImU32 roleColor(ThemeTint tint,float opacity=1)const {
        switch(tint) {
        case ThemeTint::text:return color(ivory,opacity);
        case ThemeTint::secondary:return color(silver,opacity);
        case ThemeTint::muted:return color(muted,opacity);
        case ThemeTint::accent:return color(accent(),opacity);
        case ThemeTint::border:return color(theme.border,opacity);
        case ThemeTint::selection:return color(selectionMark,opacity);
        case ThemeTint::equipped:return color(equippedMark,opacity);
        case ThemeTint::background:return color(dark,opacity);
        case ThemeTint::warning:return color(blood,opacity);
        case ThemeTint::depleted:return color(depletedMark,opacity);
        case ThemeTint::poison:return color(poisonMark,opacity);
        case ThemeTint::charge:return color(chargeMark,opacity);
        default:return color(IM_COL32_WHITE,opacity);
        }
    }
    void drawThemeImage(float x,float y,float w,float h,const LoadedTexture& tex,const ThemeImage& meta,float opacity=1,bool flipX=false) {
        if(!tex||w<=0||h<=0)return;
        const ImU32 tint=roleColor(meta.tint,meta.opacity*opacity);
        const auto& s=meta.slice;
        const bool sliced=(s[0]||s[1]||s[2]||s[3]);
        if(meta.outset>0) {
            const float padX=sliced?std::min(w,h)*meta.outset:w*meta.outset;
            const float padY=sliced?padX:h*meta.outset;
            x-=padX;y-=padY;w+=padX*2;h+=padY*2;
        }
        if(!sliced) {
            d->AddImage(tex.id,p(x,y),p(x+w,y+h),{flipX?1.f:0.f,0},{flipX?0.f:1.f,1},tint);
            return;
        }
        const float tw=tex.width,th=tex.height;
        const float sl=std::min(float(flipX?s[2]:s[0]),tw*.5f),sr=std::min(float(flipX?s[0]:s[2]),tw*.5f);
        const float st=std::min(float(s[1]),th*.5f),sb=std::min(float(s[3]),th*.5f);
        float el=0.f,er=0.f,et=0.f,eb=0.f;
        if(meta.sliceSize>0.f) {
            const float ref=std::max({sl,sr,st,sb,1.f});
            const float k=meta.sliceSize/ref;
            el=sl*k;er=sr*k;et=st*k;eb=sb*k;
        } else {
            const float unit=std::min(1.f,std::min(w,h)/std::min(tw,th));
            el=sl*unit;er=sr*unit;et=st*unit;eb=sb*unit;
        }
        if(el+er>w&&el+er>0){const float k=w/(el+er);el*=k;er*=k;}
        if(et+eb>h&&et+eb>0){const float k=h/(et+eb);et*=k;eb*=k;}
        const std::array<float,4> xs{x,x+el,x+w-er,x+w},ys{y,y+et,y+h-eb,y+h};
        const std::array<float,4> us=flipX?std::array<float,4>{1.f,(tw-sl)/tw,sr/tw,0.f}:std::array<float,4>{0.f,sl/tw,(tw-sr)/tw,1.f};
        const std::array<float,4> vs{0,st/th,(th-sb)/th,1};
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col) {
            if(xs[col+1]<=xs[col]||ys[row+1]<=ys[row])continue;
            d->AddImage(tex.id,p(xs[col],ys[row]),p(xs[col+1],ys[row+1]),{us[col],vs[row]},{us[col+1],vs[row+1]},tint);
        }
    }
    void drawRotatedImage(float cx,float cy,float w,float h,float angle,const LoadedTexture& tex,const ThemeImage& meta,float opacity=1) {
        if(!tex||w<=0||h<=0)return;
        const float c=std::cos(angle),s=std::sin(angle),hw=w*.5f,hh=h*.5f;
        const auto rot=[&](float dx,float dy){return p(cx+dx*c-dy*s,cy+dx*s+dy*c);};
        d->AddImageQuad(tex.id,rot(-hw,-hh),rot(hw,-hh),rot(hw,hh),rot(-hw,hh),{0,0},{1,0},{1,1},{0,1},roleColor(meta.tint,meta.opacity*opacity));
    }
    void slotGlyph(std::size_t slot,float cx,float cy,float size,float opacity=1.f) {
        if(textures.glyphs.empty()||theme.assets.glyphs.empty()||size<=0)return;
        const std::size_t count=std::min(textures.glyphs.size(),theme.assets.glyphs.size());
        const std::size_t at=slot%count;
        if(!textures.glyphs[at])return;
        drawThemeImage(cx-size*.5f,cy-size*.5f,size,size,textures.glyphs[at],theme.assets.glyphs[at],opacity*tweaks.frameOpacity);
    }
    void line(float x,float y,float x2,float y2,ImU32 c=silver,float width=1) {
        d->AddLine(p(x,y),p(x2,y2),color(c),std::max(1.f,width*scale));
    }
    void text(float x,float y,const std::string& str,float size=14,ImU32 c=ivory,bool title=false,float maxWidth=0,bool center=false,bool right=false) {
        auto* font=title?medium:sans;std::string out=str;
        const auto measure=[&](const std::string& v){return font->CalcTextSizeA(size*scale,FLT_MAX,0,v.c_str()).x/scale;};
        if(maxWidth>0&&measure(out)>maxWidth) {
            while(!out.empty()&&measure(out+"…")>maxWidth) {
                auto at=out.size()-1;while(at>0&&(static_cast<unsigned char>(out[at])&0xc0)==0x80)--at;
                out.resize(at);
            }
            out+="…";
        }
        if(center)x-=measure(out)*.5f;
        else if(right)x-=measure(out);

        for(const auto offset:std::array<Point,4>{{{-.5f,0},{.5f,0},{0,-.5f},{0,.5f}}})
            d->AddText(font,size*scale,p(x+offset.x,y+offset.y),color(IM_COL32(0,0,0,165)),out.c_str());
        d->AddText(font,size*scale,p(x+.6f,y+1.2f),color(IM_COL32(0,0,0,210)),out.c_str());
        d->AddText(font,size*scale,p(x,y),color(c),out.c_str());
    }
    void label(float x,float y,const std::string& str,float size=14,ImU32 c=ivory,bool title=false,float maxWidth=0,bool center=false,bool right=false) {
        text(x,y,str,size*textFactor,c,title,maxWidth,center,right);
    }
    // Centered text wrapped at spaces onto up to `lines` lines; the last line ends with an ellipsis when it runs over.
    void paragraph(float x,float y,const std::string& str,float size,ImU32 c,float maxWidth,int lines) {
        const float px=size*textFactor;
        const auto measure=[&](const std::string& v){return sans->CalcTextSizeA(px*scale,FLT_MAX,0,v.c_str()).x/scale;};
        std::string rest=str;
        for(int line=0;line<lines&&!rest.empty();++line) {
            std::string current=rest;
            if(line+1<lines&&measure(rest)>maxWidth) {
                std::size_t cut=std::string::npos;
                for(auto at=rest.find(' ');at!=std::string::npos&&measure(rest.substr(0,at))<=maxWidth;at=rest.find(' ',at+1))cut=at;
                if(cut!=std::string::npos)current=rest.substr(0,cut);
            }
            text(x,y+float(line)*(px+2),current,px,c,false,maxWidth,true);
            rest=current.size()<rest.size()?rest.substr(std::min(rest.size(),current.size()+1)):std::string{};
        }
    }
    void circle(float x,float y,float r,ImU32 c,bool fill=false,float width=1) {
        if(fill)d->AddCircleFilled(p(x,y),r*scale,color(c),48);
        else d->AddCircle(p(x,y),r*scale,color(c),48,std::max(1.f,width*scale));
    }
    void diamond(float x,float y,float r,ImU32 c,bool fill=true,float opacity=1) {
        const ImVec2 points[]{p(x,y-r),p(x+r,y),p(x,y+r),p(x-r,y)};
        if(fill)d->AddConvexPolyFilled(points,4,color(c,opacity));
        else d->AddPolyline(points,4,color(c),ImDrawFlags_Closed,std::max(1.f,scale));
    }
    // Draws an atlas cell over a w x h frame. Corners keep their artwork proportions while edges and
    // the interior stretch. `place` maps frame coordinates to the screen, which lets diamonds rotate it.
    template<class Place> void slicedFrame(float w,float h,unsigned cell,const Place& place) {
        const float left=float(cell)*256+4,top=6,artW=248,artH=308;
        const float margin=std::min({float(theme.atlasMargin),artW*.5f,artH*.5f});
        const float edge=std::min({margin*atlasScale,w*.5f,h*.5f});
        const std::array<float,4> xs{0,edge,w-edge,w},ys{0,edge,h-edge,h};
        const std::array<float,4> us{left,left+margin,left+artW-margin,left+artW},vs{top,top+margin,top+artH-margin,top+artH};
        // The middle slice is the frame's background; the slices around it are its border.
        const auto tint=color(IM_COL32_WHITE),inside=color(IM_COL32_WHITE,options.backgroundOpacity);
        for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column) {
            if(xs[column+1]<=xs[column]||ys[row+1]<=ys[row])continue;
            const ImVec2 a{us[column]/1024,vs[row]/320},b{us[column+1]/1024,vs[row+1]/320};
            d->AddImageQuad(textures.atlas,place(xs[column],ys[row]),place(xs[column+1],ys[row]),place(xs[column+1],ys[row+1]),place(xs[column],ys[row+1]),
                a,{b.x,a.y},b,{a.x,b.y},row==1&&column==1?inside:tint);
        }
    }
    // A square frame for diamonds. The cell's top and right edges fill one half and are mirrored across the
    // diagonal into the other, so the diamond is symmetric and all four sides carry the border.
    template<class Place> void mirroredFrame(float side,unsigned cell,const Place& place) {
        const float left=float(cell)*256+4,top=6,artW=248,artH=308;
        const float margin=std::min({float(theme.atlasMargin),artW*.5f,artH*.5f}),edge=std::min(margin*atlasScale,side*.5f);
        const std::array<float,4> at{0,edge,side-edge,side},us{left,left+margin,left+artW-margin,left+artW},vs{top,top+margin,top+artH-margin,top+artH};
        const auto tint=color(IM_COL32_WHITE),inside=color(IM_COL32_WHITE,options.backgroundOpacity);
        const auto uv=[&](unsigned column,unsigned row){return ImVec2{us[column]/1024,vs[row]/320};};
        const auto point=[&](unsigned column,unsigned row){return place(at[column],at[row]);};
        for(unsigned row=0;row<3;++row)for(unsigned column=row;column<3;++column) {
            if(at[column+1]<=at[column]||at[row+1]<=at[row])continue;
            const auto c=row==1&&column==1?inside:tint;
            // Cells on the diagonal are halved along it; the last two corners coincide to draw a triangle.
            const unsigned last=column>row?column:column+1;
            d->AddImageQuad(textures.atlas,point(column,row),point(column+1,row),point(column+1,row+1),point(last,row+1),uv(column,row),uv(column+1,row),uv(column+1,row+1),uv(last,row+1),c);
            d->AddImageQuad(textures.atlas,point(row,column),point(row,column+1),point(row+1,column+1),point(row+1,last),uv(column,row),uv(column+1,row),uv(column+1,row+1),uv(last,row+1),c);
        }
    }
    void textureFrame(float x,float y,float w,float h,unsigned cell) {
        slicedFrame(w,h,cell,[&](float fx,float fy){return p(x+fx,y+fy);});
    }
    bool drawBaseLayers(float x,float y,float w,float h,const FrameLayers& meta,const SkinFrameLayers& tex) {
        if(!tex.hasBase())return false;
        if(tex.shadow)drawThemeImage(x-2,y-1,w+4,h+4,tex.shadow,meta.shadow,theme.style.shadowOpacity);
        if(tex.bg)drawThemeImage(x,y,w,h,tex.bg,meta.bg,options.backgroundOpacity);
        if(tex.border)drawThemeImage(x,y,w,h,tex.border,meta.border);
        return true;
    }
    void panelFrame(float x,float y,float w,float h) {
        if(textures.panel.hasBase()) {
            struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
            drawBaseLayers(x,y,w,h,theme.assets.panel,textures.panel);
            return;
        }
        frame(x,y,w,h,false,1,true,false);
    }
    void frame(float x,float y,float w,float h,bool selected,unsigned variant=1,bool isCard=false,bool equipped=false) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        const auto& meta=isCard?theme.assets.card:theme.assets.square;
        const auto& tex=isCard?textures.card:textures.square;
        if(!drawBaseLayers(x,y,w,h,meta,tex)) {
            if(textures.atlas) {
                textureFrame(x,y,w,h,variant);
            } else {
                const float cut=theme.style.cornerCut>=0?std::min(theme.style.cornerCut,std::min(w,h)*.45f):theme.frameStyle==1||theme.frameStyle==2?0:std::min(6.f,h*.16f);
                const ImVec2 points[]{p(x+cut,y),p(x+w-cut,y),p(x+w,y+cut),p(x+w,y+h-cut),
                    p(x+w-cut,y+h),p(x+cut,y+h),p(x,y+h-cut),p(x,y+cut)};
                d->AddConvexPolyFilled(points,8,color(dark,options.backgroundOpacity));
                d->AddPolyline(points,8,color(selected?selection():theme.border),ImDrawFlags_Closed,std::max(1.f,scale*theme.style.borderWidth));
                if(theme.frameStyle==2) {
                    d->AddRect(p(x+3,y+3),p(x+w-3,y+h-3),color(theme.border,.45f),0,0,std::max(1.f,.65f*scale));
                    for(float cx:{x,x+w})for(float cy:{y,y+h})diamond(cx,cy,2,accent());
                } else if(theme.frameStyle==3) {
                    line(x+3,y+4,x+3,y+h-4,theme.border,.7f);line(x+w-3,y+4,x+w-3,y+h-4,theme.border,.7f);
                }
            }
        }
        if(equipped&&options.equippedHighlight&&tex.equipped)drawThemeImage(x,y,w,h,tex.equipped,meta.equipped);
        if(selected) {
            if(tex.selected)drawThemeImage(x,y,w,h,tex.selected,meta.selected);
            else if(theme.style.accentLine) {
                if(textures.atlas||tex.hasBase()){line(x+4,y+h*.82f,x+w-4,y+h*.82f,accent(),1.2f);diamond(x+w*.5f,y+h*.82f,2.5f,selectionMark);}
                else {
                    const float cut=theme.style.cornerCut>=0?std::min(theme.style.cornerCut,std::min(w,h)*.45f):theme.frameStyle==1||theme.frameStyle==2?0:std::min(6.f,h*.16f);
                    line(x+cut+2,y+2,x+w-cut-2,y+2,accent(),1.5f);
                    line(x+cut+2,y+h-2,x+w-cut-2,y+h-2,accent(),1.5f);
                    diamond(x+w*.5f,y+h,2.5f,selectionMark);
                }
            }
        }
    }
    void medallion(float x,float y,float r,bool selected,bool equipped=false) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        const auto& meta=theme.assets.circle;const auto& tex=textures.circle;
        if(!drawBaseLayers(x-r,y-r,2*r,2*r,meta,tex)) {
            if(theme.style.shadowOpacity>0)circle(x,y,r+2,IM_COL32(0,0,0,unsigned(255*theme.style.shadowOpacity)),false,3);
            if(textures.atlas) {
                d->AddImageRounded(textures.atlas,p(x-r,y-r),p(x+r,y+r),{772.f/1024,6.f/320},{1020.f/1024,314.f/320},color(IM_COL32_WHITE,options.backgroundOpacity),r*scale);
            } else d->AddCircleFilled(p(x,y),r*scale,color(dark,options.backgroundOpacity),48);
            circle(x,y,r,selected?selection():theme.border,false,theme.style.borderWidth);
        }
        if(equipped&&options.equippedHighlight&&tex.equipped)drawThemeImage(x-r,y-r,2*r,2*r,tex.equipped,meta.equipped);
        if(selected) {
            if(tex.selected)drawThemeImage(x-r,y-r,2*r,2*r,tex.selected,meta.selected);
            else if(theme.style.accentLine) {
                circle(x,y,r-3,accent(),false,.8f);
                d->PathArcTo(p(x,y),(r+3)*scale,.18f*pi,.82f*pi,24);
                d->PathStroke(color(accent()),0,std::max(1.f,1.5f*scale));
                diamond(x,y-r-3,2.8f,selectionMark);
            }
        }
    }
    void lozenge(float x,float y,float r,bool selected,bool equipped=false) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        const auto& meta=theme.assets.diamond;const auto& tex=textures.diamond;
        if(!drawBaseLayers(x-r,y-r,2*r,2*r,meta,tex)) {
            if(textures.atlas) {
                // The frame's top-left corner sits at the diamond's top point; its sides run toward the right and left points.
                const float side=r*std::sqrt(2.f);
                mirroredFrame(side,1,[&](float fx,float fy){const float u=fx/side,v=fy/side;return p(x+(u-v)*r,y-r+(u+v)*r);});
            }
            else {diamond(x,y,r,dark,true,options.backgroundOpacity);diamond(x,y,r,selected?selectionMark:muted,false);}
        }
        if(equipped&&options.equippedHighlight&&tex.equipped)drawThemeImage(x-r,y-r,2*r,2*r,tex.equipped,meta.equipped);
        if(selected) {
            if(tex.selected)drawThemeImage(x-r,y-r,2*r,2*r,tex.selected,meta.selected);
            else if(theme.style.accentLine){const float inner=r-4;const ImVec2 points[]{p(x,y-inner),p(x+inner,y),p(x,y+inner),p(x-inner,y)};d->AddPolyline(points,4,color(accent()),ImDrawFlags_Closed,std::max(1.f,1.2f*scale));}
        }
    }
    void hexVertices(float x,float y,float r,ImVec2 (&pts)[6])const {
        for(unsigned k=0;k<6;++k){const float a=-pi*.5f+float(k)*pi/3.f;pts[k]=p(x+std::cos(a)*r,y+std::sin(a)*r);}
    }
    void hexagon(float x,float y,float r,bool selected,bool equipped=false) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        const auto& meta=theme.assets.hexagon;const auto& tex=textures.hexagon;
        if(!drawBaseLayers(x-r,y-r,2*r,2*r,meta,tex)) {
            ImVec2 pts[6],inner[6];hexVertices(x,y,r,pts);hexVertices(x,y,std::max(1.f,r-4.f),inner);
            if(theme.style.shadowOpacity>0){ImVec2 sh[6];hexVertices(x,y+1.5f,r+2.f,sh);d->AddConvexPolyFilled(sh,6,color(IM_COL32(0,0,0,unsigned(180*theme.style.shadowOpacity))));}
            d->AddConvexPolyFilled(pts,6,color(dark,options.backgroundOpacity));
            d->AddPolyline(pts,6,color(selected?selection():theme.border),ImDrawFlags_Closed,std::max(1.f,theme.style.borderWidth*scale*1.3f));
            d->AddPolyline(inner,6,color(selected?accent():theme.border,selected?.75f:.32f),ImDrawFlags_Closed,std::max(1.f,.9f*scale));
            if(equipped&&!options.equippedHighlight)d->AddPolyline(pts,6,color(equippedMark,.8f),ImDrawFlags_Closed,std::max(1.2f,1.5f*scale));
        }
        if(equipped&&options.equippedHighlight&&tex.equipped)drawThemeImage(x-r,y-r,2*r,2*r,tex.equipped,meta.equipped);
        if(selected&&tex.selected)drawThemeImage(x-r,y-r,2*r,2*r,tex.selected,meta.selected);
    }
    void frameless(float x,float y,float r,bool selected,bool equipped=false) {
        // Soft radial vignette keeps icons readable on bright snow without drawing a box border.
        if(options.backgroundOpacity>0)d->AddCircleFilled(p(x,y),r*.82f*scale,color(dark,options.backgroundOpacity*.42f*tweaks.frameOpacity),36);
        if(selected||equipped) {
            const float half=r*.52f,by=y+r*.86f;
            d->AddLine(p(x-half,by),p(x+half,by),color(selected?accent():equippedMark,tweaks.frameOpacity),std::max(2.f,2.4f*scale));
        }
    }
    void tile(float x,float y,float r,bool selected,unsigned shape,bool equipped=false) {
        if(shape==2)frame(x-r,y-r,2*r,2*r,selected,1,false,equipped);
        else if(shape==3)lozenge(x,y,r,selected,equipped);
        else if(shape==4)hexagon(x,y,r,selected,equipped);
        else if(shape==5)frameless(x,y,r,selected,equipped);
        else medallion(x,y,r,selected,equipped);
    }
    // charge, from 0 to 1, fills an enchanted weapon's icon from the bottom when the charge shows on icons.
    void icon(float x,float y,float size,const Entry& e,ImU32 c,float charge=-1) {
        if(e.kind==Kind::combo&&e.parts.size()==2){icon(x-size*.21f,y,size*.62f,e.parts[0],c==muted?c:itemColor(e.parts[0]));icon(x+size*.21f,y,size*.62f,e.parts[1],c==muted?c:itemColor(e.parts[1]));return;}
        // An outfit shows its body piece.
        if(e.kind==Kind::outfit)if(const auto* face=outfitFace(e)){icon(x,y,size,*face,c==muted?c:itemColor(*face),charge);return;}
        size*=tweaks.iconScale;
        if(itemIcons)if(const auto supplied=itemIcons(e)) {
            const bool unavailable=c==muted;
            const auto tint=itemIconTint(*supplied,color(c),options.itemIconColor,unavailable);
            d->AddImage(supplied->mask,p(x-size*.5f+1,y-size*.5f+2),p(x+size*.5f+1,y+size*.5f+2),{0,0},{1,1},color(IM_COL32(0,0,0,150)));
            const auto a=p(x-size*.5f,y-size*.5f),b=p(x+size*.5f,y+size*.5f);
            if(options.chargeIcons==1&&charge>=0&&!unavailable) {
                // The charged part keeps the icon's color; the spent part above it turns a dim violet grey. The line
                // moves through the middle of the square, where the artwork is.
                const float f=std::clamp(charge,0.f,1.f),cut=a.y+(b.y-a.y)*(.1f+.8f*(1-f));
                if(f<1){d->PushClipRect({a.x-2,a.y-2},{b.x+2,cut},true);d->AddImage(supplied->mask,a,b,{0,0},{1,1},color(IM_COL32(92,86,108,255),.85f));d->PopClipRect();}
                if(f>0){d->PushClipRect({a.x-2,cut},{b.x+2,b.y+2},true);d->AddImage(itemIconTexture(*supplied,options.itemIconColor,false),a,b,{0,0},{1,1},tint);d->PopClipRect();}
                return;
            }
            d->AddImage(itemIconTexture(*supplied,options.itemIconColor,unavailable),a,b,{0,0},{1,1},tint);
            return;
        }
    }
    float mark()const{return options.equippedMarkerScale;}
    float rectCornerCut(float w,float h)const {
        if(theme.style.cornerCut>0)return std::min(theme.style.cornerCut*std::min(1.f,std::min(w,h)/82.f),std::min(w,h)*.45f);
        if(theme.style.cornerCut==0||theme.frameStyle==1||theme.frameStyle==2)return 0.f;
        return std::min(6.f,h*.16f);
    }
    // An expanding outline in the accent color; t runs from 0 to 1. Shapes: 1 circle, 3 diamond, 4 hexagon, 5 frameless, otherwise rectangle.
    void pulse(float x,float y,float w,float h,unsigned shape,float t) {
        if(t<0||t>=1)return;
        const float grow=7*easeOut(t),width=std::max(1.f,(2.2f-1.2f*t)*scale);const auto c=color(accent(),(1-t)*(1-t));
        const float cx=x+w*.5f,cy=y+h*.5f;
        if(shape==1||shape==5)d->AddCircle(p(cx,cy),(w*.5f+grow)*scale,c,48,width);
        else if(shape==3){const float r=w*.5f+grow*1.4f;const ImVec2 points[]{p(cx,cy-r),p(cx+r,cy),p(cx,cy+r),p(cx-r,cy)};d->AddPolyline(points,4,c,ImDrawFlags_Closed,width);}
        else if(shape==4){ImVec2 pts[6];hexVertices(cx,cy,h*.5f+grow*1.15f,pts);d->AddPolyline(pts,6,c,ImDrawFlags_Closed,width);}
        else if(const float cut=rectCornerCut(w,h);cut>0) {
            const float x0=x-grow,y0=y-grow,x1=x+w+grow,y1=y+h+grow,oc=cut+grow*.41f;
            const ImVec2 pts[]{p(x0+oc,y0),p(x1-oc,y0),p(x1,y0+oc),p(x1,y1-oc),p(x1-oc,y1),p(x0+oc,y1),p(x0,y1-oc),p(x0,y0+oc)};
            d->AddPolyline(pts,8,c,ImDrawFlags_Closed,width);
        } else d->AddRect(p(x-grow,y-grow),p(x+w+grow,y+h+grow),c,2*scale,0,width);
    }
    // Marks a tile in one color with an outline, a tint over the frame, or both; grow moves the outline outward.
    // Shapes: 1 circle, 3 diamond, 4 hexagon, 5 frameless, otherwise rectangle.
    const FrameLayers& shapeMeta(unsigned shape,bool isCard=false)const {
        if(shape==1)return theme.assets.circle;
        if(shape==3)return theme.assets.diamond;
        if(shape==4)return theme.assets.hexagon;
        return isCard?theme.assets.card:theme.assets.square;
    }
    const SkinFrameLayers& shapeTex(unsigned shape,bool isCard=false)const {
        if(shape==1)return textures.circle;
        if(shape==3)return textures.diamond;
        if(shape==4)return textures.hexagon;
        return isCard?textures.card:textures.square;
    }
    void highlight(float x,float y,float w,float h,unsigned shape,ImU32 c,bool outline,bool tinted,float grow=0) {
        const float cx=x+w*.5f,cy=y+h*.5f,r=w*.5f+grow;
        const auto tint=color(c,.16f),halo=color(c,theme.style.glowOpacity*.65f),edge=color(c,.92f);
        const float thin=std::max(1.1f,1.35f*scale),wide=std::max(2.2f,3.0f*scale);
        if(shape==1) {
            if(tinted)d->AddCircleFilled(p(cx,cy),w*.5f*scale,tint,48);
            if(outline){d->AddCircle(p(cx,cy),r*scale,halo,48,wide);d->AddCircle(p(cx,cy),r*scale,edge,48,thin);}
        } else if(shape==5) {
            if(tinted)d->AddCircleFilled(p(cx,cy),w*.45f*scale,tint,40);
            if(outline) {
                const float half=w*.34f+grow,by=cy+h*.44f+grow*.4f;
                d->AddLine(p(cx-half,by),p(cx+half,by),halo,wide);
                d->AddLine(p(cx-half,by),p(cx+half,by),edge,thin);
            }
        } else if(shape==4) {
            ImVec2 fill[6],pts[6];hexVertices(cx,cy,h*.5f,fill);hexVertices(cx,cy,h*.5f+grow*1.15f,pts);
            if(tinted)d->AddConvexPolyFilled(fill,6,tint);
            if(outline){d->AddPolyline(pts,6,halo,ImDrawFlags_Closed,wide);d->AddPolyline(pts,6,edge,ImDrawFlags_Closed,thin);}
        } else if(shape==3) {
            const float inner=w*.5f,outer=w*.5f+grow*1.4f;
            const ImVec2 fill[]{p(cx,cy-inner),p(cx+inner,cy),p(cx,cy+inner),p(cx-inner,cy)},points[]{p(cx,cy-outer),p(cx+outer,cy),p(cx,cy+outer),p(cx-outer,cy)};
            if(tinted)d->AddConvexPolyFilled(fill,4,tint);
            if(outline){d->AddPolyline(points,4,halo,ImDrawFlags_Closed,wide);d->AddPolyline(points,4,edge,ImDrawFlags_Closed,thin);}
        } else if(const float cut=rectCornerCut(w,h);cut>0) {
            if(tinted) {
                const ImVec2 fill[]{p(x+cut,y),p(x+w-cut,y),p(x+w,y+cut),p(x+w,y+h-cut),p(x+w-cut,y+h),p(x+cut,y+h),p(x,y+h-cut),p(x,y+cut)};
                d->AddConvexPolyFilled(fill,8,tint);
            }
            if(outline) {
                const float x0=x-grow,y0=y-grow,x1=x+w+grow,y1=y+h+grow,oc=cut+grow*.41f;
                const ImVec2 pts[]{p(x0+oc,y0),p(x1-oc,y0),p(x1,y0+oc),p(x1,y1-oc),p(x1-oc,y1),p(x0+oc,y1),p(x0,y1-oc),p(x0,y0+oc)};
                d->AddPolyline(pts,8,halo,ImDrawFlags_Closed,wide);
                d->AddPolyline(pts,8,edge,ImDrawFlags_Closed,thin);
            }
        } else {
            if(tinted)d->AddRectFilled(p(x,y),p(x+w,y+h),tint,2*scale);
            if(outline){d->AddRect(p(x-grow,y-grow),p(x+w+grow,y+h+grow),halo,2*scale,0,wide);d->AddRect(p(x-grow,y-grow),p(x+w+grow,y+h+grow),edge,2*scale,0,thin);}
        }
    }
    // The tile of an equipped item, marked as the Equipped highlight setting asks.
    // When a theme provides its own crafted equipped overlay texture, it replaces the procedural outline to avoid stacked borders.
    void equippedHighlight(float x,float y,float w,float h,unsigned shape,bool isCard=false) {
        const unsigned mode=options.equippedHighlight;
        if(!mode)return;
        if(shape!=5&&shapeTex(shape,isCard).equipped) {
            if(mode!=1)highlight(x,y,w,h,shape,equippedMark,false,true,0);
            return;
        }
        highlight(x,y,w,h,shape,equippedMark,mode!=2,mode!=1);
    }
    // Highlights the active group tile while browsing; prefers the theme's own luminous metallic selected overlay when provided.
    void previewHighlight(float x,float y,float w,float h,unsigned shape,bool isCard=false,float grow=2.f) {
        if(shape!=5) {
            const auto& tex=shapeTex(shape,isCard);
            if(tex.selected) {
                ThemeImage meta=shapeMeta(shape,isCard).selected;
                meta.tint=ThemeTint::accent;
                drawThemeImage(x,y,w,h,tex.selected,meta,tweaks.frameOpacity);
                return;
            }
        }
        highlight(x,y,w,h,shape,accent(),true,false,grow);
    }
    void equippedMarker(float x,float y,float r) {
        if(textures.markers.equipped){drawThemeImage(x-r*1.5f,y-r*1.5f,r*3,r*3,textures.markers.equipped,theme.assets.markers.equipped);return;}
        diamond(x,y,r,equippedMark);
    }
    // A small arrowhead: the consumable its Use key takes.
    void ready(float x,float y,float r,ImU32 c){
        if(textures.markers.ready){drawThemeImage(x-r*1.5f,y-r*1.5f,r*3,r*3,textures.markers.ready,theme.assets.markers.ready);return;}
        d->AddTriangleFilled(p(x-r*.8f,y-r),p(x+r,y),p(x-r*.8f,y+r),color(c));
    }
    // A clockwise shade over an icon covering the remaining share of a cooldown.
    // A small soul gem at an icon's upper left marks an enchanted weapon; it fills with the remaining charge and turns
    // red at a fifth or less. An empty gem keeps a red outline.
    void soulGem(float x,float y,float r,float fraction) {
        if(options.chargeIcons!=2||!(fraction>=0))return;
        const float f=std::clamp(fraction,0.f,1.f),gx=x-r*.8f,gy=y-r*.56f,s=std::max(3.5f,r*.24f),line=std::max(1.f,scale);
        if(textures.markers.soulGem) {
            const float w=s*2.2f,h=s*2.6f;
            drawThemeImage(gx-w*.5f,gy-h*.5f,w,h,textures.markers.soulGem,theme.assets.markers.soulGem,f>0?.35f+.65f*f:.4f);
            return;
        }
        const ImVec2 points[]{p(gx,gy-s*1.25f),p(gx+s*.8f,gy-s*.35f),p(gx+s*.55f,gy+s),p(gx-s*.55f,gy+s),p(gx-s*.8f,gy-s*.35f)};
        d->AddConvexPolyFilled(points,5,color(IM_COL32(10,8,16,210)));
        if(f>0) {
            const float cut=gy+s-2.25f*s*f;
            d->PushClipRect(p(gx-s,cut),p(gx+s,gy+s+1),true);d->AddConvexPolyFilled(points,5,color(f<=.2f?ImU32(theme.warning):chargeMark,.95f));d->PopClipRect();
            d->AddLine(p(gx-s*.3f,gy-s*.6f),p(gx+s*.05f,gy-s*.95f),color(IM_COL32(220,206,255,255),.6f),line);
        }
        d->AddPolyline(points,5,f>0?color(IM_COL32(0,0,0,200)):color(theme.warning,.9f),ImDrawFlags_Closed,line*1.2f);
    }
    // Five diamonds for enchantment charge, a fifth each, from left; a partly used one is dimmed.
    static float pipsWidth(float size){return size*.62f*2+size*1.9f*4;}
    void chargePips(float left,float y,float fraction,float size) {
        const float f=std::clamp(fraction,0.f,1.f),pitch=size*1.9f,r=size*.62f;
        for(int i=0;i<5;++i) {
            const float gx=left+r+float(i)*pitch,lit=std::clamp(f*5-float(i),0.f,1.f);
            if(textures.markers.chargePip) {
                const float op=lit>0?(lit<1?.35f+.65f*lit:1.f):.25f;
                drawThemeImage(gx-r*1.2f,y-r*1.2f,r*2.4f,r*2.4f,textures.markers.chargePip,theme.assets.markers.chargePip,op);
                continue;
            }
            diamond(gx,y,r+.8f,dark,true);
            if(lit>0)diamond(gx,y,r,f<=.2f?blood:chargeMark,true,lit<1?.35f+.65f*lit:1.f);
            else diamond(gx,y,r,f>0?muted:blood,false,.7f);
        }
    }
    // A row's status line centered on x, with its enchantment charge on a line of its own beneath.
    bool showsCharge(const Row& row)const{return options.showCharge&&row.weapon.charge>=0;}
    static float chargeRow(float size,float text){return size*text*1.1f;}
    void statusLine(float x,float y,const Row& row,float size,ImU32 c,float maxWidth) {
        const auto status=rowStatus(row);
        label(x,y,status,size,c,false,maxWidth,true);
        if(!showsCharge(row))return;
        const float pip=size*textFactor*.54f,line=status.empty()?0:size*textFactor;
        chargePips(x-pipsWidth(pip)*.5f,y+line+chargeRow(size,textFactor)*.5f,row.weapon.charge,pip);
    }
    void sweep(float x,float y,float r,float fraction) {
        if(!(fraction>0))return;
        const int segments=std::max(2,int(32*std::min(fraction,1.f)));const float span=2*pi*std::min(fraction,1.f);const auto shade=color(IM_COL32(0,0,0,150));
        for(int i=0;i<segments;++i) {
            const float a=-pi*.5f+span*float(i)/float(segments),b=-pi*.5f+span*float(i+1)/float(segments);
            d->AddTriangleFilled(p(x,y),p(x+std::cos(a)*r,y+std::sin(a)*r),p(x+std::cos(b)*r,y+std::sin(b)*r),shade);
        }
    }
    void rule(float x,float y,float half=70) {
        if(textures.divider) {
            const float w=half*2,h=std::clamp(w*(textures.divider.height/textures.divider.width),6.f,28.f);
            drawThemeImage(x-w*.5f,y-h*.5f,w,h,textures.divider,theme.assets.divider);
            return;
        }
        const auto edge=(theme.border&0x00ffffffu)|0x82000000u;
        line(x-half,y,x-7,y,edge,.7f);
        line(x+7,y,x+half,y,edge,.7f);diamond(x,y,2.5f,accent());
    }
    void key(float x,float y,unsigned scan,bool selected,float height=18,bool show=true) {
        if(!show)return;
        if(scan>=256&&scan<288&&(scan-256)%16<4) {
            const auto direction=(scan-256)%16;const float r=height*.5f;
            const ImVec2 offsets[]{{0,-1},{1,0},{0,1},{-1,0}};
            d->AddRectFilled(p(x-r*.32f,y+r-r*.32f),p(x+r*.32f,y+r+r*.32f),color(IM_COL32(70,72,73,220)));
            for(unsigned i=0;i<4;++i){const auto v=offsets[i];d->AddRectFilled(p(x+v.x*r*.62f-r*.24f,y+r+v.y*r*.62f-r*.24f),p(x+v.x*r*.62f+r*.24f,y+r+v.y*r*.62f+r*.24f),color(i==direction?(selected?ivory:silver):IM_COL32(70,72,73,200)),scale);}
            return;
        }
        if(options.keyIcons&&keyTexture&&scan<keyGlyphs.size()&&keyGlyphs[scan].aspect>0) {
            const auto& glyph=keyGlyphs[scan];const float w=height*glyph.aspect;

            d->AddImage(keyTexture,p(x-w*.5f,y),p(x+w*.5f,y+height),{glyph.u0,glyph.v0},{glyph.u1,glyph.v1},color(IM_COL32_WHITE));
            return;
        }
        if(scan>=276&&scan<=279) {
            const float r=height*.42f,cy=y+height*.5f;const auto c=selected?ivory:silver;
            if(scan==276){line(x-r,cy-r,x+r,cy+r,c,1.5f);line(x-r,cy+r,x+r,cy-r,c,1.5f);}
            if(scan==277)d->AddCircle(p(x,cy),r*scale,color(c),24,1.5f*scale);
            if(scan==278)d->AddRect(p(x-r,cy-r),p(x+r,cy+r),color(c),0,0,1.5f*scale);
            if(scan==279)d->AddTriangle(p(x,cy-r),p(x+r,cy+r),p(x-r,cy+r),color(c),1.5f*scale);
            return;
        }
        const auto label=keyLabel(scan);
        text(x,y,label,12,selected?ivory:silver,true,0,true);
    }
    float keyWidth(unsigned scan,float height)const {
        if(scan>=256&&scan<288&&(scan-256)%16<4)return height;
        if(options.keyIcons&&keyTexture&&scan<keyGlyphs.size()&&keyGlyphs[scan].aspect>0)return height*keyGlyphs[scan].aspect;
        if(scan>=276&&scan<=279)return height;
        return std::max(16.f,sans->CalcTextSizeA(12*scale,FLT_MAX,0,keyLabel(scan).c_str()).x/scale+8);
    }
    // The width a group's key or button label takes; zero when it shows none.
    float groupKeyWidth(std::size_t group,bool controller,bool vr,float height=18,bool show=true)const {
        if(!show)return 0;
        if(!controller)return options.keys[group]?keyWidth(options.keys[group],height):0;
        const auto binding=(vr?options.vrGroups:options.controllerGroups)[group];const auto style=vr?2u:options.controllerStyle;
        if(binding.button==PadButton::none)return 0;
        if(binding.modifier==PadButton::none)return keyWidth(padGlyph(binding.button,style),height);
        return keyWidth(padGlyph(binding.modifier,style),height*.85f)+keyWidth(padGlyph(binding.button,style),height*.85f)+12;
    }
    void groupKey(float x,float y,std::size_t group,bool selected,bool controller,bool vr,float height=18,bool show=true) {
        if(!show)return;
        if(!controller){if(options.keys[group])key(x,y,options.keys[group],selected,height,true);return;}
        const auto binding=(vr?options.vrGroups:options.controllerGroups)[group];
        const auto style=vr?2u:options.controllerStyle;
        if(binding.button==PadButton::none)return;
        if(binding.modifier==PadButton::none){key(x,y,padGlyph(binding.button,style),selected,height,true);return;}
        const float h=height*.85f;
        const auto mod=padGlyph(binding.modifier,style),button=padGlyph(binding.button,style);
        const float left=keyWidth(mod,h),right=keyWidth(button,h),start=x-(left+right+12)*.5f;
        key(start+left*.5f,y,mod,selected,h,true);text(start+left+6,y,"+",11,silver,false,0,true);key(start+left+12+right*.5f,y,button,selected,h,true);
    }
    void badge(float x,float y,const Entry& entry,int quantity,float cooldown,float size=13,bool rightAligned=false,WeaponStatus weapon={}) {
        const int count=badgeQuantity(entry,quantity,weapon,options);
        const bool poisoned=options.showPoison&&weapon.poisoned;
        if(count<0&&cooldown<=0&&!poisoned)return;
        const auto label=count>=0?(count>999?std::string("999+"):std::to_string(count)):cooldown>0?std::to_string(int(std::ceil(cooldown)))+"s":std::string{};
        auto* font=numbers?numbers:sans;
        const auto textWidth=font->CalcTextSizeA(size*scale,FLT_MAX,0,label.c_str()).x/scale;
        const float poisonWidth=poisoned?size+3:0,w=textWidth+poisonWidth+8;
        if(rightAligned)x-=w*.5f;
        const float left=x-w*.5f;
        if(poisoned) {
            if(textures.markers.poison) {
                drawThemeImage(left+4,y,size,size,textures.markers.poison,theme.assets.markers.poison);
            } else {
                const auto green=color(poisonMark);const float cx=left+4+size*.5f,cy=y+size*.65f,r=size*.26f;
                const auto droplet=[&]{d->PathLineTo(p(cx,cy-size*.65f));d->PathArcTo(p(cx,cy),r*scale,-pi*.2f,pi*1.2f,12);};
                droplet();d->PathFillConvex(green);
                droplet();d->PathStroke(color(IM_COL32(12,18,10,210)),ImDrawFlags_Closed,std::max(1.f,scale*.7f));
            }
        }
        if(!label.empty()) {
            auto* savedSans=sans;sans=font;
            text(left+4+poisonWidth+textWidth*.5f,y,label,size,count==0?depletedMark:ivory,false,0,true);
            sans=savedSans;
        }
    }
};
}
ImFont* Hud::loadCachedFont(const std::filesystem::path& path,float size,bool allowFallback) {
    if(path.empty())return nullptr;
    std::error_code error;
    if(!std::filesystem::is_regular_file(path,error))return nullptr;
    const auto fileSize=std::filesystem::file_size(path,error);
    if(error||fileSize<12||fileSize>32*1024*1024)return nullptr;
    const auto mtime=std::filesystem::last_write_time(path,error);
    auto& io=ImGui::GetIO();
    auto& entry=fontCache_[path];
    const bool sameFile=!entry.bytes.empty()&&entry.size==fileSize&&entry.mtime==mtime;
    if(entry.font&&sameFile)return entry.font;
    if(!sameFile) {
        std::vector<char> bytes;
        {std::ifstream file(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});}
        if(bytes.size()<12)return nullptr;
        const std::string_view magic(bytes.data(),4);
        if(magic!=std::string_view("\0\1\0\0",4)&&magic!="OTTO"&&magic!="ttcf")return nullptr;
        if(!entry.bytes.empty())retiredFontBytes_.push_back(std::move(entry.bytes));
        entry.bytes=std::move(bytes);entry.size=fileSize;entry.mtime=mtime;
    } else {
        const std::string_view magic(entry.bytes.data(),4);
        if(magic!=std::string_view("\0\1\0\0",4)&&magic!="OTTO"&&magic!="ttcf")return nullptr;
    }
    if(entry.font&&entry.font!=sans&&entry.font!=medium&&entry.font!=io.FontDefault)io.Fonts->RemoveFont(entry.font);
    ImFontConfig config;config.FontDataOwnedByAtlas=false;config.MergeMode=false;
    entry.font=io.Fonts->AddFontFromMemoryTTF(entry.bytes.data(),int(entry.bytes.size()),size,&config);
    if(entry.font&&allowFallback) {
        for(const auto& fb:fallbacks_) {
            if(fb==path)continue;
            auto it=fontCache_.find(fb);
            if(it==fontCache_.end()||it->second.bytes.size()<12)continue;
            ImFontConfig merge;merge.FontDataOwnedByAtlas=false;merge.MergeMode=true;
            io.Fonts->AddFontFromMemoryTTF(it->second.bytes.data(),int(it->second.bytes.size()),size,&merge);
        }
    }
    return entry.font;
}
void Hud::initialize(const std::string& directory,const std::string& installedFontDirectory) {
    auto& io=ImGui::GetIO();
    fontDirectory_=std::filesystem::path(directory);
    installedFontDirectory_=std::filesystem::path(installedFontDirectory);
    themesDirectory_=fontDirectory_.parent_path()/"themes";
    const auto latin=fontDirectory_/"Jost-Regular.ttf";
    fallbacks_={latin};
    std::error_code ec;
    if(std::filesystem::is_directory(fontDirectory_/"fallback",ec)) {
        std::vector<std::filesystem::path> extra;
        for(const auto& file:std::filesystem::directory_iterator(fontDirectory_/"fallback",ec))
            if(file.is_regular_file()&&(file.path().extension()==".ttf"||file.path().extension()==".otf"))extra.push_back(file.path());
        std::sort(extra.begin(),extra.end());fallbacks_.insert(fallbacks_.end(),extra.begin(),extra.end());
    }
    for(const auto& fb:fallbacks_) {
        std::error_code error;
        if(!std::filesystem::is_regular_file(fb,error))continue;
        const auto sz=std::filesystem::file_size(fb,error);
        if(error||sz<12||sz>32*1024*1024)continue;
        auto& entry=fontCache_[fb];
        if(entry.bytes.empty()){std::ifstream file(fb,std::ios::binary);entry.bytes.assign(std::istreambuf_iterator<char>(file),{});}
        entry.size=sz;entry.mtime=std::filesystem::last_write_time(fb,error);
    }
    medium=loadCachedFont(fontDirectory_/"Jost-Medium.ttf",24,true);
    if(!medium)medium=io.Fonts->AddFontDefault();
    sans=loadCachedFont(latin,18,true);
    if(!sans)sans=io.Fonts->AddFontDefault();
    io.FontDefault=sans;
    reloadThemes(themesDirectory_);
}
void Hud::reloadThemes(const std::filesystem::path& themesDirectory) {
    if(!themesDirectory.empty())themesDirectory_=themesDirectory;
    themes_.load(themesDirectory_);
    if(!detected_.empty()&&!themes_.contains(detected_))detected_.clear();
    const auto packFont=[&](const std::filesystem::path& path,ImFont* fallback) {
        if(path.empty())return fallback;
        auto* loaded=loadCachedFont(path,20,true);
        return loaded?loaded:fallback;
    };
    std::map<std::string,Skin> nextSkins;
    for(const auto& t:themes_.all()) {
        auto* body=packFont(t.font,sans);
        auto* title=packFont(t.titleFont.empty()?t.font:t.titleFont,medium);
        auto* number=packFont(t.numberFont,body);
        Skin skin{body,title,number,{}};
        if(const auto prev=skins_.find(t.id);prev!=skins_.end())skin.textures=prev->second.textures;
        nextSkins.emplace(t.id,std::move(skin));
    }
    skins_=std::move(nextSkins);
    fonts_.clear();
    const auto discover=[&](const std::filesystem::path& folder,const char* prefix) {
        std::error_code error;if(folder.empty()||!std::filesystem::is_directory(folder,error))return;
        std::vector<std::filesystem::path> files;
        for(const auto& file:std::filesystem::directory_iterator(folder,error))
            if(file.is_regular_file(error)&&(file.path().extension()==".ttf"||file.path().extension()==".otf"))files.push_back(file.path());
        std::sort(files.begin(),files.end());
        for(const auto& file:files) {
            if(fonts_.size()>=24)break;
            const auto name=file.filename().string();
            if(name.size()>96||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}))continue;
            if(isNonTextFont(name))continue;
            if(auto* font=loadCachedFont(file,20,true))fonts_.push_back({prefix+name,file.stem().string(),font});
        }
    };
    discover(installedFontDirectory_,"ui/");discover(fontDirectory_,"quickslot/");
    ++themeRevision_;
}
void Hud::drawItemIcon(ImDrawList* list,ImVec2 center,float size,const Entry& entry,const Options& o,bool available)const {
    if(entry.kind==Kind::combo&&entry.parts.size()==2) {
        drawItemIcon(list,{center.x-size*.21f,center.y},size*.62f,entry.parts[0],o,available);
        drawItemIcon(list,{center.x+size*.21f,center.y},size*.62f,entry.parts[1],o,available);return;
    }
    if(entry.kind==Kind::outfit)if(const auto* face=outfitFace(entry)){drawItemIcon(list,center,size,*face,o,available);return;}
    if(!list||!itemIcons_)return;
    const auto supplied=itemIcons_(entry);if(!supplied)return;
    const auto& palette=theme(o);const auto role=o.coloredMagic?iconColor(entry):ivory;
    const ImU32 base=!available?palette.muted:role==ivory?palette.text:role;
    list->AddImage(itemIconTexture(*supplied,o.itemIconColor,!available),{center.x-size*.5f,center.y-size*.5f},{center.x+size*.5f,center.y+size*.5f},{0,0},{1,1},
        itemIconTint(*supplied,base|0xff000000u,o.itemIconColor,!available));
}
EquipmentMetrics Hud::equipmentMetrics(const EquippedDisplay& state,const Options& o)const {
    EquipmentMetrics metrics;auto* font=bodyFont(o);if(!font)return metrics;
    metrics.measured=true;unsigned index=0;const float t=o.equipmentTextScale;
    forEachEquipmentRow(state,o,[&](const char* role,const std::optional<Row>& row) {
        const auto name=equipmentTextName(row,o.equipmentStyle!=4,o);
        if(index<metrics.textWidths.size())metrics.textWidths[index++]=std::max(font->CalcTextSizeA(11.5f*t,FLT_MAX,0,tr(role)).x,
            font->CalcTextSizeA(17*t,FLT_MAX,0,name.c_str()).x)+2;
    },o.equipmentLayout==freeLayout);
    return metrics;
}
void Hud::draw(const Snapshot& state,const Options& o,double now,ImVec2 origin,ImVec2 viewport,ImDrawList* list,HudBounds* bounds) {
    if(bounds)*bounds={};
    const auto displacement=motion_.update(state,o,now);const auto visibilityTarget=fade_.update(state,o,now);
    // Editor previews show the settled layout; gameplay eases the HUD in and the selector open and closed.
    const bool animate=o.animateHud&&!state.editorPreview;
    if(!state.browsing){wheelHighlightSet_=false;wedgeLiftSet_=false;}
    const auto visibility=animate?shown_.update(visibilityTarget,now,.12f,.15f):visibilityTarget;
    const float selectorTarget=state.browsing?1.f:0.f,selectorShown=animate?selectorOpen_.update(selectorTarget,now,.1f,.08f):selectorTarget;
    const auto equipmentAlpha=equipmentFade_.update(state,o,now);
    if(visibility<=0&&equipmentAlpha<=0)return;
    // A tip takes the notice's place by the widget while no notice is showing and the selector is closed.
    const bool noticed=!state.notice.empty()&&now<state.noticeUntil,tipped=!noticed&&!state.tip.empty()&&!state.browsing;
    const bool notification=noticed||tipped;
    const auto screen=viewport.x>0&&viewport.y>0?viewport:ImGui::GetIO().DisplaySize;
    const auto metrics=equipmentMetrics(state.equipment,o);
    const bool eqIncludeEmpty=state.placementPreview&&o.equipmentLayout==freeLayout;
    const unsigned eqRows=equipmentRows(state.equipment,o,eqIncludeEmpty);
    const auto eqRoles=activeEquipmentRoles(state.equipment,o,eqIncludeEmpty);
    // The display position of the group in use; the Free arrangement gathers the selector and notices around its tile.
    std::size_t activeSlot=0;for(std::size_t i=0;i<o.groupOrder.size();++i)if(o.groupOrder[i]==state.activeGroup)activeSlot=i;
    auto geometry=hudGeometry(o,screen.x,screen.y,eqRows,metrics,state.rows.size(),activeSlot,eqRoles);
    if(state.editorPreview&&!state.placementPreview&&list){const auto display=ImGui::GetIO().DisplaySize;geometry=previewGeometry(o,display.x,display.y,screen.x,screen.y,state.browsing,!state.selectorOnly,state.equipmentOnly,eqRows,metrics,state.rows.size(),state.previewZoom,activeSlot,eqRoles);}
    const auto& focus=geometry.focus;
    const float scale=geometry.scale;
    const auto& theme=this->theme(o);const auto& skin=skins_.at(theme.id);
    Paint p{list?list:ImGui::GetBackgroundDrawList(),
        {origin.x+geometry.origin.x+geometry.selectorOrigin.x*scale,origin.y+geometry.origin.y+geometry.selectorOrigin.y*scale},
        geometry.selectorScale,o.selectorOpacity*visibility,chosenFont(o)?chosenFont(o):skin.title,chosenFont(o)?chosenFont(o):skin.body,chosenFont(o)?chosenFont(o):skin.number,skin.textures,keyTexture,keyGlyphs,o,theme,adjustments(o),itemIcons_,selectorTextFactor(o)};
    p.tweaks.iconScale=o.iconScale;
    const auto collect=[&](int start,Bounds& box){for(int i=start;i<p.d->VtxBuffer.Size;++i){const auto v=p.d->VtxBuffer[i].pos;box.include({v.x,v.y});}};
    const int selectorStart=p.d->VtxBuffer.Size;const float text=selectorTextFactor(o);
    const float center=geometry.selectorWidth*.5f;
    const auto group=std::min(state.activeGroup,Arsenal::count-1);
    const auto index=state.browsing?state.preview:state.selected;
    const Row* active=index<state.rows.size()?&state.rows[index]:nullptr;
    const bool selectorVisible=(state.browsing||selectorShown>0)&&visibility>0;
    const auto groupColor=[&](std::size_t id)->std::optional<ImU32>{return o.groupColored[id]?std::optional(Paint::solid(o.groupColors[id])):std::nullopt;};
    Paint selectorPaint=p;
    {
        const float open=easeOut(selectorShown),zoom=.94f+.06f*open;
        selectorPaint.alpha*=open;selectorPaint.scale*=zoom;
        selectorPaint.origin.x+=geometry.selectorWidth*geometry.selectorScale*(1-zoom)*.5f;selectorPaint.origin.y+=geometry.selectorHeight*geometry.selectorScale*(1-zoom)*.5f;
    }
    if(selectorVisible) {
        // While closing, the selector keeps showing the entry that was highlighted.
        auto p=selectorPaint;p.groupAccent=groupColor(group);
        const auto index=state.browsing||state.preview<state.rows.size()?state.preview:state.selected;
        const Row* active=index<state.rows.size()?&state.rows[index]:nullptr;
        const auto n=state.rows.size();
        const float clipPad=o.selector==wheelSelector?48.f:0.f;
        p.d->PushClipRect(p.p(-clipPad,-32.f-clipPad),p.p(geometry.selectorWidth+clipPad,geometry.selectorHeight+24.f+clipPad),true);
        const auto node=[&](const Row& row,float x,float y,float r,bool selected,unsigned shape,float opacity) {
            auto nodePaint=p;nodePaint.alpha*=opacity;
            nodePaint.tile(x,y,r,selected,shape,row.equipped);
            nodePaint.icon(x,y,r*1.48f,rowIconEntry(row),row.available?nodePaint.itemColor(rowIconEntry(row)):muted,row.weapon.charge);
            nodePaint.sweep(x,y,r*.74f*nodePaint.tweaks.iconScale,cooldowns_.fraction(row.entry.form,row.cooldown));nodePaint.soulGem(x,y,r*.74f*nodePaint.tweaks.iconScale,row.weapon.charge);
            // A diamond keeps its marker in its lower point and its count inside its lower right edge.
            const bool lozenge=shape==3;
            const float marker=lozenge?x:x+(isStack(row.entry)||row.cooldown>0||(o.showAmmo&&row.weapon.ammunition>=0)||(o.showPoison&&row.weapon.poisoned)?-1.f:1.f)*r*.7f,markerY=y+r*(lozenge?.72f:.7f);
            if(!row.available&&!isStack(row.entry)){nodePaint.circle(marker,markerY,lozenge?4.5f:6.f,IM_COL32(22,18,18,240),true);nodePaint.line(marker-3,markerY+3,marker+3,markerY-3,blood,1.6f);}
            else if(row.equipped)nodePaint.equippedMarker(marker,markerY,(lozenge?2.8f:3.5f)*nodePaint.mark());
            if(lozenge)nodePaint.badge(x+r*.43f,y+r*.16f,row.entry,row.quantity,row.cooldown,selected?14:11,true,row.weapon);
            else nodePaint.badge(x+r*.58f,y+r*.5f,row.entry,row.quantity,row.cooldown,selected?15:12,false,row.weapon);
        };
        const auto layout=selectorGeometry(o,n,displacement);
        if(active) {
            if(o.selector==wheelSelector&&layout.segments) {
                // A ring of segments, one for each entry, from the first slot clockwise around a hub.
                const auto hub=layout.hub;const float turn=2*pi/float(std::max<std::size_t>(n,1)),frameAlpha=p.tweaks.frameOpacity;
                const auto center=p.p(hub.x,hub.y);const float thickness=layout.outer-layout.inner;
                const auto middleOf=[&](float slot){return -pi*.5f+layout.rotation+slot*turn;};
                // A segment between two radii around a middle angle. The gap between segments keeps one width from the
                // hub to the rim; a segment that reaches the center ends in a point set back by the gap.
                const auto segment=[&](float middle,float inner,float outer) {
                    const auto inset=[&](float radius){return n>1&&radius>0?std::asin(std::min(1.f,layout.wedgeGap*.5f/radius)):0.f;};
                    const int steps=std::max(4,int(turn*outer*p.scale/5));
                    p.d->PathArcTo(center,outer*p.scale,middle-turn*.5f+inset(outer),middle+turn*.5f-inset(outer),steps);
                    const float apex=n>1&&turn<pi?layout.wedgeGap*.5f/std::sin(turn*.5f):0;
                    if(inner>apex+1)p.d->PathArcTo(center,inner*p.scale,middle+turn*.5f-inset(inner),middle-turn*.5f+inset(inner),steps);
                    else p.d->PathLineTo({center.x+std::cos(middle)*apex*p.scale,center.y+std::sin(middle)*apex*p.scale});
                    std::vector<ImVec2> points(p.d->_Path.begin(),p.d->_Path.end());p.d->PathClear();return points;
                };
                // A lone entry takes the whole ring.
                const auto fill=[&](const std::vector<ImVec2>& shape,float outer,ImU32 c) {
                    if(n>1)p.d->AddConcavePolyFilled(shape.data(),int(shape.size()),c);
                    else if(layout.inner>0)p.d->AddCircle(center,(layout.inner+outer)*.5f*p.scale,c,96,(outer-layout.inner)*p.scale);
                    else p.d->AddCircleFilled(center,outer*p.scale,c,96);
                };
                const auto stroke=[&](const std::vector<ImVec2>& shape,float outer,ImU32 c,float width) {
                    if(n>1)p.d->AddPolyline(shape.data(),int(shape.size()),c,ImDrawFlags_Closed,std::max(1.f,width*p.scale));
                    else for(const float radius:{layout.inner,outer})if(radius>0)p.d->AddCircle(center,radius*p.scale,c,96,std::max(1.f,width*p.scale));
                };
                const bool outline=o.wheelHighlight==0||o.wheelHighlight==2,filled=o.wheelHighlight==0||o.wheelHighlight==1,rim=o.wheelHighlight==3;
                const auto& wTex=p.textures.wheel;const auto& wMeta=theme.assets.wheel;
                const auto drawWedgeRing=[&](float middle,float inner,float outer) {
                    const float insetOuter=n>1&&outer>0?std::asin(std::min(1.f,layout.wedgeGap*.5f/outer)):0.f;
                    const float apex=n>1&&turn<pi?layout.wedgeGap*.5f/std::sin(turn*.5f):0.f;
                    const bool hasInner=inner>apex+1.f;
                    const float insetInner=hasInner&&n>1&&inner>0?std::asin(std::min(1.f,layout.wedgeGap*.5f/inner)):0.f;
                    const float spanOuter=std::max(0.f,turn-2*insetOuter),spanInner=hasInner?std::max(0.f,turn-2*insetInner):0.f;
                    const int steps=std::max(6,int(turn*outer*p.scale/6));
                    const ImU32 tint=p.roleColor(wMeta.ringBg.tint,wMeta.ringBg.opacity*o.backgroundOpacity*o.wheelOpacity*frameAlpha);
                    const ImVec2 apexPt=p.p(hub.x+std::cos(middle)*apex,hub.y+std::sin(middle)*apex);
                    for(int k=0;k<steps;++k) {
                        const float t0=float(k)/float(steps),t1=float(k+1)/float(steps);
                        const float ao0=middle-turn*.5f+insetOuter+t0*spanOuter,ao1=middle-turn*.5f+insetOuter+t1*spanOuter;
                        const ImVec2 po0=p.p(hub.x+std::cos(ao0)*outer,hub.y+std::sin(ao0)*outer);
                        const ImVec2 po1=p.p(hub.x+std::cos(ao1)*outer,hub.y+std::sin(ao1)*outer);
                        ImVec2 pi0=apexPt,pi1=apexPt;
                        if(hasInner) {
                            const float ai0=middle-turn*.5f+insetInner+t0*spanInner,ai1=middle-turn*.5f+insetInner+t1*spanInner;
                            pi0=p.p(hub.x+std::cos(ai0)*inner,hub.y+std::sin(ai0)*inner);
                            pi1=p.p(hub.x+std::cos(ai1)*inner,hub.y+std::sin(ai1)*inner);
                        }
                        p.d->AddImageQuad(wTex.ringBg.id,po0,po1,pi1,pi0,{t0,0},{t1,0},{t1,1},{t0,1},tint);
                    }
                };
                const float wheelDt=(!animate||!wheelHighlightSet_)?0.f:std::clamp(float(now-wheelHighlightAt_),0.f,.25f);
                const float kGlide=1.f-std::exp(-22.f*wheelDt),kLift=1.f-std::exp(-20.f*wheelDt);
                const auto wedgeLiftOf=[&](std::size_t i) {
                    const float targetLift=i==index?layout.lift:0.f;
                    if(i>=wedgeLift_.size())return targetLift;
                    if(!animate||!wedgeLiftSet_)wedgeLift_[i]=targetLift;
                    else {
                        wedgeLift_[i]+=(targetLift-wedgeLift_[i])*kLift;
                        if(std::abs(targetLift-wedgeLift_[i])<.05f)wedgeLift_[i]=targetLift;
                    }
                    return wedgeLift_[i];
                };
                std::vector<std::vector<ImVec2>> shapes;
                std::vector<float> lifts;
                for(std::size_t i=0;i<n&&i<layout.ring.size();++i) {
                    const auto& row=state.rows[i];const float lift=wedgeLiftOf(i),outer=layout.outer+lift;
                    lifts.push_back(lift);
                    shapes.push_back(segment(middleOf(float(i)),layout.inner,outer));
                    if(wTex.ringBg)drawWedgeRing(middleOf(float(i)),layout.inner,outer);
                    else fill(shapes.back(),outer,p.color(dark,o.backgroundOpacity*o.wheelOpacity*frameAlpha));
                    if(row.equipped&&o.equippedHighlight!=0&&o.equippedHighlight!=1)fill(shapes.back(),outer,p.color(equippedMark,.24f));
                }
                wedgeLiftSet_=true;
                // The highlight glides to the pointed segment while motion is on.
                const float target=middleOf(float(index));
                if(!animate||!wheelHighlightSet_)wheelHighlight_=target;
                else {
                    wheelHighlight_+=std::remainder(target-wheelHighlight_,2*pi)*kGlide;
                    if(std::abs(std::remainder(target-wheelHighlight_,2*pi))<.002f)wheelHighlight_=target;
                }
                wheelHighlightSet_=true;wheelHighlightAt_=now;
                if(index<shapes.size()&&(filled||rim)) {
                    const float outer=layout.outer+(index<lifts.size()?lifts[index]:layout.lift);
                    fill(segment(wheelHighlight_,layout.inner,outer),outer,p.color(selectionMark,filled?.3f:.12f));
                    if(rim) {
                        // A rim highlight is a band of the accent color along the segment's outer edge.
                        const float width=std::max(3.f,thickness*.08f),edge=outer-width*.5f;
                        const float inset=n>1&&edge>0?std::asin(std::min(1.f,layout.wedgeGap*.5f/edge)):0.f;
                        p.d->PathArcTo(center,edge*p.scale,wheelHighlight_-turn*.5f+inset,wheelHighlight_+turn*.5f-inset,std::max(6,int(turn*edge*p.scale/4)));
                        p.d->PathStroke(p.color(p.accent()),n>1?0:ImDrawFlags_Closed,width*p.scale);
                    }
                }
                for(std::size_t i=0;i<shapes.size();++i) {
                    const auto& row=state.rows[i];const bool pointed=i==index;const float outer=layout.outer+lifts[i];
                    if(!wTex.outerRim)stroke(shapes[i],outer,p.color(theme.border,frameAlpha),theme.style.borderWidth);
                    if(row.equipped&&o.equippedHighlight!=0&&o.equippedHighlight!=2)stroke(shapes[i],outer,p.color(equippedMark),1.6f);
                    if(pointed&&outline)stroke(shapes[i],outer,p.color(selectionMark),1.8f);
                }
                if(wTex.divider&&n>1) {
                    const float spokeLen=std::max(4.f,layout.outer-layout.inner),spokeW=std::max(2.f,spokeLen*(wTex.divider.width/wTex.divider.height));
                    const float midR=(layout.inner+layout.outer)*.5f;
                    for(std::size_t i=0;i<n;++i) {
                        const float angle=middleOf(float(i))-turn*.5f;
                        p.drawRotatedImage(hub.x+std::cos(angle)*midR,hub.y+std::sin(angle)*midR,spokeW,spokeLen,angle+pi*.5f,wTex.divider,wMeta.divider,frameAlpha);
                    }
                }
                if(wTex.innerRim&&layout.inner>2.f)p.drawThemeImage(hub.x-layout.inner,hub.y-layout.inner,layout.inner*2,layout.inner*2,wTex.innerRim,wMeta.innerRim,frameAlpha);
                if(wTex.outerRim)p.drawThemeImage(hub.x-layout.outer,hub.y-layout.outer,layout.outer*2,layout.outer*2,wTex.outerRim,wMeta.outerRim,frameAlpha);
                for(std::size_t i=0;i<shapes.size();++i) {
                    const auto& row=state.rows[i];const bool pointed=i==index;
                    const float lift=lifts[i],middle=middleOf(float(i)),outer=layout.outer+lift,reach=layout.reach+lift*.5f;
                    // Icons fill the ring: as large as its thickness and the segment's width allow.
                    const float chord=n>1?2*reach*std::sin(std::min(turn,pi)*.5f)-layout.wedgeGap:thickness;
                    const float liftRatio=layout.lift>0?std::clamp(lift/layout.lift,0.f,1.f):(pointed?1.f:0.f);
                    const float size=std::max(8.f,std::min(thickness*.62f,chord*.72f))*(1.f+.12f*liftRatio);
                    const bool labelled=o.wheelLabels&&thickness>=size+16*text;
                    const Point slot{hub.x+std::cos(middle)*reach,hub.y+std::sin(middle)*reach-(labelled?7*text:0.f)};
                    auto item=p;if(!pointed)item.alpha*=.88f;
                    item.slotGlyph(i,slot.x,slot.y,size*1.18f,pointed?.62f:.38f);
                    const float r=size*.5f*item.tweaks.iconScale;
                    item.icon(slot.x,slot.y,size,rowIconEntry(row),row.available?item.itemColor(rowIconEntry(row)):muted,row.weapon.charge);
                    item.sweep(slot.x,slot.y,r,cooldowns_.fraction(row.entry.form,row.cooldown));
                    item.soulGem(slot.x,slot.y,r,row.weapon.charge);
                    if(labelled)item.label(slot.x,slot.y+size*.5f+1,displayName(row.entry),11,row.available?ivory:muted,false,std::max(20.f,chord*.88f),true);
                    const Point out{std::cos(middle),std::sin(middle)};
                    if(row.equipped)item.equippedMarker(hub.x+out.x*(outer-7),hub.y+out.y*(outer-7),2.8f*item.mark());
                    if(!row.available&&!isStack(row.entry)){item.circle(slot.x+size*.36f,slot.y+size*.36f,5,IM_COL32(22,18,18,240),true);item.line(slot.x+size*.36f-3,slot.y+size*.36f+3,slot.x+size*.36f+3,slot.y+size*.36f-3,blood,1.5f);}
                    item.badge(slot.x+size*.32f,slot.y+size*.14f,row.entry,row.quantity,row.cooldown,pointed?13:11,false,row.weapon);
                }
                // The hub: an optional circle, the pointed item's name when it fits, and a dot or pointer that follows the stick or mouse.
                const float radius=layout.hubRadius;
                if(o.wheelHubCircle&&radius>0) {
                    if(wTex.hubBg||wTex.hubBorder) {
                        if(wTex.hubBg)p.drawThemeImage(hub.x-radius,hub.y-radius,radius*2,radius*2,wTex.hubBg,wMeta.hubBg,o.backgroundOpacity*o.wheelOpacity*frameAlpha);
                        if(wTex.hubBorder)p.drawThemeImage(hub.x-radius,hub.y-radius,radius*2,radius*2,wTex.hubBorder,wMeta.hubBorder,frameAlpha);
                    } else {
                        p.d->AddCircleFilled(center,radius*p.scale,p.color(dark,o.backgroundOpacity*o.wheelOpacity*frameAlpha),64);
                        p.d->AddCircle(center,radius*p.scale,p.color(theme.border,frameAlpha),64,std::max(1.f,theme.style.borderWidth*p.scale));
                    }
                    if(wTex.hubOverlay)p.drawThemeImage(hub.x-radius,hub.y-radius,radius*2,radius*2,wTex.hubOverlay,wMeta.hubOverlay,frameAlpha);
                }
                if(layout.nameInHub) {
                    const float nameSize=17,statusSize=12.5f,room=2*std::sqrt(std::max(1.f,radius*radius-27*text*27*text))-12;
                    const auto measure=[&](const std::string& v){return p.medium->CalcTextSizeA(nameSize*text*p.scale,FLT_MAX,0,v.c_str()).x/p.scale;};
                    std::vector<std::string> lines{displayName(active->entry)};
                    if(measure(lines[0])>room) {
                        // Break at the space that leaves the two lines closest in width.
                        const auto whole=lines[0];float best=FLT_MAX;
                        for(std::size_t at=whole.find(' ');at!=std::string::npos;at=whole.find(' ',at+1)) {
                            const auto first=whole.substr(0,at),second=whole.substr(at+1);const float widest=std::max(measure(first),measure(second));
                            if(!first.empty()&&!second.empty()&&widest<best){best=widest;lines={first,second};}
                        }
                    }
                    const float block=float(lines.size())*(nameSize+2)*text+3+statusSize*text+(p.showsCharge(*active)?Paint::chargeRow(statusSize,text):0.f);float y=hub.y-block*.5f;
                    for(const auto& line:lines){p.label(hub.x,y,line,nameSize,active->available?ivory:muted,true,room,true);y+=(nameSize+2)*text;}
                    p.statusLine(hub.x,y+3,*active,statusSize,active->available?silver:depletedMark,room);
                }
                if(const float length=std::hypot(state.pointerX,state.pointerY);o.wheelPointer&&radius>0&&n>1&&length>.15f) {
                    const float dx=state.pointerX/length,dy=state.pointerY/length;
                    if(wTex.pointer) {
                        const float h=std::clamp(radius*.36f,10.f,28.f),w=h*(wTex.pointer.width/wTex.pointer.height);
                        p.drawRotatedImage(hub.x+dx*radius,hub.y+dy*radius,w,h,std::atan2(dy,dx)+pi*.5f,wTex.pointer,wMeta.pointer);
                    } else p.circle(hub.x+dx*radius,hub.y+dy*radius,3,selectionMark,true);
                }
            } else if(o.selector==wheelSelector) {
                for(std::size_t i=0;i<n&&i<layout.ring.size();++i) {
                    const auto slot=layout.ring[i];
                    node(state.rows[i],slot.x,slot.y,i==index?29.f:21.f,i==index,selectorTileShape(o),i==index?1.f:.8f);
                }
                if(n>1&&index<layout.ring.size()) {
                    // The hub shows where the player points and which slot that selects.
                    const auto hub=layout.hub,slot=layout.ring[index];const float length=std::hypot(slot.x-hub.x,slot.y-hub.y);
                    const Point toward{(slot.x-hub.x)/length,(slot.y-hub.y)/length};const float inner=std::max(14.f,layout.reach-44);
                    if(o.wheelHubCircle)p.circle(hub.x,hub.y,10,muted,false,.8f);
                    p.line(hub.x+toward.x*12,hub.y+toward.y*12,hub.x+toward.x*inner,hub.y+toward.y*inner,p.accent(),1.4f);
                    p.diamond(hub.x+toward.x*inner,hub.y+toward.y*inner,2.6f,selectionMark);
                    if(o.wheelPointer)p.circle(hub.x+state.pointerX*9,hub.y+state.pointerY*9,2.4f,ivory,true);
                }
            } else if(o.selector==2) {
                const auto visible=std::min(n,std::size_t(8));
                for(std::size_t i=0;i<visible;++i) {
                    const auto offset=i<=visible/2?std::ptrdiff_t(i):std::ptrdiff_t(i)-std::ptrdiff_t(visible);
                    const auto at=std::size_t((std::ptrdiff_t(index)+offset+std::ptrdiff_t(n))%std::ptrdiff_t(n));
                    const auto p=layout.ring[i];
                    node(state.rows[at],p.x,p.y,i==0?29.f:22.f,i==0,selectorTileShape(o),i==0?1.f:.72f);
                }
            } else if(o.selector==4) {
                // Compact SkyUI/QuickLoot-style scrollable panel: single frame, header with group/position, inline row metadata.
                const float panelX=6.f,panelW=layout.width-12.f,panelH=layout.ruleY-4.f;
                p.panelFrame(panelX,0,panelW,panelH);
                if(layout.listHeaderHeight>0) {
                    p.label(panelX+16,6,o.names[group],13,p.groupAccent?p.accent():ImU32(silver),true,panelW-84);
                    if(n>1)p.label(panelX+panelW-16,6.5f,std::to_string(index+1)+" / "+std::to_string(n),12,silver,false,60,false,true);
                    p.line(panelX+8,layout.listHeaderHeight,panelX+panelW-8,layout.listHeaderHeight,(theme.border&0x00ffffffu)|0x78000000u,1.f);
                }
                const std::size_t visible=std::max<std::size_t>(1,std::min<std::size_t>(layout.listVisible,n));
                const std::size_t topRow=n<=visible?0:std::min(n-visible,index>visible/2?index-visible/2:0);
                const bool scrollable=n>visible;
                const float rowLeft=panelX+4.f,rowRight=panelX+panelW-(scrollable?12.f:4.f),rowW=rowRight-rowLeft;
                for(std::size_t k=0;k<visible;++k) {
                    const std::size_t at=topRow+k;const auto& row=state.rows[at];
                    const bool activeRow=at==index;
                    const float y0=layout.centerY+float(k)*layout.listHeight,cy=y0+layout.listHeight*.5f;
                    if(k>0)p.line(rowLeft+8,y0,rowRight-8,y0,(theme.border&0x00ffffffu)|0x3c000000u,.7f);
                    if(activeRow) {
                        p.d->AddRectFilled(p.p(rowLeft,y0+1),p.p(rowRight,y0+layout.listHeight-1),p.color(selectionMark,.20f),2*p.scale);
                        p.d->AddRectFilled(p.p(rowLeft,y0+2),p.p(rowLeft+3,y0+layout.listHeight-2),p.color(p.accent()),p.scale);
                    }
                    p.icon(rowLeft+19,cy,24,rowIconEntry(row),row.available?p.itemColor(rowIconEntry(row)):muted,row.weapon.charge);
                    p.sweep(rowLeft+19,cy,12*p.tweaks.iconScale,cooldowns_.fraction(row.entry.form,row.cooldown));
                    p.soulGem(rowLeft+19,cy,12*p.tweaks.iconScale,row.weapon.charge);
                    // Inline right-aligned metadata: hand / equipped / unavailable / quantity or cooldown.
                    float metaX=rowRight-8.f;
                    const auto hands=entryHands(row.entry,row.equipped);
                    if(hands) {
                        const auto hm=handMarker(hands);
                        p.text(metaX,cy-6*text,hm,11*text,equippedMark,false,0,false,true);
                        metaX-=p.sans->CalcTextSizeA(11*text*p.scale,FLT_MAX,0,hm.c_str()).x/p.scale+8.f;
                    } else if(row.equipped) {
                        p.equippedMarker(metaX-4,cy,2.6f*p.mark());metaX-=14.f;
                    } else if(!row.available&&!isStack(row.entry)) {
                        p.circle(metaX-5,cy,4.5f,IM_COL32(22,18,18,240),true);
                        p.line(metaX-8,cy+3,metaX-2,cy-3,blood,1.4f);metaX-=14.f;
                    }
                    const int count=badgeQuantity(row.entry,row.quantity,row.weapon,o);
                    const bool poisoned=o.showPoison&&row.weapon.poisoned;
                    if(count>=0||row.cooldown>0||poisoned) {
                        p.badge(metaX,cy-6.5f*text,row.entry,row.quantity,row.cooldown,11.5f,true,row.weapon);
                        metaX-=34.f;
                    }
                    const float nameX=rowLeft+38.f,nameW=std::max(40.f,metaX-nameX-4.f);
                    p.label(nameX,cy-7.5f*text,displayName(row.entry),14.5f,row.available?(activeRow?ivory:silver):muted,activeRow,nameW);
                }
                if(scrollable) {
                    const float trackTop=layout.centerY+2.f,trackH=std::max(12.f,float(visible)*layout.listHeight-4.f);
                    const float thumbH=std::max(14.f,trackH*float(visible)/float(n));
                    const float thumbY=trackTop+(trackH-thumbH)*float(topRow)/float(std::max<std::size_t>(1,n-visible));
                    p.d->AddRectFilled(p.p(panelX+panelW-8,trackTop),p.p(panelX+panelW-5,trackTop+trackH),p.color(theme.border,.22f),p.scale);
                    p.d->AddRectFilled(p.p(panelX+panelW-8,thumbY),p.p(panelX+panelW-5,thumbY+thumbH),p.color(p.accent(),.85f),p.scale);
                }
                if(p.showsCharge(*active)) {
                    const float pip=14.f*text*.54f;
                    p.chargePips(center-Paint::pipsWidth(pip)*.5f,layout.ruleY+Paint::chargeRow(14.f,text)*.5f,active->weapon.charge,pip);
                }
            } else {
                p.d->PushClipRect(p.p(0,-32),p.p(layout.width,layout.ruleY-2),true);
                const int reach=int(o.selectorVisible)+1;const float falloff=reach<=2?.30f:.55f/float(reach-1);
                for(int offset=-reach;offset<=reach;++offset) {
                    if(offset<0&&std::size_t(-offset)>(n-1)/2)continue;
                    if(offset>0&&std::size_t(offset)>n/2)continue;
                    const auto at=std::size_t((int(index)+offset+int(n))%int(n));
                    const auto& row=state.rows[at];
                    const float position=float(offset)+displacement,distance=std::abs(position);
                    const float opacity=distance<=1?1-.22f*distance:std::max(0.f,.78f-falloff*(distance-1));
                    const auto positionOffset=selectorOffset(o,position);
                    node(row,center+positionOffset.x,layout.centerY+positionOffset.y,selectorRadius(distance),offset==0,selectorTileShape(o),opacity);
                }
                if(n>1&&o.selector<2){p.line(16,layout.centerY-3,11,layout.centerY+2,muted);p.line(11,layout.centerY+2,16,layout.centerY+7,muted);p.line(geometry.selectorWidth-16,layout.centerY-3,geometry.selectorWidth-11,layout.centerY+2,muted);p.line(geometry.selectorWidth-11,layout.centerY+2,geometry.selectorWidth-16,layout.centerY+7,muted);}
                if(n>1&&o.selector==3){p.line(center-70,layout.centerY-37,center-65,layout.centerY-42,muted);p.line(center-65,layout.centerY-42,center-60,layout.centerY-37,muted);p.line(center-70,layout.centerY+37,center-65,layout.centerY+42,muted);p.line(center-65,layout.centerY+42,center-60,layout.centerY+37,muted);}
                p.d->PopClipRect();
            }
            if(o.selector!=4&&(!layout.segments||layout.nameBelow)) {
                p.rule(center,layout.ruleY,o.selector==2||o.selector==wheelSelector?42.f:58.f);
                const bool round=o.selector==2||o.selector==wheelSelector;
                const float nameWidth=round?238*std::max(1.f,text):layout.width-24;
                p.label(center,layout.nameY,displayName(active->entry),round?20.f:23.f,active->available?ivory:muted,true,nameWidth,true);
                p.statusLine(center,layout.statusY,*active,14,active->available?silver:depletedMark,layout.width-24);
            }
            if(o.showDescriptions&&!active->description.empty())p.paragraph(center,layout.descriptionY,active->description,13,silver,layout.width-32,2);
            // The last line names the group the selector belongs to and the entry's place in it (compact list shows it in the header).
            if(o.selector!=4&&o.selectorFooter)p.label(center,layout.pagerY,o.names[group]+(n>1?"   ·   "+std::to_string(index+1)+" / "+std::to_string(n):std::string{}),13,p.groupAccent?p.accent():ImU32(silver),false,layout.width-24,true);
            if(o.showHints) {
                std::string cancel;
                if(state.controller) {
                    if(state.vr)cancel=tr("Main B/Y  cancel");
                    else if(o.controllerActions[0].button!=PadButton::none)cancel=padLabel(o.controllerActions[0],o.controllerStyle)+"  "+tr("cancel");
                }else {
                    const auto binding=o.selectorBindings[unsigned(KeyboardAction::cancel)];
                    if(binding.key) {
                        if(binding.modifiers&keyModifier::ctrl)cancel+="Ctrl + ";
                        if(binding.modifiers&keyModifier::shift)cancel+="Shift + ";
                        if(binding.modifiers&keyModifier::alt)cancel+="Alt + ";
                        cancel+=keyLabel(binding.key);
                    }
                    if(cancel.empty()&&o.mouseActions[0]>=0)cancel=tr(mouseButtonName(o.mouseActions[0]));
                    if(!cancel.empty())cancel+="  "+std::string(tr("cancel"));
                }
                const std::string browse=o.selector==wheelSelector?(state.controller?tr("Stick  point"):tr("Mouse  point")):state.controller?(state.vr?tr("Stick / pad  choose"):tr("Stick / buttons  choose")):tr("Scroll  choose");
                p.label(center,layout.hintY,browse+(cancel.empty()?std::string{}:"   ·   "+cancel),13,silver,false,layout.width-24,true);
            }
        } else {
            const float y=std::min(layout.centerY,75.f);
            p.diamond(center,y,26,IM_COL32(155,151,141,130),false);p.text(center,y-18,"+",26,silver,false,0,true);
            p.label(center,y+46,tr("No items"),22,ivory,true,layout.width-24,true);
            // An empty group says where its items come from.
            p.label(center,y+46+30*p.textFactor,state.editorShortcut.empty()?std::string(tr("Add items in the Quickslot editor")):trNamed("Press %s to add items",state.editorShortcut),13,silver,false,layout.width-24,true);
        }
    }
    if(selectorVisible)p.d->PopClipRect();
    if(bounds)collect(selectorStart,bounds->selector);
    const int widgetStart=p.d->VtxBuffer.Size;
    Paint groups=p;groups.origin={origin.x+geometry.origin.x+geometry.groupOrigin.x*scale,origin.y+geometry.origin.y+geometry.groupOrigin.y*scale};
    groups.scale=scale;groups.alpha=o.opacity*visibility;groups.textFactor=hudTextFactor(o);
    const bool showGroups=visibility>0&&showGroupDisplay(state,o,now);
    const bool unified=unifiedHotbar(o);
    if(showGroups&&unified&&!geometry.groups.tiles.empty()) {
        groups.panelFrame(-4,-3,geometry.groups.width+8,geometry.groups.height+6);
    }
    if(showGroups)for(std::size_t i=0;i<geometry.groups.tiles.size();++i) {
        const int tileStart=p.d->VtxBuffer.Size;
        const auto id=o.groupOrder[i];
        const float groupAlpha=groupVisibility(state,o,id,now);
        if(groupAlpha<=0)continue;
        const auto& tile=state.groups[id];const bool selected=id==group;
        const auto* entry=selected&&active?&active->entry:tile.entry?&*tile.entry:nullptr;
        // The pulse follows the group's own remembered item, whatever the selector is previewing.
        const auto* identity=tile.iconEntry?&*tile.iconEntry:tile.entry?&*tile.entry:nullptr;
        const float pulse=animate?pulses_[id].update(identity!=nullptr,tile.equipped,identity?identity->form:0,identity?identity->unique:std::uint16_t(0),tile.quantity,identity&&isStack(*identity),now,state.nudges[id]):-1;
        const auto* artwork=selected&&active?&rowIconEntry(*active):tile.iconEntry?&*tile.iconEntry:entry;
        const bool equipped=selected&&active?active->equipped:tile.equipped;
        const bool available=selected&&active?active->available:tile.available;
        const int quantity=selected&&active?active->quantity:tile.quantity;
        const float cooldown=selected&&active?active->cooldown:tile.cooldown;
        const auto weapon=selected&&active?active->weapon:tile.weapon;
        const auto pos=geometry.groups.tiles[i];float x=pos.x,y=pos.y;
        auto q=groups;q.alpha*=groupAlpha;q.groupAccent=groupColor(id);
        const auto ownTileSize=tileSizeFor(geometry.groups,i);
        // Hairline slot separators inside a unified hotbar.
        if(unified) {
            const ImU32 sep=(theme.border&0x00ffffffu)|0x50000000u;
            const auto cols=std::max(1u,unsigned(std::clamp(o.gridColumns,1.f,8.f)));
            if((o.layout==0&&i>0)||(o.layout==6&&(i%cols)!=0))groups.line(x,y+6,x,y+ownTileSize.y-6,sep,.8f);
            if((o.layout==1&&i>0)||(o.layout==6&&i>=cols))groups.line(x+8,y,x+ownTileSize.x-8,y,sep,.8f);
        }
        // A Free tile with its own size draws from its corner at that size.
        if(const float own=tileScale(geometry.groups,i);own!=1){q.origin=q.p(x,y);q.scale*=own;x=y=0;}
        // Optional subtle center-anchored scale pop on the active group tile.
        if(selected&&o.activeGroupBoost>0&&!unified) {
            const float boost=1.f+o.activeGroupBoost;
            const auto mid=q.p(x+ownTileSize.x*.5f,y+ownTileSize.y*.5f);
            q.scale*=boost;
            q.origin={mid.x-ownTileSize.x*.5f*q.scale,mid.y-ownTileSize.y*.5f*q.scale};
            x=y=0;
        }
        const float text=groups.textFactor;   // card rows follow the widget's text size
        const ImU32 nameColor=q.groupAccent?*q.groupAccent:selected?ImU32(ivory):ImU32(silver);
        const bool previewing=selected&&state.browsing;
        const auto hands=entry?entryHands(*entry,equipped):0;

        // An equipped highlight takes the place of the accent line a frame draws for its equipped item.
        const bool emphasized=equipped&&!o.equippedHighlight;
        const bool ready=entry&&isConsumable(*entry)&&tile.consumable&&(!previewing||state.preview==state.selected);
        const bool badged=(o.showAmmo&&weapon.ammunition>=0)||(o.showPoison&&weapon.poisoned);
        const unsigned tileShape=effectiveTileShape(o,id);
        const bool tileShowLabel=effectiveTileShowLabel(o,id);
        const bool tileShowKey=effectiveTileShowKey(o,id);
        const auto swap=swaps_[id].update(artwork,now,animate&&!state.browsing);
        const auto drawTileIcon=[&](Paint& dst,float ix,float iy,float isize) {
            if(swap.hasPrevious&&swap.progress<1) {
                const float ease=easeOut(swap.progress);
                Paint outPaint=dst;outPaint.alpha*=1.f-ease;
                outPaint.icon(ix-9.f*ease,iy,isize,swap.previous,outPaint.itemColor(swap.previous));
                Paint inPaint=dst;inPaint.alpha*=ease;
                inPaint.icon(ix+9.f*(1.f-ease),iy,isize,*artwork,available?inPaint.itemColor(*artwork):muted,weapon.charge);
            } else dst.icon(ix,iy,isize,*artwork,available?dst.itemColor(*artwork):muted,weapon.charge);
            dst.sweep(ix,iy,isize*.5f*dst.tweaks.iconScale,cooldowns_.fraction(entry->form,cooldown));
            dst.soulGem(ix,iy,isize*.5f*dst.tweaks.iconScale,weapon.charge);
        };
        const float keyWidth=q.groupKeyWidth(id,state.controller,state.vr,18,tileShowKey);
        if(tileShape==0) {
            // A card reads icon, text, key from left to right; a mirrored card reads the other way.
            const bool wide=o.showCardNames||tileShowLabel,mirror=o.mirrorCards&&wide;const float side=mirror?-1.f:1.f;
            const float width=ownTileSize.x,keyColumn=tileShowKey?std::max(cardKeyColumn,keyWidth+8):0;
            const float iconX=x+(!wide?(width-keyColumn)*.5f:mirror?width-25.f:25.f);
            const float h=ownTileSize.y-4,cy=y+2+h*.5f,textX=mirror?x+width-51:x+51,textWidth=width-59-keyColumn;
            if(!unified)q.frame(x,y+2,width,h,emphasized,1,true,equipped);
            else if(previewing||equipped)q.d->AddRectFilled(q.p(x+1,y+2),q.p(x+width-1,y+2+h),q.color(previewing?q.accent():equippedMark,.15f),2*q.scale);
            if(previewing||equipped) {
                const float bx=mirror?x+width-4:x+1;
                q.d->AddRectFilled(q.p(bx,y+4),q.p(bx+3,y+h),q.color(previewing?q.accent():equippedMark,q.tweaks.frameOpacity),q.scale);
            }
            if(equipped&&!unified)q.equippedHighlight(x,y+2,width,h,0,true);
            if(previewing) {
                if(unified)q.highlight(x,y+2,width,h,0,q.accent(),true,false,0.f);
                else q.previewHighlight(x,y+2,width,h,0,true,2.f);
            }
            q.pulse(x,y+2,width,h,0,pulse);
            q.slotGlyph(id,wide?(mirror?x+20.f:x+width-keyColumn-20.f):iconX,cy,h*.76f,(previewing||equipped)?.65f:.44f);
            if(entry)drawTileIcon(q,iconX,cy,34);
            else q.text(iconX,cy-15,"+",24,muted,false,0,true);
            const float block=(tileShowLabel&&o.showCardNames?26*text+4:14*text),top=cy-block*.5f;
            if(tileShowLabel)q.label(textX,top,o.names[id],o.showCardNames?12.f:14.f,q.groupAccent?nameColor:ImU32(silver),true,textWidth,false,mirror);
            if(o.showCardNames)q.label(textX,top+(tileShowLabel?12*text+4:0),entry?displayName(*entry):tr("Empty"),14,entry&&!available?muted:ivory,true,textWidth,false,mirror);
            if(keyWidth>0) {
                float kx=mirror?x+4+keyColumn*.5f:x+width-4-keyColumn*.5f,ky=cy-9;
                if(o.keyPosition==1){kx=x+width*.5f;ky=y-13;}
                else if(o.keyPosition==2){kx=x+width*.5f;ky=y+2+h+1;}
                else if(o.keyPosition==3){kx=x-keyWidth*.5f-4;ky=cy-9;}
                else if(o.keyPosition==4){kx=x+width+keyWidth*.5f+4;ky=cy-9;}
                else if(o.keyPosition==5){kx=x+keyWidth*.5f+3;ky=y-4;}
                q.groupKey(kx,ky,id,previewing,state.controller,state.vr,18,tileShowKey);
            }
            // Hand letters keep to the icon's outer corner, above the icon when a count or poison mark sits below it.
            if(hands)q.text(iconX-18*side,badged?cy-h*.5f+3:cy+h*.5f-8-10*q.mark(),handMarker(hands),10*q.mark(),equippedMark,false,0,mirror);
            else if(equipped)q.equippedMarker(iconX+18*side,cy+14,2.5f*q.mark());
            else if(ready)q.ready(iconX+18*side,cy+14,2.5f*q.mark(),equippedMark);
            if(entry) {
                float bx=iconX+2,by=cy+8;bool bRight=false;
                if(o.badgePosition==2){bx=iconX-14;by=cy+8;}
                else if(o.badgePosition==3){bx=iconX+14;by=cy-18;bRight=true;}
                q.badge(bx,by,*entry,quantity,cooldown,11,bRight,weapon);
            }
        } else {
            // The tile is drawn at its own size inside the widget; external names keep the widget's text size.
            const float size=o.tileSize,tileWidth=ownTileSize.x/size;
            const auto corner=q.p(x,y);q.origin=corner;q.scale*=size;q.textFactor/=size;
            const float x=0,y=0;
            const bool round=tileShape==1,slot=tileShape==2,hex=tileShape==4,bare=tileShape==5;
            const bool labelAbove=tileShowLabel&&o.labelPosition==1,labelInside=tileShowLabel&&o.labelPosition==2;
            const float topPad=labelAbove?(12.f*q.textFactor+4.f):0.f;
            const float cx=x+tileWidth*.5f;
            const float halfR=round?29.f:slot?39.f:hex?41.f:bare?30.f:42.f;
            const float cy=y+topPad+(round?34.f:slot?39.f:hex?42.f:bare?32.f:44.f);
            const bool badgeLeft=o.badgePosition==2;
            const bool hasBadge=entry&&(isStack(*entry)||cooldown>0||badged);
            const float markerSide=badgeLeft?1.f:(hasBadge?-1.f:1.f);
            const float markerX=cx+markerSide*(round||bare?21.f:slot?26.f:hex?24.f:0.f);
            const float markerY=cy+(round||bare?21.f:slot?24.f:hex?25.f:31.f);
            const unsigned hlShape=round?1u:slot?0u:hex?4u:bare?5u:3u;
            const float hlW=halfR*2,hlH=halfR*2;
            if(unified) {
                if(previewing||equipped)q.d->AddRectFilled(q.p(cx-halfR+2,cy-halfR+2),q.p(cx+halfR-2,cy+halfR-2),q.color(previewing?q.accent():equippedMark,.16f),2*q.scale);
                if(previewing)q.highlight(cx-halfR+1,cy-halfR+1,hlW-2,hlH-2,0,q.accent(),true,false,0);
                else if(equipped&&o.equippedHighlight)q.equippedHighlight(cx-halfR+1,cy-halfR+1,hlW-2,hlH-2,0);
                q.pulse(cx-halfR,cy-halfR,hlW,hlH,0,pulse);
            } else {
                if(round)q.medallion(cx,cy,halfR,emphasized,equipped);
                else if(slot)q.frame(cx-halfR,cy-halfR,hlW,hlH,emphasized,o.layout==5?(i==0?0u:i==2?1u:2u):1u,false,equipped);
                else if(hex)q.hexagon(cx,cy,halfR,emphasized,equipped);
                else if(bare)q.frameless(cx,cy,halfR,emphasized,equipped);
                else q.lozenge(cx,cy,halfR,emphasized,equipped);
                if(equipped)q.equippedHighlight(cx-halfR,cy-halfR,hlW,hlH,hlShape,false);
                if(previewing)q.previewHighlight(cx-halfR,cy-halfR,hlW,hlH,hlShape,false,2.f);
                q.pulse(cx-halfR,cy-halfR,hlW,hlH,hlShape,pulse);
            }
            q.slotGlyph(id,cx,cy,halfR*1.32f,(previewing||equipped)?.62f:.42f);
            const float keyH=round||slot||bare?17.f:14.f;
            const float ownKeyW=q.groupKeyWidth(id,state.controller,state.vr,keyH,tileShowKey);
            if(entry) {
                const float iconSize=slot?48.f:round?40.f:46.f;
                drawTileIcon(q,cx,cy,iconSize);
            }
            else {q.line(cx-5,cy,cx+5,cy,muted);q.line(cx,cy-5,cx,cy+5,muted);}
            if(ownKeyW>0) {
                // Automatic keys tuck into the upper-left corner on square slots and straddle the top edge/vertex on radial shapes so the item icon stays dead-center.
                float kx=slot?cx-halfR*.72f:cx,ky=slot?cy-halfR*.88f:cy-halfR-(round||bare?6.f:4.f);
                if(o.keyPosition==1){kx=cx;ky=y+topPad-12.f;}
                else if(o.keyPosition==2){kx=cx;ky=cy+halfR+2.f+(tileShowLabel&&o.labelPosition==0?13.f:0.f);}
                else if(o.keyPosition==3){kx=cx-halfR-ownKeyW*.5f-4.f;ky=cy-keyH*.5f;}
                else if(o.keyPosition==4){kx=cx+halfR+ownKeyW*.5f+4.f;ky=cy-keyH*.5f;}
                else if(o.keyPosition==5){kx=cx-halfR*.72f;ky=cy-halfR*.88f;}
                q.groupKey(kx,ky,id,previewing,state.controller,state.vr,keyH,tileShowKey);
            }
            if(tileShowLabel) {
                if(labelAbove)q.label(cx,y,o.names[id],12,nameColor,true,tileWidth-2,true);
                else if(labelInside)q.label(cx,cy+halfR-14.f,o.names[id],10.5f,nameColor,true,halfR*1.65f,true);
                else q.label(cx,cy+halfR+3.f,o.names[id],12,nameColor,true,tileWidth-2,true);
            }
            // Hand letters and counts tuck cleanly into lower corners.
            if(hands) {
                if(slot)q.text(badgeLeft?cx+19:cx-26,cy+17,handMarker(hands),10*q.mark(),equippedMark);
                else q.text(badgeLeft?(round||bare?cx+14:cx+10):(round||bare?cx-19:cx-14),(round||bare?cy+17:cy+20)-10*q.mark(),handMarker(hands),10*q.mark(),equippedMark);
            }
            else if(equipped)q.equippedMarker(markerX,markerY,3*q.mark());
            else if(ready)q.ready(markerX,markerY,3*q.mark(),equippedMark);
            else if(entry&&!available&&!isStack(*entry)){q.circle(markerX,markerY,5,IM_COL32(22,18,18,230),true);q.line(markerX-3,markerY+3,markerX+3,markerY-3,blood,1.5f);}
            if(entry) {
                if(o.badgePosition==2) {
                    if(slot)q.badge(cx-21,cy+17,*entry,quantity,cooldown,13,false,weapon);
                    else q.badge(round||bare?cx-14:cx-10,round||bare?cy+5:cy+8,*entry,quantity,cooldown,12,false,weapon);
                } else if(o.badgePosition==3) {
                    if(slot)q.badge(cx+25,cy-halfR+4,*entry,quantity,cooldown,13,true,weapon);
                    else q.badge(round||bare?cx+23:cx+18,cy-22,*entry,quantity,cooldown,12,true,weapon);
                } else {
                    if(slot)q.badge(cx+21,cy+17,*entry,quantity,cooldown,13,false,weapon);
                    else q.badge(round||bare?cx+23:cx+18,round||bare?cy+5:cy+8,*entry,quantity,cooldown,12,true,weapon);
                }
            }
        }
        if(bounds){Bounds box;collect(tileStart,box);bounds->tiles.push_back(box);}
    }
    // The name of a newly selected item appears beside its group's tile: under it, over it or to one side,
    // wherever it stays on screen clear of the other tiles. Cards that show item names already carry it.
    bool captionAbove=false;
    if(!state.browsing&&showGroups&&active&&now<state.revealUntil&&groupVisibility(state,o,group,now)>0&&!(o.shape==0&&o.showCardNames)) {
        auto caption=groups;const auto name=displayName(active->entry);const float text=groups.textFactor,line=15*text;
        const float limit=std::min(std::max(240.f,geometry.groups.width),(screen.x-16)/scale);
        const float width=std::min(limit,caption.medium->CalcTextSizeA(line*scale,FLT_MAX,0,name.c_str()).x/scale+2);
        const float tileWidth=geometry.groups.tileWidth,tileHeight=geometry.groups.tileHeight;
        std::size_t slot=geometry.groups.tiles.size();
        for(std::size_t i=0;i<geometry.groups.tiles.size();++i)if(o.groupOrder[i]==group)slot=i;
        // Widget units to screen pixels, measured from the viewport's corner.
        const auto screenX=[&](float x){return groups.origin.x-origin.x+x*scale;};const auto screenY=[&](float y){return groups.origin.y-origin.y+y*scale;};
        struct Spot {float x,y;unsigned align;bool above;};std::optional<Spot> chosen;
        if(slot<geometry.groups.tiles.size()) {
            const auto tile=geometry.groups.tiles[slot];const float middle=tile.x+tileWidth*.5f;
            // A centered caption slides sideways to stay on screen.
            const float centered=std::clamp(middle-width*.5f,(8-(groups.origin.x-origin.x))/scale,(screen.x-8-(groups.origin.x-origin.x))/scale-width);
            const std::array<Spot,4> spots{{{centered,tile.y+tileHeight+5,1,false},{centered,tile.y-line-6,1,true},
                {tile.x+tileWidth+8,tile.y+(tileHeight-line)*.5f,0,false},{tile.x-8-width,tile.y+(tileHeight-line)*.5f,2,false}}};
            for(const auto& spot:spots) {
                if(screenX(spot.x)<8||screenX(spot.x+width)>screen.x-8||screenY(spot.y)<8||screenY(spot.y+line)>screen.y-8)continue;
                bool clear=true;
                for(std::size_t i=0;i<geometry.groups.tiles.size()&&clear;++i) {
                    if(i==slot||groupVisibility(state,o,o.groupOrder[i],now)<=0)continue;
                    const auto other=geometry.groups.tiles[i];
                    clear=spot.x>=other.x+tileWidth||spot.x+width<=other.x||spot.y>=other.y+tileHeight||spot.y+line<=other.y;
                }
                if(clear){chosen=spot;break;}
            }
        }
        if(chosen) {
            captionAbove=chosen->above&&chosen->y<0;
            caption.label(chosen->x+(chosen->align==1?width*.5f:chosen->align==2?width:0.f),chosen->y,name,15,active->available?ivory:muted,true,width,chosen->align==1,chosen->align==2);
        } else {
            // With no clear spot beside the tile, the name is centered under the widget, or over it at the screen's foot.
            captionAbove=groups.origin.y-origin.y+(focus.max.y+geometry.captionHeight)*scale>screen.y-8;
            caption.origin.x=origin.x+std::clamp(groups.origin.x-origin.x+(focus.min.x+focus.width()*.5f)*scale,8+limit*scale*.5f,screen.x-8-limit*scale*.5f);
            const float y=captionAbove?focus.min.y-geometry.captionHeight:focus.max.y+7;
            caption.origin.y=std::clamp(groups.origin.y+y*scale,origin.y+8,origin.y+screen.y-8-geometry.captionHeight*scale);
            caption.label(0,0,name,15,active->available?ivory:muted,true,limit,true);
        }
    }
    if(bounds)collect(widgetStart,bounds->widget);
    if(equipmentAlpha>0&&showEquipmentDisplay(state,o,now)&&eqRows>0) {
        const int start=p.d->VtxBuffer.Size;
        auto panel=p;const auto& box=geometry.equipment;
        panel.origin={origin.x+box.origin.x,origin.y+box.origin.y};panel.scale=box.scale;panel.alpha=o.equipmentOpacity*equipmentAlpha;panel.textFactor=o.equipmentTextScale;
        const float width=box.width,height=box.height,t=o.equipmentTextScale;
        const bool mirroredList=o.mirrorEquipment&&o.equipmentStyle==0;
        if(o.equipmentStyle==0&&o.equipmentLayout==1) {
            if(panel.textures.equipmentPanel.any()) {
                if(panel.textures.equipmentPanel.bg)panel.drawThemeImage(0,0,width,height,panel.textures.equipmentPanel.bg,theme.assets.equipmentPanel.bg,o.backgroundOpacity*panel.tweaks.frameOpacity,mirroredList);
                if(panel.textures.equipmentPanel.border)panel.drawThemeImage(0,0,width,height,panel.textures.equipmentPanel.border,theme.assets.equipmentPanel.border,panel.tweaks.frameOpacity,mirroredList);
            } else {
                const auto shade=panel.color(dark,.72f*panel.tweaks.frameOpacity),fade=shade&0x00ffffff;
                if(mirroredList)panel.d->AddRectFilledMultiColor(panel.p(0,0),panel.p(width,height),fade,shade,shade,fade);
                else panel.d->AddRectFilledMultiColor(panel.p(0,0),panel.p(width,height),shade,fade,fade,shade);
                if(panel.textures.atlas)panel.d->AddImage(panel.textures.atlas,panel.p(0,0),panel.p(width,height),{.80f,.16f},{.96f,.78f},panel.color(IM_COL32_WHITE,.12f*panel.tweaks.frameOpacity));
                const float barX=mirroredList?width-1.f:1.f;
                panel.d->AddLine(panel.p(barX,3),panel.p(barX,height-3),panel.color(panel.accent(),panel.tweaks.frameOpacity),std::max(1.f,1.4f*panel.scale));
            }
        }
        unsigned slot=0;
        const auto drawRow=[&](const char* role,const std::optional<Row>& row) {
            const int cellStart=p.d->VtxBuffer.Size;
            const unsigned curSlot=slot++;
            const auto cell=box.cells[curSlot];
            auto cellPaint=panel;
            Point localOrigin=cell.origin;
            if(const float own=equipmentCellScale(box,curSlot);own!=1){cellPaint.origin=cellPaint.p(cell.origin.x,cell.origin.y);cellPaint.scale*=own;localOrigin={0,0};}
            const float x=localOrigin.x+cell.icon.x,y=localOrigin.y+cell.icon.y;
            if(o.equipmentStyle==0&&o.equipmentLayout==1&&slot>1) {
                const float lx0=mirroredList?8.f:45.f,lx1=mirroredList?width-45.f:width-8.f;
                panel.d->AddLine(panel.p(lx0,cell.origin.y-o.equipmentSpacing*.5f),panel.p(lx1,cell.origin.y-o.equipmentSpacing*.5f),panel.color(theme.border,.32f*panel.tweaks.frameOpacity));
            }
            if(o.equipmentStyle==0&&o.equipmentLayout!=1)cellPaint.frame(localOrigin.x,localOrigin.y,cell.width,cell.height,false,1,true,false);
            if(o.equipmentStyle>0&&o.equipmentStyle<4)cellPaint.tile(x,y,28,false,o.equipmentStyle==2?3u:o.equipmentStyle==3?2u:1u);
            auto item=cellPaint;if(row&&equipmentDimmed(*row))item.alpha*=.4f;
            if(box.iconSize>0) {
                if(row) {
                    item.icon(x,y,box.iconSize,rowIconEntry(*row),item.itemColor(rowIconEntry(*row)),row->weapon.charge);
                    item.sweep(x,y,box.iconSize*.5f*item.tweaks.iconScale,cooldowns_.fraction(row->entry.form,row->cooldown));item.soulGem(x,y,box.iconSize*.5f*item.tweaks.iconScale,row->weapon.charge);
                    const float size=11*std::clamp(t,.8f,1.25f),radius=o.equipmentStyle==0?box.iconSize*.5f:28.f;
                    cellPaint.badge(x+radius*(o.equipmentStyle==2?.7f:1.f),y+(o.equipmentStyle==0?radius-size:radius*.5f),row->entry,row->quantity,row->cooldown,size,true,row->weapon);
                }
                else cellPaint.text(x,y-8*t,"—",16*t,muted,false,0,true);
            }
            if(o.equipmentShowText||o.equipmentStyle==0||o.equipmentStyle==4) {
                const bool right=mirroredList||(o.equipmentStyle>0&&o.equipmentStyle<4&&o.equipmentTextPosition==2);
                const float tx=localOrigin.x+cell.text.x+(right?cell.textWidth:0),ty=localOrigin.y+cell.text.y;
                cellPaint.text(tx,ty,tr(role),11.5f*t,silver,false,cell.textWidth,cell.centered,right);
                item.text(tx,ty+16*t,equipmentTextName(row,box.iconSize>0,o),17*t,ivory,false,cell.textWidth,cell.centered,right);
            }
            if(bounds){Bounds cb;collect(cellStart,cb);bounds->equipmentCells.push_back(cb);}
        };
        forEachEquipmentRow(state.equipment,o,drawRow,eqIncludeEmpty);
        if(bounds)collect(start,bounds->equipment);
    }
    if(notification&&visibility>0) {
        const int noticeStart=p.d->VtxBuffer.Size;
        // A notice sits above the open selector, or above the widget while the selector is closed.
        auto toast=state.browsing?p:groups;if(noticed)toast.alpha*=float(std::clamp((state.noticeUntil-now)/.35,0.,1.));const float text=toast.textFactor;
        if(!state.browsing){
            const float width=std::min(std::max(240.f,focus.width()),(screen.x-16)/scale);
            toast.origin.x=origin.x+std::clamp(groups.origin.x-origin.x+(focus.min.x+focus.width()*.5f)*scale,8+width*scale*.5f,screen.x-8-width*scale*.5f)-center*scale;
            toast.origin.y=groups.origin.y+(focus.min.y-(captionAbove&&showGroups&&active?geometry.captionHeight+8:8.f))*scale;
            if(toast.origin.y-origin.y-27*text*scale<8)toast.origin.y=groups.origin.y+(focus.max.y+geometry.captionHeight+35*text)*scale;
            toast.origin.y=std::clamp(toast.origin.y,origin.y+8+27*text*scale,origin.y+screen.y-8);
        }
        toast.label(center,-27*text,noticed?state.notice:state.tip,13,ivory,false,state.browsing?410.f:std::max(240.f,focus.width()),true);
        toast.rule(center,-6,36);
        if(bounds)collect(noticeStart,state.browsing?bounds->selector:bounds->widget);
    }
}
}

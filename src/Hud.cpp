#include "Hud.h"
#include <fstream>
#include "Layout.h"
#include "IconPolicy.h"
#include "EquippedState.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace quickslot {
namespace {
// Palette roles. These zero-alpha tokens stand for the active theme's colors; Paint::color resolves them.
constexpr ImU32 ivory=1,silver=2,muted=3,blood=4,dark=5,selectionMark=6,equippedMark=7,poisonMark=8,depletedMark=9;
// Enchantment charge, in the violet of Skyrim's own charge meter.
constexpr ImU32 chargeViolet=IM_COL32(138,108,214,255);
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
    ImDrawList* d;ImVec2 origin;float scale,alpha;ImFont* medium;ImFont* sans;ImTextureID themeTexture,keyTexture;const KeyGlyphs& keyGlyphs;const Options& options;const Theme& theme;ThemeAdjustments tweaks;const ItemIconProvider& itemIcons;
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
        case poisonMark:c=theme.poison;break;case depletedMark:c=theme.depleted;break;default:break;
        }
        return (c&0xffffff)|(ImU32(float(c>>24)*std::clamp(alpha*opacity,0.f,1.f))<<24);
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
            d->AddImageQuad(themeTexture,place(xs[column],ys[row]),place(xs[column+1],ys[row]),place(xs[column+1],ys[row+1]),place(xs[column],ys[row+1]),
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
            d->AddImageQuad(themeTexture,point(column,row),point(column+1,row),point(column+1,row+1),point(last,row+1),uv(column,row),uv(column+1,row),uv(column+1,row+1),uv(last,row+1),c);
            d->AddImageQuad(themeTexture,point(row,column),point(row,column+1),point(row+1,column+1),point(row+1,last),uv(column,row),uv(column+1,row),uv(column+1,row+1),uv(last,row+1),c);
        }
    }
    void textureFrame(float x,float y,float w,float h,unsigned cell) {
        slicedFrame(w,h,cell,[&](float fx,float fy){return p(x+fx,y+fy);});
    }
    void frame(float x,float y,float w,float h,bool selected,unsigned variant=1) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        if(themeTexture) {
            textureFrame(x,y,w,h,variant);
            if(selected){line(x+4,y+h*.82f,x+w-4,y+h*.82f,accent(),1.2f);diamond(x+w*.5f,y+h*.82f,2.5f,selectionMark);}
            return;
        }
        const float cut=theme.frameStyle==1||theme.frameStyle==2?0:std::min(6.f,h*.16f);
        const ImVec2 points[]{p(x+cut,y),p(x+w-cut,y),p(x+w,y+cut),p(x+w,y+h-cut),
            p(x+w-cut,y+h),p(x+cut,y+h),p(x,y+h-cut),p(x,y+cut)};
        d->AddConvexPolyFilled(points,8,color(dark,options.backgroundOpacity));
        d->AddPolyline(points,8,color(selected?selection():theme.border),ImDrawFlags_Closed,std::max(1.f,scale*.8f));
        if(theme.frameStyle==2) {
            d->AddRect(p(x+3,y+3),p(x+w-3,y+h-3),color(theme.border,.45f),0,0,std::max(1.f,.65f*scale));
            for(float cx:{x,x+w})for(float cy:{y,y+h})diamond(cx,cy,2,accent());
        } else if(theme.frameStyle==3) {
            line(x+3,y+4,x+3,y+h-4,theme.border,.7f);line(x+w-3,y+4,x+w-3,y+h-4,theme.border,.7f);
        }
        if(selected) {
            line(x+cut+2,y+2,x+w-cut-2,y+2,accent(),1.5f);
            line(x+cut+2,y+h-2,x+w-cut-2,y+h-2,accent(),1.5f);
            diamond(x+w*.5f,y+h,2.5f,selectionMark);
        }
    }
    void medallion(float x,float y,float r,bool selected) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;

        circle(x,y,r+2,IM_COL32(0,0,0,90),false,3);
        if(themeTexture) {
            d->AddImageRounded(themeTexture,p(x-r,y-r),p(x+r,y+r),{772.f/1024,6.f/320},{1020.f/1024,314.f/320},color(IM_COL32_WHITE,options.backgroundOpacity),r*scale);
        } else d->AddCircleFilled(p(x,y),r*scale,color(dark,options.backgroundOpacity),48);
        circle(x,y,r,selected?selection():theme.border,false,.8f);
        if(selected) {
            circle(x,y,r-3,accent(),false,.8f);
            d->PathArcTo(p(x,y),(r+3)*scale,.18f*pi,.82f*pi,24);
            d->PathStroke(color(accent()),0,std::max(1.f,1.5f*scale));
            diamond(x,y-r-3,2.8f,selectionMark);
        }
    }
    void lozenge(float x,float y,float r,bool selected) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        if(themeTexture) {
            // The frame's top-left corner sits at the diamond's top point; its sides run toward the right and left points.
            const float side=r*std::sqrt(2.f);
            mirroredFrame(side,1,[&](float fx,float fy){const float u=fx/side,v=fy/side;return p(x+(u-v)*r,y-r+(u+v)*r);});
        }
        else {diamond(x,y,r,dark,true,options.backgroundOpacity);diamond(x,y,r,selected?selectionMark:muted,false);}
        if(selected){const float inner=r-4;const ImVec2 points[]{p(x,y-inner),p(x+inner,y),p(x,y+inner),p(x-inner,y)};d->AddPolyline(points,4,color(accent()),ImDrawFlags_Closed,std::max(1.f,1.2f*scale));}
    }
    void tile(float x,float y,float r,bool selected,unsigned shape) {
        if(shape==2)frame(x-r,y-r,2*r,2*r,selected);
        else if(shape==3)lozenge(x,y,r,selected);
        else medallion(x,y,r,selected);
    }
    // charge, from 0 to 1, fills an enchanted weapon's icon from the bottom when the charge shows on icons.
    void icon(float x,float y,float size,const Entry& e,ImU32 c,float charge=-1) {
        if(e.kind==Kind::combo&&e.parts.size()==2){icon(x-size*.21f,y,size*.62f,e.parts[0],c==muted?c:itemColor(e.parts[0]));icon(x+size*.21f,y,size*.62f,e.parts[1],c==muted?c:itemColor(e.parts[1]));return;}
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
    // An expanding outline in the accent color; t runs from 0 to 1. Shapes: 1 circle, 3 diamond, otherwise rectangle.
    void pulse(float x,float y,float w,float h,unsigned shape,float t) {
        if(t<0||t>=1)return;
        const float grow=7*easeOut(t),width=std::max(1.f,(2.2f-1.2f*t)*scale);const auto c=color(accent(),(1-t)*(1-t));
        const float cx=x+w*.5f,cy=y+h*.5f;
        if(shape==1)d->AddCircle(p(cx,cy),(w*.5f+grow)*scale,c,48,width);
        else if(shape==3){const float r=w*.5f+grow*1.4f;const ImVec2 points[]{p(cx,cy-r),p(cx+r,cy),p(cx,cy+r),p(cx-r,cy)};d->AddPolyline(points,4,c,ImDrawFlags_Closed,width);}
        else d->AddRect(p(x-grow,y-grow),p(x+w+grow,y+h+grow),c,2*scale,0,width);
    }
    // Marks a tile in one color with an outline, a tint over the frame, or both; grow moves the outline outward.
    // Shapes: 1 circle, 3 diamond, otherwise rectangle.
    void highlight(float x,float y,float w,float h,unsigned shape,ImU32 c,bool outline,bool tinted,float grow=0) {
        const float cx=x+w*.5f,cy=y+h*.5f,r=w*.5f+grow;
        const auto tint=color(c,.24f),halo=color(c,.28f),edge=color(c);
        const float thin=std::max(1.5f,1.8f*scale),wide=std::max(3.f,5*scale);
        if(shape==1) {
            if(tinted)d->AddCircleFilled(p(cx,cy),w*.5f*scale,tint,48);
            if(outline){d->AddCircle(p(cx,cy),r*scale,halo,48,wide);d->AddCircle(p(cx,cy),r*scale,edge,48,thin);}
        } else if(shape==3) {
            const float inner=w*.5f,outer=w*.5f+grow*1.4f;
            const ImVec2 fill[]{p(cx,cy-inner),p(cx+inner,cy),p(cx,cy+inner),p(cx-inner,cy)},points[]{p(cx,cy-outer),p(cx+outer,cy),p(cx,cy+outer),p(cx-outer,cy)};
            if(tinted)d->AddConvexPolyFilled(fill,4,tint);
            if(outline){d->AddPolyline(points,4,halo,ImDrawFlags_Closed,wide);d->AddPolyline(points,4,edge,ImDrawFlags_Closed,thin);}
        } else {
            if(tinted)d->AddRectFilled(p(x,y),p(x+w,y+h),tint,2*scale);
            if(outline){d->AddRect(p(x-grow,y-grow),p(x+w+grow,y+h+grow),halo,2*scale,0,wide);d->AddRect(p(x-grow,y-grow),p(x+w+grow,y+h+grow),edge,2*scale,0,thin);}
        }
    }
    // The tile of an equipped item, marked as the Equipped highlight setting asks.
    void equippedHighlight(float x,float y,float w,float h,unsigned shape) {
        if(const unsigned mode=options.equippedHighlight)highlight(x,y,w,h,shape,equippedMark,mode!=2,mode!=1);
    }
    // A small arrowhead: the consumable its Use key takes.
    void ready(float x,float y,float r,ImU32 c){d->AddTriangleFilled(p(x-r*.8f,y-r),p(x+r,y),p(x-r*.8f,y+r),color(c));}
    // A clockwise shade over an icon covering the remaining share of a cooldown.
    // A small soul gem at an icon's upper left marks an enchanted weapon; it fills with the remaining charge and turns
    // red at a fifth or less. An empty gem keeps a red outline.
    void soulGem(float x,float y,float r,float fraction) {
        if(options.chargeIcons!=2||!(fraction>=0))return;
        const float f=std::clamp(fraction,0.f,1.f),gx=x-r*.8f,gy=y-r*.56f,s=std::max(3.5f,r*.24f),line=std::max(1.f,scale);
        const ImVec2 points[]{p(gx,gy-s*1.25f),p(gx+s*.8f,gy-s*.35f),p(gx+s*.55f,gy+s),p(gx-s*.55f,gy+s),p(gx-s*.8f,gy-s*.35f)};
        d->AddConvexPolyFilled(points,5,color(IM_COL32(10,8,16,210)));
        if(f>0) {
            const float cut=gy+s-2.25f*s*f;
            d->PushClipRect(p(gx-s,cut),p(gx+s,gy+s+1),true);d->AddConvexPolyFilled(points,5,color(f<=.2f?ImU32(theme.warning):chargeViolet,.95f));d->PopClipRect();
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
            diamond(gx,y,r+.8f,dark,true);
            if(lit>0)diamond(gx,y,r,f<=.2f?blood:chargeViolet,true,lit<1?.35f+.65f*lit:1.f);
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
        line(x-half,y,x-7,y,IM_COL32(175,173,161,130),.7f);
        line(x+7,y,x+half,y,IM_COL32(175,173,161,130),.7f);diamond(x,y,2.5f,accent());
    }
    void key(float x,float y,unsigned scan,bool selected,float height=18) {
        if(!options.showKeys)return;
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
    float groupKeyWidth(std::size_t group,bool controller,bool vr,float height=18)const {
        if(!options.showKeys)return 0;
        if(!controller)return keyWidth(options.keys[group],height);
        const auto binding=(vr?options.vrGroups:options.controllerGroups)[group];const auto style=vr?2u:options.controllerStyle;
        if(binding.button==PadButton::none)return 0;
        if(binding.modifier==PadButton::none)return keyWidth(padGlyph(binding.button,style),height);
        return keyWidth(padGlyph(binding.modifier,style),height*.85f)+keyWidth(padGlyph(binding.button,style),height*.85f)+12;
    }
    void groupKey(float x,float y,std::size_t group,bool selected,bool controller,bool vr,float height=18) {
        if(!controller){key(x,y,options.keys[group],selected,height);return;}
        const auto binding=(vr?options.vrGroups:options.controllerGroups)[group];
        const auto style=vr?2u:options.controllerStyle;
        if(binding.button==PadButton::none)return;
        if(binding.modifier==PadButton::none){key(x,y,padGlyph(binding.button,style),selected,height);return;}
        const float h=height*.85f;
        const auto mod=padGlyph(binding.modifier,style),button=padGlyph(binding.button,style);
        const float left=keyWidth(mod,h),right=keyWidth(button,h),start=x-(left+right+12)*.5f;
        key(start+left*.5f,y,mod,selected,h);text(start+left+6,y,"+",11,silver,false,0,true);key(start+left+12+right*.5f,y,button,selected,h);
    }
    void badge(float x,float y,const Entry& entry,int quantity,float cooldown,float size=13,bool rightAligned=false,WeaponStatus weapon={}) {
        const int count=badgeQuantity(entry,quantity,weapon,options);
        const bool poisoned=options.showPoison&&weapon.poisoned;
        if(count<0&&cooldown<=0&&!poisoned)return;
        const auto label=count>=0?(count>999?std::string("999+"):std::to_string(count)):cooldown>0?std::to_string(int(std::ceil(cooldown)))+"s":std::string{};
        const auto textWidth=sans->CalcTextSizeA(size*scale,FLT_MAX,0,label.c_str()).x/scale;
        const float poisonWidth=poisoned?size+3:0,w=textWidth+poisonWidth+8;
        if(rightAligned)x-=w*.5f;
        const float left=x-w*.5f;
        if(poisoned) {
            const auto green=color(poisonMark);const float cx=left+4+size*.5f,cy=y+size*.65f,r=size*.26f;
            const auto droplet=[&]{d->PathLineTo(p(cx,cy-size*.65f));d->PathArcTo(p(cx,cy),r*scale,-pi*.2f,pi*1.2f,12);};
            droplet();d->PathFillConvex(green);
            droplet();d->PathStroke(color(IM_COL32(12,18,10,210)),ImDrawFlags_Closed,std::max(1.f,scale*.7f));
        }
        if(!label.empty())text(left+4+poisonWidth+textWidth*.5f,y,label,size,count==0?depletedMark:ivory,false,0,true);
    }
};
}
void Hud::initialize(const std::string& directory,const std::string& installedFontDirectory) {
    auto& io=ImGui::GetIO();
    const auto root=std::filesystem::path(directory);const auto latin=root/"Jost-Regular.ttf";
    std::vector<std::filesystem::path> fallbacks{latin};
    std::error_code ec;
    if(std::filesystem::is_directory(root/"fallback",ec)) {
        std::vector<std::filesystem::path> extra;
        for(const auto& file:std::filesystem::directory_iterator(root/"fallback",ec))if(file.is_regular_file()&&(file.path().extension()==".ttf"||file.path().extension()==".otf"))extra.push_back(file.path());
        std::sort(extra.begin(),extra.end());fallbacks.insert(fallbacks.end(),extra.begin(),extra.end());
    }
    const auto add=[&](const std::filesystem::path& path,float size,bool merge)->ImFont* {
        std::error_code error;
        if(!std::filesystem::is_regular_file(path,error)||std::filesystem::file_size(path,error)>32*1024*1024)return nullptr;
        auto& bytes=fontData_[path];
        if(bytes.empty()){std::ifstream file(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(file),{});}
        if(bytes.size()<12)return nullptr;
        const std::string_view magic(bytes.data(),4);
        if(magic!=std::string_view("\0\1\0\0",4)&&magic!="OTTO"&&magic!="ttcf")return nullptr;
        ImFontConfig config;config.FontDataOwnedByAtlas=false;config.MergeMode=merge;
        return io.Fonts->AddFontFromMemoryTTF(bytes.data(),int(bytes.size()),size,&config);
    };
    const auto load=[&](const std::filesystem::path& path,float size) {
        auto* font=add(path,size,false);if(!font)font=io.Fonts->AddFontDefault();
        for(const auto& fallback:fallbacks)if(fallback!=path)add(fallback,size,true);
        return font;
    };
    medium=load(root/"Jost-Medium.ttf",24);sans=load(latin,18);io.FontDefault=sans;
    themes_.load(root.parent_path()/"themes");
    std::map<std::filesystem::path,ImFont*> cache;
    const auto packFont=[&](const std::filesystem::path& path,ImFont* fallback) {
        if(path.empty())return fallback;
        if(auto it=cache.find(path);it!=cache.end())return it->second;
        std::error_code error;if(!std::filesystem::is_regular_file(path,error))return fallback;
        auto* result=load(path,20);cache[path]=result;return result;
    };
    for(const auto& t:themes_.all())skins_[t.id]={packFont(t.font,sans),packFont(t.titleFont.empty()?t.font:t.titleFont,medium),0};
    fonts_.clear();
    const auto choice=[&](const std::filesystem::path& path)->ImFont* {
        if(auto it=cache.find(path);it!=cache.end())return it->second;
        auto* font=add(path,20,false);if(!font)return nullptr;
        for(const auto& fallback:fallbacks)if(fallback!=path)add(fallback,20,true);
        cache[path]=font;return font;
    };
    const auto discover=[&](const std::filesystem::path& folder,const char* prefix) {
        std::error_code error;if(folder.empty()||!std::filesystem::is_directory(folder,error))return;
        std::vector<std::filesystem::path> files;
        for(const auto& file:std::filesystem::directory_iterator(folder,error))if(file.is_regular_file(error)&&(file.path().extension()==".ttf"||file.path().extension()==".otf"))files.push_back(file.path());
        std::sort(files.begin(),files.end());
        for(const auto& file:files) {
            if(fonts_.size()>=24)break;
            const auto name=file.filename().string();
            if(name.size()>96||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;}))continue;
            if(auto* font=choice(file))fonts_.push_back({prefix+name,file.stem().string(),font});
        }
    };
    discover(installedFontDirectory,"ui/");discover(root,"quickslot/");
}
void Hud::drawItemIcon(ImDrawList* list,ImVec2 center,float size,const Entry& entry,const Options& o,bool available)const {
    if(entry.kind==Kind::combo&&entry.parts.size()==2) {
        drawItemIcon(list,{center.x-size*.21f,center.y},size*.62f,entry.parts[0],o,available);
        drawItemIcon(list,{center.x+size*.21f,center.y},size*.62f,entry.parts[1],o,available);return;
    }
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
        metrics.textWidths[index++]=std::max(font->CalcTextSizeA(11.5f*t,FLT_MAX,0,tr(role)).x,
            font->CalcTextSizeA(17*t,FLT_MAX,0,name.c_str()).x)+2;
    });
    return metrics;
}
void Hud::draw(const Snapshot& state,const Options& o,double now,ImVec2 origin,ImVec2 viewport,ImDrawList* list,HudBounds* bounds) {
    if(bounds)*bounds={};
    const auto displacement=motion_.update(state,o,now);const auto visibilityTarget=fade_.update(state,o,now);
    // Editor previews show the settled layout; gameplay eases the HUD in and the selector open and closed.
    const bool animate=o.animateHud&&!state.editorPreview;
    if(!state.browsing)wheelHighlightSet_=false;
    const auto visibility=animate?shown_.update(visibilityTarget,now,.12f,.15f):visibilityTarget;
    const float selectorTarget=state.browsing?1.f:0.f,selectorShown=animate?selectorOpen_.update(selectorTarget,now,.1f,.08f):selectorTarget;
    const auto equipmentAlpha=equipmentFade_.update(state,o,now);
    if(visibility<=0&&equipmentAlpha<=0)return;
    // A tip takes the notice's place by the widget while no notice is showing and the selector is closed.
    const bool noticed=!state.notice.empty()&&now<state.noticeUntil,tipped=!noticed&&!state.tip.empty()&&!state.browsing;
    const bool notification=noticed||tipped;
    const auto screen=viewport.x>0&&viewport.y>0?viewport:ImGui::GetIO().DisplaySize;
    const auto metrics=equipmentMetrics(state.equipment,o);
    // The display position of the group in use; the Free arrangement gathers the selector and notices around its tile.
    std::size_t activeSlot=0;for(std::size_t i=0;i<o.groupOrder.size();++i)if(o.groupOrder[i]==state.activeGroup)activeSlot=i;
    auto geometry=hudGeometry(o,screen.x,screen.y,equipmentRows(state.equipment,o),metrics,state.rows.size(),activeSlot);
    if(state.editorPreview&&!state.placementPreview&&list){const auto display=ImGui::GetIO().DisplaySize;geometry=previewGeometry(o,display.x,display.y,screen.x,screen.y,state.browsing,!state.selectorOnly,state.equipmentOnly,equipmentRows(state.equipment,o),metrics,state.rows.size(),state.previewZoom,activeSlot);}
    const auto& focus=geometry.focus;
    const float scale=geometry.scale;
    const auto& theme=this->theme(o);const auto& skin=skins_.at(theme.id);
    Paint p{list?list:ImGui::GetBackgroundDrawList(),
        {origin.x+geometry.origin.x+geometry.selectorOrigin.x*scale,origin.y+geometry.origin.y+geometry.selectorOrigin.y*scale},
        geometry.selectorScale,o.selectorOpacity*visibility,chosenFont(o)?chosenFont(o):skin.title,chosenFont(o)?chosenFont(o):skin.body,skin.texture,keyTexture,keyGlyphs,o,theme,adjustments(o),itemIcons_,selectorTextFactor(o)};
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
        p.d->PushClipRect(p.p(0,-32),p.p(geometry.selectorWidth,geometry.selectorHeight+24),true);
        const auto node=[&](const Row& row,float x,float y,float r,bool selected,unsigned shape,float opacity) {
            auto nodePaint=p;nodePaint.alpha*=opacity;
            nodePaint.tile(x,y,r,selected,shape);
            nodePaint.icon(x,y,r*1.48f,rowIconEntry(row),row.available?nodePaint.itemColor(rowIconEntry(row)):muted,row.weapon.charge);
            nodePaint.sweep(x,y,r*.74f*nodePaint.tweaks.iconScale,cooldowns_.fraction(row.entry.form,row.cooldown));nodePaint.soulGem(x,y,r*.74f*nodePaint.tweaks.iconScale,row.weapon.charge);
            // A diamond keeps its marker in its lower point and its count inside its lower right edge.
            const bool lozenge=shape==3;
            const float marker=lozenge?x:x+(isStack(row.entry)||row.cooldown>0||(o.showAmmo&&row.weapon.ammunition>=0)||(o.showPoison&&row.weapon.poisoned)?-1.f:1.f)*r*.7f,markerY=y+r*(lozenge?.72f:.7f);
            if(!row.available&&!isStack(row.entry)){nodePaint.circle(marker,markerY,lozenge?4.5f:6.f,IM_COL32(22,18,18,240),true);nodePaint.line(marker-3,markerY+3,marker+3,markerY-3,blood,1.6f);}
            else if(row.equipped)nodePaint.diamond(marker,markerY,(lozenge?2.8f:3.5f)*nodePaint.mark(),equippedMark);
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
                std::vector<std::vector<ImVec2>> shapes;
                for(std::size_t i=0;i<n&&i<layout.ring.size();++i) {
                    const auto& row=state.rows[i];const float outer=layout.outer+(i==index?layout.lift:0.f);
                    shapes.push_back(segment(middleOf(float(i)),layout.inner,outer));
                    fill(shapes.back(),outer,p.color(dark,o.backgroundOpacity*o.wheelOpacity*frameAlpha));
                    if(row.equipped&&o.equippedHighlight!=0&&o.equippedHighlight!=1)fill(shapes.back(),outer,p.color(equippedMark,.24f));
                }
                // The highlight glides to the pointed segment while motion is on.
                const float target=middleOf(float(index));
                if(!animate||!wheelHighlightSet_)wheelHighlight_=target;
                else {
                    wheelHighlight_+=std::remainder(target-wheelHighlight_,2*pi)*std::clamp(float(now-wheelHighlightAt_)*18.f,0.f,1.f);
                    if(std::abs(std::remainder(target-wheelHighlight_,2*pi))<.002f)wheelHighlight_=target;
                }
                wheelHighlightSet_=true;wheelHighlightAt_=now;
                if(index<shapes.size()&&(filled||rim)) {
                    const float outer=layout.outer+layout.lift;
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
                    const auto& row=state.rows[i];const bool pointed=i==index;const float outer=layout.outer+(pointed?layout.lift:0.f);
                    stroke(shapes[i],outer,p.color(theme.border,frameAlpha),.8f);
                    if(row.equipped&&o.equippedHighlight!=0&&o.equippedHighlight!=2)stroke(shapes[i],outer,p.color(equippedMark),1.6f);
                    if(pointed&&outline)stroke(shapes[i],outer,p.color(selectionMark),1.8f);
                }
                for(std::size_t i=0;i<shapes.size();++i) {
                    const auto& row=state.rows[i];const bool pointed=i==index;
                    const float middle=middleOf(float(i)),outer=layout.outer+(pointed?layout.lift:0.f),reach=layout.reach+(pointed?layout.lift*.5f:0.f);
                    // Icons fill the ring: as large as its thickness and the segment's width allow.
                    const float chord=n>1?2*reach*std::sin(std::min(turn,pi)*.5f)-layout.wedgeGap:thickness;
                    const float size=std::max(8.f,std::min(thickness*.62f,chord*.72f))*(pointed&&layout.lift>0?1.12f:1.f);
                    const bool labelled=o.wheelLabels&&thickness>=size+16*text;
                    const Point slot{hub.x+std::cos(middle)*reach,hub.y+std::sin(middle)*reach-(labelled?7*text:0.f)};
                    auto item=p;if(!pointed)item.alpha*=.88f;
                    const float r=size*.5f*item.tweaks.iconScale;
                    item.icon(slot.x,slot.y,size,rowIconEntry(row),row.available?item.itemColor(rowIconEntry(row)):muted,row.weapon.charge);
                    item.sweep(slot.x,slot.y,r,cooldowns_.fraction(row.entry.form,row.cooldown));
                    item.soulGem(slot.x,slot.y,r,row.weapon.charge);
                    if(labelled)item.label(slot.x,slot.y+size*.5f+1,displayName(row.entry),11,row.available?ivory:muted,false,std::max(20.f,chord*.88f),true);
                    const Point out{std::cos(middle),std::sin(middle)};
                    if(row.equipped)item.diamond(hub.x+out.x*(outer-7),hub.y+out.y*(outer-7),2.8f*item.mark(),equippedMark);
                    if(!row.available&&!isStack(row.entry)){item.circle(slot.x+size*.36f,slot.y+size*.36f,5,IM_COL32(22,18,18,240),true);item.line(slot.x+size*.36f-3,slot.y+size*.36f+3,slot.x+size*.36f+3,slot.y+size*.36f-3,blood,1.5f);}
                    item.badge(slot.x+size*.32f,slot.y+size*.14f,row.entry,row.quantity,row.cooldown,pointed?13:11,false,row.weapon);
                }
                // The hub: an optional circle, the pointed item's name when it fits, and a dot that follows the stick or mouse.
                const float radius=layout.hubRadius;
                if(o.wheelHubCircle&&radius>0) {
                    p.d->AddCircleFilled(center,radius*p.scale,p.color(dark,o.backgroundOpacity*o.wheelOpacity*frameAlpha),64);
                    p.d->AddCircle(center,radius*p.scale,p.color(theme.border,frameAlpha),64,std::max(1.f,.8f*p.scale));
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
                if(const float length=std::hypot(state.pointerX,state.pointerY);o.wheelPointer&&radius>0&&n>1&&length>.15f)p.circle(hub.x+state.pointerX/length*radius,hub.y+state.pointerY/length*radius,3,selectionMark,true);
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
                    if(o.selector==4) {
                        const float y=layout.centerY+position*layout.listPitch;
                        auto q=p;q.alpha*=opacity;
                        q.frame(12,y-layout.listHeight*.5f,layout.width-24,layout.listHeight,offset==0);
                        q.icon(37,y,32,rowIconEntry(row),row.available?q.itemColor(rowIconEntry(row)):muted,row.weapon.charge);
                        q.sweep(37,y,16*q.tweaks.iconScale,cooldowns_.fraction(row.entry.form,row.cooldown));q.soulGem(37,y,16*q.tweaks.iconScale,row.weapon.charge);
                        q.label(66,y-8*text,displayName(row.entry),16,row.available?ivory:muted,true,layout.width-92);
                        if(row.equipped)q.diamond(53,y+15,2.5f*q.mark(),equippedMark);
                        else if(!row.available&&!isStack(row.entry)){q.circle(53,y+15,4,IM_COL32(22,18,18,240),true);q.line(50,y+18,56,y+12,blood,1.4f);}
                        q.badge(35,y+8,row.entry,row.quantity,row.cooldown,11,false,row.weapon);
                    } else {
                        const auto positionOffset=selectorOffset(o,position);
                        node(row,center+positionOffset.x,layout.centerY+positionOffset.y,selectorRadius(distance),offset==0,selectorTileShape(o),opacity);
                    }
                }
                if(n>1&&o.selector<2){p.line(16,layout.centerY-3,11,layout.centerY+2,muted);p.line(11,layout.centerY+2,16,layout.centerY+7,muted);p.line(geometry.selectorWidth-16,layout.centerY-3,geometry.selectorWidth-11,layout.centerY+2,muted);p.line(geometry.selectorWidth-11,layout.centerY+2,geometry.selectorWidth-16,layout.centerY+7,muted);}
                if(n>1&&o.selector==3){p.line(center-70,layout.centerY-37,center-65,layout.centerY-42,muted);p.line(center-65,layout.centerY-42,center-60,layout.centerY-37,muted);p.line(center-70,layout.centerY+37,center-65,layout.centerY+42,muted);p.line(center-65,layout.centerY+42,center-60,layout.centerY+37,muted);}
                p.d->PopClipRect();
            }
            if(!layout.segments||layout.nameBelow) {
                p.rule(center,layout.ruleY,o.selector==2||o.selector==wheelSelector?42.f:58.f);
                const bool round=o.selector==2||o.selector==wheelSelector;
                const float nameWidth=round?238*std::max(1.f,text):layout.width-24;
                p.label(center,layout.nameY,displayName(active->entry),round?20.f:23.f,active->available?ivory:muted,true,nameWidth,true);
                p.statusLine(center,layout.statusY,*active,14,active->available?silver:depletedMark,layout.width-24);
            }
            if(o.showDescriptions&&!active->description.empty())p.paragraph(center,layout.descriptionY,active->description,13,silver,layout.width-32,2);
            // The last line names the group the selector belongs to and the entry's place in it.
            if(o.selectorFooter)p.label(center,layout.pagerY,o.names[group]+(n>1?"   ·   "+std::to_string(index+1)+" / "+std::to_string(n):std::string{}),13,p.groupAccent?p.accent():ImU32(silver),false,layout.width-24,true);
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
        // A Free tile with its own size draws from its corner at that size.
        if(const float own=tileScale(geometry.groups,i);own!=1){q.origin=q.p(x,y);q.scale*=own;x=y=0;}
        const float text=groups.textFactor;   // card rows follow the widget's text size
        const ImU32 nameColor=q.groupAccent?*q.groupAccent:selected?ImU32(ivory):ImU32(silver);
        const bool previewing=selected&&state.browsing;
        const auto hands=entry?entryHands(*entry,equipped):0;

        // An equipped highlight takes the place of the accent line a frame draws for its equipped item.
        const bool emphasized=equipped&&!o.equippedHighlight;
        const bool ready=entry&&isConsumable(*entry)&&tile.consumable&&(!previewing||state.preview==state.selected);
        const bool badged=(o.showAmmo&&weapon.ammunition>=0)||(o.showPoison&&weapon.poisoned);
        const float keyWidth=q.groupKeyWidth(id,state.controller,state.vr);
        if(o.shape==0) {
            // A card reads icon, text, key from left to right; a mirrored card reads the other way.
            const bool wide=o.showCardNames||o.showLabels,mirror=o.mirrorCards&&wide;const float side=mirror?-1.f:1.f;
            const float width=geometry.groups.tileWidth,keyColumn=o.showKeys?std::max(cardKeyColumn,keyWidth+8):0;
            const float iconX=x+(!wide?(width-keyColumn)*.5f:mirror?width-25.f:25.f);
            const float h=geometry.groups.tileHeight-7,cy=y+3+h*.5f,textX=mirror?x+width-51:x+51,textWidth=width-59-keyColumn;
            q.frame(x,y+3,width,h,emphasized);if(equipped)q.equippedHighlight(x,y+3,width,h,0);if(previewing)q.highlight(x,y+3,width,h,0,q.accent(),true,false,3);q.pulse(x,y+3,width,h,0,pulse);
            if(entry){q.icon(iconX,cy,36,*artwork,available?q.itemColor(*artwork):muted,weapon.charge);q.sweep(iconX,cy,18*q.tweaks.iconScale,cooldowns_.fraction(entry->form,cooldown));q.soulGem(iconX,cy,18*q.tweaks.iconScale,weapon.charge);}
            else q.text(iconX,cy-15,"+",24,muted,false,0,true);
            const float block=(o.showLabels&&o.showCardNames?26*text+4:14*text),top=cy-block*.5f;
            if(o.showLabels)q.label(textX,top,o.names[id],o.showCardNames?12.f:14.f,q.groupAccent?nameColor:ImU32(silver),true,textWidth,false,mirror);
            if(o.showCardNames)q.label(textX,top+(o.showLabels?12*text+4:0),entry?displayName(*entry):tr("Empty"),14,entry&&!available?muted:ivory,true,textWidth,false,mirror);
            if(keyWidth>0)q.groupKey(mirror?x+4+keyColumn*.5f:x+width-4-keyColumn*.5f,cy-9,id,previewing,state.controller,state.vr);
            // Hand letters keep to the icon's outer corner, above the icon when a count or poison mark sits below it.
            if(hands)q.text(iconX-18*side,badged?cy-h*.5f+3:cy+h*.5f-8-10*q.mark(),handMarker(hands),10*q.mark(),equippedMark,false,0,mirror);
            else if(equipped)q.diamond(iconX+18*side,cy+15,2.5f*q.mark(),equippedMark);
            else if(ready)q.ready(iconX+18*side,cy+15,2.5f*q.mark(),equippedMark);
            if(entry)q.badge(iconX+2,cy+9,*entry,quantity,cooldown,11,false,weapon);
        } else {
            // The tile is drawn at its own size inside the widget; its name keeps the widget's text size.
            const float size=o.tileSize,tileWidth=geometry.groups.tileWidth/size,tileHeight=geometry.groups.tileHeight/size;
            const auto corner=q.p(x,y);q.origin=corner;q.scale*=size;q.textFactor/=size;
            const float x=0,y=0;
            const bool round=o.shape==1,slot=o.shape==2;
            const float cx=x+tileWidth*.5f;
            const float cy=y+(round?37.f:slot?50.f:44.f);
            const float markerX=cx+(entry&&(isStack(*entry)||cooldown>0||badged)?-1.f:1.f)*(round?21.f:slot?28.f:0.f);
            const float markerY=cy+(round?21.f:slot?25.f:31.f);
            if(round){q.medallion(cx,cy,29,emphasized);if(equipped)q.equippedHighlight(cx-29,cy-29,58,58,1);if(previewing)q.highlight(cx-29,cy-29,58,58,1,q.accent(),true,false,3);q.pulse(cx-29,cy-29,58,58,1,pulse);}
            else if(slot){q.frame(cx-41,y,82,100,emphasized,o.layout==5?(i==0?0u:i==2?1u:2u):1u);if(equipped)q.equippedHighlight(cx-41,y,82,100,0);if(previewing)q.highlight(cx-41,y,82,100,0,q.accent(),true,false,3);q.pulse(cx-41,y,82,100,0,pulse);}
            else {q.lozenge(cx,cy,42,emphasized);if(equipped)q.equippedHighlight(cx-42,cy-42,84,84,3);if(previewing)q.highlight(cx-42,cy-42,84,84,3,q.accent(),true,false,3);q.pulse(cx-42,cy-42,84,84,3,pulse);}
            if(entry) {
                // A diamond's icon sits a little low under its key and takes the middle of the tile without one.
                const float iconY=cy+(round||slot?0.f:keyWidth>0?4.f:0.f),iconSize=slot?54.f:round?40.f:keyWidth>0?42.f:46.f;
                q.icon(cx,iconY,iconSize,*artwork,available?q.itemColor(*artwork):muted,weapon.charge);
                q.sweep(cx,iconY,iconSize*.5f*q.tweaks.iconScale,cooldowns_.fraction(entry->form,cooldown));q.soulGem(cx,iconY,iconSize*.5f*q.tweaks.iconScale,weapon.charge);
            }
            else {q.line(cx-5,cy,cx+5,cy,muted);q.line(cx,cy-5,cx,cy+5,muted);}
            q.groupKey(cx,y+(round?0.f:slot?9.f:12.f),id,previewing,state.controller,state.vr,round||slot?18.f:14.f);
            if(o.showLabels)q.label(cx,y+(round?71.f:slot?100.f:88.f),o.names[id],12,nameColor,true,tileWidth-2,true);
            // Circles and diamonds keep hand letters and counts inside their lower corners, over the icon.
            if(hands) {
                if(slot)q.text(cx-27,cy+18,handMarker(hands),10*q.mark(),equippedMark);
                else q.text(round?cx-19:cx-14,(round?cy+17:cy+20)-10*q.mark(),handMarker(hands),10*q.mark(),equippedMark);
            }
            else if(equipped)q.diamond(markerX,markerY,3*q.mark(),equippedMark);
            else if(ready)q.ready(markerX,markerY,3*q.mark(),equippedMark);
            else if(entry&&!available&&!isStack(*entry)){q.circle(markerX,markerY,5,IM_COL32(22,18,18,230),true);q.line(markerX-3,markerY+3,markerX+3,markerY-3,blood,1.5f);}
            if(entry) {
                if(slot)q.badge(cx+22,cy+18,*entry,quantity,cooldown,13,false,weapon);
                else q.badge(round?cx+23:cx+18,round?cy+5:cy+8,*entry,quantity,cooldown,12,true,weapon);
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
    if(equipmentAlpha>0&&showEquipmentDisplay(state,o,now)) {
        const int start=p.d->VtxBuffer.Size;
        auto panel=p;const auto& box=geometry.equipment;
        panel.origin={origin.x+box.origin.x,origin.y+box.origin.y};panel.scale=box.scale;panel.alpha=o.equipmentOpacity*equipmentAlpha;panel.textFactor=o.equipmentTextScale;
        const float width=box.width,height=box.height,t=o.equipmentTextScale;
        if(o.equipmentStyle==0&&o.equipmentLayout==1) {
            const auto shade=panel.color(dark,.72f*panel.tweaks.frameOpacity);
            panel.d->AddRectFilledMultiColor(panel.p(0,0),panel.p(width,height),shade,shade&0x00ffffff,shade&0x00ffffff,shade);
            if(panel.themeTexture)panel.d->AddImage(panel.themeTexture,panel.p(0,0),panel.p(width,height),{.80f,.16f},{.96f,.78f},panel.color(IM_COL32_WHITE,.12f*panel.tweaks.frameOpacity));
            panel.d->AddLine(panel.p(1,3),panel.p(1,height-3),panel.color(panel.accent(),panel.tweaks.frameOpacity),std::max(1.f,1.4f*panel.scale));
        }
        unsigned slot=0;
        const auto drawRow=[&](const char* role,const std::optional<Row>& row) {
            const auto cell=box.cells[slot++];const float x=cell.origin.x+cell.icon.x,y=cell.origin.y+cell.icon.y;
            if(o.equipmentStyle==0&&o.equipmentLayout==1&&slot>1)panel.d->AddLine(panel.p(45,cell.origin.y-o.equipmentSpacing*.5f),panel.p(width-8,cell.origin.y-o.equipmentSpacing*.5f),panel.color(theme.border,.32f*panel.tweaks.frameOpacity));
            if(o.equipmentStyle==0&&o.equipmentLayout!=1)panel.frame(cell.origin.x,cell.origin.y,cell.width,cell.height,false);
            if(o.equipmentStyle>0&&o.equipmentStyle<4)panel.tile(x,y,28,false,o.equipmentStyle==2?3u:o.equipmentStyle==3?2u:1u);
            auto item=panel;if(row&&equipmentDimmed(*row))item.alpha*=.4f;
            if(box.iconSize>0) {
                if(row) {
                    item.icon(x,y,box.iconSize,rowIconEntry(*row),item.itemColor(rowIconEntry(*row)),row->weapon.charge);
                    item.sweep(x,y,box.iconSize*.5f*item.tweaks.iconScale,cooldowns_.fraction(row->entry.form,row->cooldown));item.soulGem(x,y,box.iconSize*.5f*item.tweaks.iconScale,row->weapon.charge);
                    const float size=11*std::clamp(t,.8f,1.25f),radius=o.equipmentStyle==0?box.iconSize*.5f:28.f;
                    panel.badge(x+radius*(o.equipmentStyle==2?.7f:1.f),y+(o.equipmentStyle==0?radius-size:radius*.5f),row->entry,row->quantity,row->cooldown,size,true,row->weapon);
                }
                else panel.text(x,y-8*t,"—",16*t,muted,false,0,true);
            }
            if(o.equipmentShowText||o.equipmentStyle==0||o.equipmentStyle==4) {
                const bool right=o.equipmentStyle>0&&o.equipmentStyle<4&&o.equipmentTextPosition==2;
                const float tx=cell.origin.x+cell.text.x+(right?cell.textWidth:0),ty=cell.origin.y+cell.text.y;
                panel.text(tx,ty,tr(role),11.5f*t,silver,false,cell.textWidth,cell.centered,right);
                item.text(tx,ty+16*t,equipmentTextName(row,box.iconSize>0,o),17*t,ivory,false,cell.textWidth,cell.centered,right);
            }
        };
        forEachEquipmentRow(state.equipment,o,drawRow);
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

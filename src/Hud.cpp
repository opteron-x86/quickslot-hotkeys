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
constexpr ImU32 ivory=IM_COL32(233,230,221,255),silver=IM_COL32(180,179,174,255);
constexpr ImU32 muted=IM_COL32(134,135,132,255),blood=IM_COL32(157,65,54,255);
constexpr ImU32 dark=IM_COL32(13,14,15,155);
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
    ImDrawList* d;ImVec2 origin;float scale,alpha;ImFont* medium;ImFont* sans;ImTextureID icons,themeTexture,keyTexture;const KeyGlyphs& keyGlyphs;const Options& options;const Theme& theme;ThemeAdjustments tweaks;
    ImU32 accent()const{return themeAccent(theme,options);}
    ImU32 itemColor(const Entry& e)const{return options.coloredMagic?iconColor(e):ivory;}
    ImVec2 p(float x,float y)const{return {origin.x+x*scale,origin.y+y*scale};}
    ImU32 color(ImU32 c,float opacity=1)const{if(c==ivory)c=theme.text;else if(c==silver)c=theme.secondary;else if(c==muted)c=theme.muted;else if(c==dark)c=theme.background;return (c&0xffffff)|(ImU32(float(c>>24)*std::clamp(alpha*opacity,0.f,1.f))<<24);}
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
    void label(float x,float y,const std::string& str,float size=14,ImU32 c=ivory,bool title=false,float maxWidth=0,bool center=false) {
        text(x,y,str,size*hudTextFactor(options),c,title,maxWidth,center);
    }
    void circle(float x,float y,float r,ImU32 c,bool fill=false,float width=1) {
        if(fill)d->AddCircleFilled(p(x,y),r*scale,color(c),48);
        else d->AddCircle(p(x,y),r*scale,color(c),48,std::max(1.f,width*scale));
    }
    void diamond(float x,float y,float r,ImU32 c,bool fill=true) {
        const ImVec2 points[]{p(x,y-r),p(x+r,y),p(x,y+r),p(x-r,y)};
        if(fill)d->AddConvexPolyFilled(points,4,color(c));
        else d->AddPolyline(points,4,color(c),ImDrawFlags_Closed,std::max(1.f,scale));
    }
    void textureFrame(float x,float y,float w,float h,unsigned cell) {
        const ImVec2 uv0{(float(cell)*256+4)/1024,6.f/320},uv1{(float(cell)*256+252)/1024,314.f/320};
        d->AddImage(themeTexture,p(x,y),p(x+w,y+h),uv0,uv1,color(IM_COL32_WHITE));
    }
    void frame(float x,float y,float w,float h,bool selected,unsigned variant=1) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        if(themeTexture) {
            textureFrame(x,y,w,h,variant);
            if(selected){line(x+4,y+h*.82f,x+w-4,y+h*.82f,accent(),1.2f);diamond(x+w*.5f,y+h*.82f,2.5f,ivory);}
            return;
        }
        const float cut=theme.frameStyle==1||theme.frameStyle==2?0:std::min(6.f,h*.16f);
        const ImVec2 points[]{p(x+cut,y),p(x+w-cut,y),p(x+w,y+cut),p(x+w,y+h-cut),
            p(x+w-cut,y+h),p(x+cut,y+h),p(x,y+h-cut),p(x,y+cut)};
        d->AddConvexPolyFilled(points,8,color(dark));
        d->AddPolyline(points,8,color(selected?theme.text:theme.border),ImDrawFlags_Closed,std::max(1.f,scale*.8f));
        if(theme.frameStyle==2) {
            d->AddRect(p(x+3,y+3),p(x+w-3,y+h-3),color(theme.border,.45f),0,0,std::max(1.f,.65f*scale));
            for(float cx:{x,x+w})for(float cy:{y,y+h})diamond(cx,cy,2,accent());
        } else if(theme.frameStyle==3) {
            line(x+3,y+4,x+3,y+h-4,theme.border,.7f);line(x+w-3,y+4,x+w-3,y+h-4,theme.border,.7f);
        }
        if(selected) {
            line(x+cut+2,y+2,x+w-cut-2,y+2,accent(),1.5f);
            line(x+cut+2,y+h-2,x+w-cut-2,y+h-2,accent(),1.5f);
            diamond(x+w*.5f,y+h,2.5f,ivory);
        }
    }
    void medallion(float x,float y,float r,bool selected) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;

        circle(x,y,r+2,IM_COL32(0,0,0,90),false,3);
        if(themeTexture) {
            d->AddImageRounded(themeTexture,p(x-r,y-r),p(x+r,y+r),{772.f/1024,6.f/320},{1020.f/1024,314.f/320},color(IM_COL32_WHITE),r*scale);
        } else circle(x,y,r,dark,true);
        circle(x,y,r,selected?theme.text:theme.border,false,.8f);
        if(selected) {
            circle(x,y,r-3,accent(),false,.8f);
            d->PathArcTo(p(x,y),(r+3)*scale,.18f*pi,.82f*pi,24);
            d->PathStroke(color(accent()),0,std::max(1.f,1.5f*scale));
            diamond(x,y-r-3,2.8f,ivory);
        }
    }
    void lozenge(float x,float y,float r,bool selected) {
        struct Restore {float& value;float old;~Restore(){value=old;}} restore{alpha,alpha};alpha*=tweaks.frameOpacity;
        if(themeTexture)
            d->AddImageQuad(themeTexture,p(x,y-r),p(x+r,y),p(x,y+r),p(x-r,y),
                {260.f/1024,6.f/320},{508.f/1024,6.f/320},{508.f/1024,314.f/320},{260.f/1024,314.f/320},color(IM_COL32_WHITE));
        else {diamond(x,y,r,dark);diamond(x,y,r,selected?ivory:muted,false);}
        if(selected){line(x-r+4,y,x,y+r-4,accent(),1.2f);line(x,y+r-4,x+r-4,y,accent(),1.2f);}
    }
    void tile(float x,float y,float r,bool selected,unsigned shape) {
        if(shape==2)frame(x-r,y-r,2*r,2*r,selected);
        else if(shape==3)lozenge(x,y,r,selected);
        else medallion(x,y,r,selected);
    }
    void icon(float x,float y,float size,const Entry& e,ImU32 c) {
        if(e.kind==Kind::combo&&e.parts.size()==2){icon(x-size*.21f,y,size*.62f,e.parts[0],c==muted?c:itemColor(e.parts[0]));icon(x+size*.21f,y,size*.62f,e.parts[1],c==muted?c:itemColor(e.parts[1]));return;}
        size*=tweaks.iconScale;
        if(c!=muted&&options.itemIconColor!=1)c=IM_COL32_WHITE;
        const auto region=atlasRegion(iconFor(e));
        const bool useAtlas=icons&&region.has_value();
        if(isHandAction(e)&&!useAtlas) {
            const bool fist=e.kind==Kind::unarmed;const float mirror=e.hand==Hand::left?-1.f:1.f;
            const auto point=[&](float dx,float dy){return p(x+dx*size*mirror,y+dy*size);};
            const auto rect=[&](float x1,float y1,float x2,float y2,float rounding){
                auto a=point(x1,y1),b=point(x2,y2);if(a.x>b.x)std::swap(a.x,b.x);
                d->AddRectFilled(a,b,color(c),rounding*size*scale);
            };
            rect(-.22f,-.09f,.22f,.25f,.08f);rect(-.15f,.22f,.14f,.39f,.015f);
            const float heights[]{-.31f,-.42f,-.38f,-.25f};
            for(unsigned finger=0;finger<4;++finger){const float dx=-.22f+float(finger)*.115f;rect(dx,fist?-.24f:heights[finger],dx+.095f,.02f,.04f);}
            const ImVec2 thumb[]{point(-.19f,.17f),point(-.36f,-.01f),point(-.34f,-.11f),point(-.27f,-.14f),point(-.12f,.04f)};

            if(mirror>0)d->AddConvexPolyFilled(thumb,5,color(c));
            else {ImVec2 reverse[5];for(unsigned i=0;i<5;++i)reverse[i]=thumb[4-i];d->AddConvexPolyFilled(reverse,5,color(c));}
            if(fist)line(x-size*.16f,y+size*.035f,x+size*.14f,y+size*.035f,dark,1.2f);
            return;
        }
        if(e.kind==Kind::ammo&&!useAtlas) {

            const float r=size*(e.icon==Icon::bolt?.30f:.40f),t=size*.055f;
            line(x-r,y+r,x+r,y-r,c,t);
            const ImVec2 tip[]{p(x+r,y-r),p(x+r-size*.22f,y-r+size*.04f),p(x+r-size*.04f,y-r+size*.22f)};
            d->AddTriangleFilled(tip[0],tip[1],tip[2],color(c));
            for(float at:{0.f,.12f}){const float a=size*at;line(x-r+a,y+r-a,x-r+a-size*.04f,y+r-a-size*.18f,c,t);line(x-r+a,y+r-a,x-r+a+size*.18f,y+r-a+size*.04f,c,t);}
            return;
        }
        if(!useAtlas){diamond(x,y,size*.3f,c,false);text(x,y-size*.2f,"?",size*.4f,c,false,0,true);return;}
        const ImVec2 uv0{region->u0,region->v0},uv1{region->u1,region->v1};

        const auto tint=atlasTint(color(c),options.itemIconColor,c==muted);
        d->AddImage(icons,p(x-size*.5f+1,y-size*.5f+2),p(x+size*.5f+1,y+size*.5f+2),uv0,uv1,color(IM_COL32(0,0,0,150)));
        d->AddImage(icons,p(x-size*.5f,y-size*.5f),p(x+size*.5f,y+size*.5f),uv0,uv1,tint);
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
        const float w=std::max(16.f,sans->CalcTextSizeA(12*scale,FLT_MAX,0,label.c_str()).x/scale+8);
        d->AddRectFilled(p(x-w*.5f,y),p(x+w*.5f,y+16),color(IM_COL32(15,16,17,210)),2*scale);
        d->AddRect(p(x-w*.5f,y),p(x+w*.5f,y+16),color(selected?ivory:silver),2*scale);
        text(x,y,label,12,selected?ivory:silver,true,0,true);
    }
    void groupKey(float x,float y,std::size_t group,bool selected,bool controller,bool vr,float height=18) {
        if(!controller){key(x,y,options.keys[group],selected,height);return;}
        const auto binding=(vr?options.vrGroups:options.controllerGroups)[group];
        const auto style=vr?2u:options.controllerStyle;
        if(binding.button==PadButton::none)return;
        if(binding.modifier==PadButton::none){key(x,y,padGlyph(binding.button,style),selected,height);return;}
        const float h=height*.85f;
        const auto width=[&](unsigned glyph){
            if((glyph-256)%16<4)return h;
            if(options.keyIcons&&keyTexture&&glyph<keyGlyphs.size()&&keyGlyphs[glyph].aspect>0)return h*keyGlyphs[glyph].aspect;
            if(glyph>=276&&glyph<=279)return h;
            return std::max(16.f,sans->CalcTextSizeA(12*scale,FLT_MAX,0,keyLabel(glyph).c_str()).x/scale+8);
        };
        const auto mod=padGlyph(binding.modifier,style),button=padGlyph(binding.button,style);
        const float left=width(mod),right=width(button),start=x-(left+right+12)*.5f;
        key(start+left*.5f,y,mod,selected,h);text(start+left+6,y,"+",11,silver,false,0,true);key(start+left+12+right*.5f,y,button,selected,h);
    }
    void badge(float x,float y,const Entry& entry,int quantity,float cooldown,float size=13,bool rightAligned=false) {
        if(!isStack(entry)&&cooldown<=0)return;
        const auto label=isStack(entry)?(quantity>999?std::string("999+"):std::to_string(std::max(0,quantity))):std::to_string(int(std::ceil(cooldown)))+"s";
        const auto w=sans->CalcTextSizeA(size*scale,FLT_MAX,0,label.c_str()).x/scale+8;
        if(rightAligned)x-=w*.5f;
        d->AddRectFilled(p(x-w*.5f,y-1),p(x+w*.5f,y+size+2),color(IM_COL32(12,13,14,215)),2*scale);
        text(x,y,label,size,isStack(entry)&&quantity<=0?IM_COL32(219,143,132,255):ivory,false,0,true);
    }
};
}
void Hud::initialize(const std::string& directory) {
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
}
void Hud::draw(const Snapshot& state,const Options& o,double now,ImVec2 origin,ImVec2 viewport,ImDrawList* list,HudBounds* bounds) {
    if(bounds)*bounds={};
    const auto displacement=motion_.update(state,o,now);const auto visibility=fade_.update(state,o,now);
    const auto equipmentAlpha=equipmentFade_.update(state,o,now);
    if(visibility<=0&&equipmentAlpha<=0)return;
    const bool notification=!state.notice.empty()&&now<state.noticeUntil;
    const auto screen=viewport.x>0&&viewport.y>0?viewport:ImGui::GetIO().DisplaySize;
    auto geometry=hudGeometry(o,screen.x,screen.y,equipmentRows(state.equipment));
    if(state.editorPreview&&!state.placementPreview&&list){const auto display=ImGui::GetIO().DisplaySize;geometry=previewGeometry(o,display.x,display.y,screen.x,screen.y,state.browsing,!state.selectorOnly,state.equipmentOnly,equipmentRows(state.equipment));}
    const float scale=geometry.scale;
    const auto& theme=themes_.get(o.themeId);const auto& skin=skins_.at(theme.id);
    Paint p{list?list:ImGui::GetBackgroundDrawList(),
        {origin.x+geometry.origin.x+geometry.selectorOrigin.x*scale,origin.y+geometry.origin.y+geometry.selectorOrigin.y*scale},
        scale,o.opacity*visibility,skin.title,skin.body,iconTexture(o),skin.texture,keyTexture,keyGlyphs,o,theme,adjustments(o)};
    const auto collect=[&](int start,Bounds& box){for(int i=start;i<p.d->VtxBuffer.Size;++i){const auto v=p.d->VtxBuffer[i].pos;box.include({v.x,v.y});}};
    const int selectorStart=p.d->VtxBuffer.Size;const float text=hudTextFactor(o);
    const float center=geometry.selectorWidth*.5f;
    const auto group=std::min(state.activeGroup,Arsenal::count-1);
    const auto index=state.browsing?state.preview:state.selected;
    const Row* active=index<state.rows.size()?&state.rows[index]:nullptr;
    if(state.browsing&&visibility>0) {
        const auto n=state.rows.size();
        p.d->PushClipRect(p.p(0,-32),p.p(geometry.selectorWidth,geometry.selectorHeight+24),true);
        const auto node=[&](const Row& row,float x,float y,float r,bool selected,unsigned shape,float opacity) {
            auto nodePaint=p;nodePaint.alpha*=opacity;
            nodePaint.tile(x,y,r,selected,shape);
            nodePaint.icon(x,y,r*1.48f,row.entry,row.available?nodePaint.itemColor(row.entry):muted);
            const float marker=x+(isStack(row.entry)||row.cooldown>0?-1.f:1.f)*r*.7f;
            if(!row.available&&!isStack(row.entry)){nodePaint.circle(marker,y+r*.7f,6,IM_COL32(22,18,18,240),true);nodePaint.line(marker-3,y+r*.7f+3,marker+3,y+r*.7f-3,blood,1.6f);}
            else if(row.equipped)nodePaint.diamond(marker,y+r*.7f,3.5f,ivory);
            nodePaint.badge(x+r*.58f,y+r*.5f,row.entry,row.quantity,row.cooldown,selected?15:12);
        };
        const auto layout=selectorGeometry(o);
        if(active) {
            if(o.selector==2) {
                const auto visible=std::min(n,std::size_t(8));
                for(std::size_t i=0;i<visible;++i) {
                    const auto offset=i<=visible/2?std::ptrdiff_t(i):std::ptrdiff_t(i)-std::ptrdiff_t(visible);
                    const auto at=std::size_t((std::ptrdiff_t(index)+offset+std::ptrdiff_t(n))%std::ptrdiff_t(n));
                    const float angle=-pi*.5f+(float(i)+displacement)*2*pi/float(visible);
                    node(state.rows[at],center+layout.ringRadiusX*std::cos(angle),layout.centerY+layout.ringRadiusY*std::sin(angle),i==0?29.f:22.f,i==0,o.selectorShape?o.selectorShape:1u,i==0?1.f:.72f);
                }
            } else {
                p.d->PushClipRect(p.p(0,-32),p.p(layout.width,layout.ruleY-2),true);
                for(int offset=-2;offset<=2;++offset) {
                    if(offset<0&&std::size_t(-offset)>(n-1)/2)continue;
                    if(offset>0&&std::size_t(offset)>n/2)continue;
                    const auto at=std::size_t((int(index)+offset+int(n))%int(n));
                    const auto& row=state.rows[at];
                    const float position=float(offset)+displacement,distance=std::abs(position);
                    const float opacity=distance<=1?1-.22f*distance:std::max(0.f,.78f-.30f*(distance-1));
                    if(o.selector==4) {
                        const float y=layout.centerY+position*layout.listPitch;
                        auto q=p;q.alpha*=opacity;
                        q.frame(12,y-layout.listHeight*.5f,layout.width-24,layout.listHeight,offset==0);
                        q.icon(37,y,32,row.entry,row.available?q.itemColor(row.entry):muted);
                        q.label(66,y-8*text,displayName(row.entry),16,row.available?ivory:muted,true,layout.width-92);
                        if(row.equipped)q.diamond(53,y+15,2.5f,ivory);
                        else if(!row.available&&!isStack(row.entry)){q.circle(53,y+15,4,IM_COL32(22,18,18,240),true);q.line(50,y+18,56,y+12,blood,1.4f);}
                        q.badge(35,y+8,row.entry,row.quantity,row.cooldown,11);
                    } else {
                        const float r=distance<1?40-12*distance:std::max(18.f,28-7*(distance-1));
                        const float step=distance<=1?distance*(68+o.selectorSpacing):68+o.selectorSpacing+(distance-1)*(49+o.selectorSpacing);
                        const float shift=position<0?-step:step;
                        node(row,o.selector==3?center:center+shift,layout.centerY+(o.selector==3?shift:o.selector==1?distance*13.f:0),r,offset==0,
                             o.selectorShape?o.selectorShape:o.selector==0?2u:1u,opacity);
                    }
                }
                if(n>1&&o.selector<2){p.line(16,layout.centerY-3,11,layout.centerY+2,muted);p.line(11,layout.centerY+2,16,layout.centerY+7,muted);p.line(geometry.selectorWidth-16,layout.centerY-3,geometry.selectorWidth-11,layout.centerY+2,muted);p.line(geometry.selectorWidth-11,layout.centerY+2,geometry.selectorWidth-16,layout.centerY+7,muted);}
                if(n>1&&o.selector==3){p.line(center-70,layout.centerY-37,center-65,layout.centerY-42,muted);p.line(center-65,layout.centerY-42,center-60,layout.centerY-37,muted);p.line(center-70,layout.centerY+37,center-65,layout.centerY+42,muted);p.line(center-65,layout.centerY+42,center-60,layout.centerY+37,muted);}
                p.d->PopClipRect();
            }
            p.rule(center,layout.ruleY,o.selector==2?42.f:58.f);
            const float nameWidth=o.selector==2?238*std::max(1.f,text):layout.width-24;
            p.label(center,layout.nameY,displayName(active->entry),o.selector==2?20.f:23.f,active->available?ivory:muted,true,nameWidth,true);
            p.label(center,layout.statusY,rowStatus(*active),13,active->available?silver:IM_COL32(209,134,122,255),false,layout.width-24,true);
            if(n>1) {
                const float start=center-float(n-1)*4;
                for(std::size_t i=0;i<n;++i)p.diamond(start+float(i)*8,layout.pagerY,i==index?2.7f:1.6f,i==index?ivory:IM_COL32(155,151,141,140),i==index);
                p.label(center,layout.pagerY+6,std::to_string(index+1)+" / "+std::to_string(n),12,silver,false,100,true);
            }
            if(o.showHints) {
                std::string cancel;
                if(state.controller) {
                    if(state.vr)cancel=tr("Main B/Y  cancel");
                    else if(o.controllerActions[0].button!=PadButton::none)cancel=padLabel(o.controllerActions[0],o.controllerStyle)+"  "+tr("cancel");
                }else {
                    const auto binding=o.selectorBindings[unsigned(KeyboardAction::cancel)];
                    if(binding.modifiers&keyModifier::ctrl)cancel+="Ctrl + ";
                    if(binding.modifiers&keyModifier::shift)cancel+="Shift + ";
                    if(binding.modifiers&keyModifier::alt)cancel+="Alt + ";
                    cancel+=(binding.key?keyLabel(binding.key):std::string(tr("Right-click")))+"  "+tr("cancel");
                }
                const std::string browse=state.controller?(state.vr?tr("Stick / pad  choose"):tr("Stick / buttons  choose")):tr("Scroll  choose");
                p.label(center,layout.hintY,browse+(cancel.empty()?std::string{}:"   ·   "+cancel),13,silver,false,layout.width-24,true);
            }
        } else {
            const float y=std::min(layout.centerY,75.f);
            p.diamond(center,y,26,IM_COL32(155,151,141,130),false);p.text(center,y-18,"+",26,silver,false,0,true);
            p.label(center,y+46,tr("No items"),22,ivory,true,layout.width-24,true);
        }
    }
    if(state.browsing&&visibility>0)p.d->PopClipRect();
    if(bounds)collect(selectorStart,bounds->selector);
    const int widgetStart=p.d->VtxBuffer.Size;
    Paint groups=p;groups.origin={origin.x+geometry.origin.x+geometry.groupOrigin.x*scale,origin.y+geometry.origin.y+geometry.groupOrigin.y*scale};
    const bool showGroups=visibility>0&&showGroupDisplay(state,o,now);
    if(showGroups)for(std::size_t i=0;i<geometry.groups.tiles.size();++i) {
        const int tileStart=p.d->VtxBuffer.Size;
        const auto id=o.groupOrder[i];
        const float groupAlpha=groupVisibility(state,o,id,now);
        if(groupAlpha<=0)continue;
        const auto& tile=state.groups[id];const bool selected=id==group;
        const auto* entry=selected&&active?&active->entry:tile.entry?&*tile.entry:nullptr;
        const bool equipped=selected&&active?active->equipped:tile.equipped;
        const bool available=selected&&active?active->available:tile.available;
        const int quantity=selected&&active?active->quantity:tile.quantity;
        const float cooldown=selected&&active?active->cooldown:tile.cooldown;
        const auto pos=geometry.groups.tiles[i];const float x=pos.x,y=pos.y;
        auto q=groups;q.alpha*=groupAlpha;
        const bool previewing=selected&&state.browsing;
        const auto hands=entry?entryHands(*entry,equipped):0;

        const bool emphasized=equipped;
        const bool ready=entry&&isConsumable(*entry)&&tile.consumable&&(!previewing||state.preview==state.selected);
        if(o.shape==0) {
            const float width=geometry.groups.tileWidth,iconX=x+(o.showCardNames||o.showLabels?25.f:width*.5f);
            const float h=geometry.groups.tileHeight-7,cy=y+3+h*.5f;
            q.frame(x,y+3,width,h,emphasized);
            if(entry)q.icon(iconX,cy,36,*entry,available?q.itemColor(*entry):muted);
            else q.text(iconX,cy-15,"+",24,muted,false,0,true);
            const float block=(o.showLabels&&o.showCardNames?26*text+4:14*text),top=cy-block*.5f;
            if(o.showLabels)q.label(x+51,top,o.names[id],o.showCardNames?12.f:14.f,silver,true,width-59);
            if(o.showCardNames)q.label(x+51,top+(o.showLabels?12*text+4:0),entry?displayName(*entry):tr("Empty"),14,entry&&!available?muted:ivory,true,width-59);
            q.groupKey(x+width-20,y-3,id,previewing,state.controller,state.vr);
            if(previewing)q.diamond(x+width*.5f,y-3,2.5f,q.accent());
            if(hands)q.text(iconX-18,cy+14,handMarker(hands),10,ivory);
            else if(equipped||ready)q.diamond(iconX+18,cy+15,2.5f,ivory);
            if(entry)q.badge(iconX+2,cy+9,*entry,quantity,cooldown,11);
        } else {
            const float cx=x+geometry.groups.tileWidth*.5f;
            const float cy=y+(o.shape==1?37.f:o.shape==2?50.f:44.f);
            const float markerX=cx+(entry&&(isStack(*entry)||cooldown>0)?-1.f:1.f)*(o.shape==1?21.f:o.shape==2?28.f:0.f);
            const float markerY=cy+(o.shape==1?21.f:o.shape==2?25.f:30.f);
            if(o.shape==1)q.medallion(cx,cy,29,emphasized);
            else if(o.shape==2)q.frame(cx-41,y,82,100,emphasized,o.layout==5?(i==0?0u:i==2?1u:2u):1u);
            else q.lozenge(cx,cy,42,emphasized);
            if(entry)q.icon(cx,cy+(o.shape==3?6.f:0.f),o.shape==2?54.f:o.shape==3?36.f:40.f,*entry,available?q.itemColor(*entry):muted);
            else {q.line(cx-5,cy,cx+5,cy,muted);q.line(cx,cy-5,cx,cy+5,muted);}
            q.groupKey(cx,y+(o.shape==1?0.f:o.shape==2?9.f:16.f),id,previewing,state.controller,state.vr,o.shape==3?16.f:18.f);
            if(previewing)q.diamond(cx,y+geometry.groups.tileHeight+2,2.5f,q.accent());
            if(o.showLabels)q.label(cx,y+(o.shape==1?71.f:o.shape==2?100.f:88.f),o.names[id],12,selected?ivory:silver,true,geometry.groups.tileWidth-2,true);
            if(hands)q.text(cx-27,cy+18,handMarker(hands),10,ivory);
            else if(equipped||ready)q.diamond(markerX,markerY,3,ivory);
            else if(entry&&!available&&!isStack(*entry)){q.circle(markerX,markerY,5,IM_COL32(22,18,18,230),true);q.line(markerX-3,markerY+3,markerX+3,markerY-3,blood,1.5f);}
            if(entry)q.badge(cx+(o.shape==3?18.f:22.f),cy+(o.shape==3?14.f:18.f),*entry,quantity,cooldown);
        }
        if(bounds){Bounds box;collect(tileStart,box);bounds->tiles.push_back(box);}
    }
    const bool captionAbove=groups.origin.y-origin.y+(geometry.groups.height+geometry.captionHeight)*scale>screen.y-8;
    if(!state.browsing&&showGroups&&active&&now<state.revealUntil&&groupVisibility(state,o,group,now)>0) {
        auto caption=groups;const float width=std::min(std::max(240.f,geometry.groups.width),(screen.x-16)/scale);
        caption.origin.x=origin.x+std::clamp(groups.origin.x-origin.x+geometry.groups.width*scale*.5f,8+width*scale*.5f,screen.x-8-width*scale*.5f);
        const float y=captionAbove?-geometry.captionHeight:geometry.groups.height+7;
        caption.origin.y=std::clamp(groups.origin.y+y*scale,origin.y+8,origin.y+screen.y-8-geometry.captionHeight*scale);
        caption.label(0,0,displayName(active->entry),15,active->available?ivory:muted,true,width,true);
    }
    if(bounds)collect(widgetStart,bounds->widget);
    if(equipmentAlpha>0&&showEquipmentDisplay(state,o,now)) {
        const int start=p.d->VtxBuffer.Size;
        auto panel=p;const auto& box=geometry.equipment;
        panel.origin={origin.x+box.origin.x,origin.y+box.origin.y};panel.scale=box.scale;panel.alpha=o.opacity*equipmentAlpha;
        const float width=box.width,height=box.height,t=o.textScale*o.equipmentTextScale;
        if(o.equipmentStyle==0) {
            const auto shade=panel.color(dark,.72f*panel.tweaks.frameOpacity);
            panel.d->AddRectFilledMultiColor(panel.p(0,0),panel.p(width,height),shade,shade&0x00ffffff,shade&0x00ffffff,shade);
            if(panel.themeTexture)panel.d->AddImage(panel.themeTexture,panel.p(0,0),panel.p(width,height),{.80f,.16f},{.96f,.78f},panel.color(IM_COL32_WHITE,.12f*panel.tweaks.frameOpacity));
            panel.d->AddLine(panel.p(1,3),panel.p(1,height-3),panel.color(panel.accent(),panel.tweaks.frameOpacity),std::max(1.f,1.4f*panel.scale));
        }
        unsigned slot=0;
        const auto drawRow=[&](const char* role,const std::optional<Row>& row) {
            const auto cell=box.cells[slot++];const float x=cell.x+box.icon.x,y=cell.y+box.icon.y;
            if(o.equipmentStyle==0&&slot>1)panel.d->AddLine(panel.p(45,cell.y),panel.p(width-8,cell.y),panel.color(theme.border,.32f*panel.tweaks.frameOpacity));
            if(o.equipmentStyle>0&&o.equipmentStyle<4)panel.tile(x,y,28,false,o.equipmentStyle==2?3u:o.equipmentStyle==3?2u:1u);
            auto item=panel;if(row&&equipmentDimmed(*row))item.alpha*=.4f;
            if(box.iconSize>0) {
                if(row) {
                    item.icon(x,y,box.iconSize,row->entry,item.itemColor(row->entry));
                    const float size=11*std::clamp(t,.8f,1.25f),radius=o.equipmentStyle==0?box.iconSize*.5f:28.f;
                    panel.badge(x+radius*(o.equipmentStyle==2?.7f:1.f),y+(o.equipmentStyle==0?radius-size:radius*.5f),row->entry,row->quantity,row->cooldown,size,true);
                }
                else panel.text(x,y-8*t,"—",16*t,muted,false,0,true);
            }
            const bool right=o.equipmentStyle>0&&o.equipmentStyle<4&&o.equipmentTextPosition==2;
            const float tx=cell.x+box.text.x+(right?box.textWidth:0),ty=cell.y+box.text.y;
            panel.text(tx,ty,tr(role),11.5f*t,silver,false,box.textWidth,box.centered,right);
            item.text(tx,ty+16*t,equipmentTextName(row,box.iconSize>0),17*t,ivory,false,box.textWidth,box.centered,right);
        };
        if(twoHandedDisplay(state.equipment))drawRow("Both hands",state.equipment.hands[0]);
        else {drawRow("Right hand",state.equipment.hands[0]);drawRow("Left hand",state.equipment.hands[1]);}
        drawRow("Consumable",state.equipment.consumable);drawRow("Power / shout",state.equipment.voice);
        if(bounds)collect(start,bounds->equipment);
    }
    if(notification&&visibility>0) {
        const int noticeStart=p.d->VtxBuffer.Size;
        auto toast=p;toast.alpha*=float(std::clamp((state.noticeUntil-now)/.35,0.,1.));
        if(!state.browsing){
            const float width=std::min(std::max(240.f,geometry.groups.width),(screen.x-16)/scale);
            toast.origin.x=origin.x+std::clamp(groups.origin.x-origin.x+geometry.groups.width*scale*.5f,8+width*scale*.5f,screen.x-8-width*scale*.5f)-center*scale;
            toast.origin.y=groups.origin.y-(captionAbove&&showGroups&&active?geometry.captionHeight+8:8.f)*scale;
            if(toast.origin.y-origin.y-27*text*scale<8)toast.origin.y=groups.origin.y+(geometry.height+geometry.captionHeight+35*text)*scale;
            toast.origin.y=std::clamp(toast.origin.y,origin.y+8+27*text*scale,origin.y+screen.y-8);
        }
        toast.label(center,-27*text,state.notice,13,ivory,false,state.browsing?410.f:std::max(240.f,geometry.groups.width),true);
        toast.rule(center,-6,36);
        if(bounds)collect(noticeStart,state.browsing?bounds->selector:bounds->widget);
    }
}
}

#pragma once
#include "Model.h"
#include <limits>

namespace quickslot {
struct Point {float x=0,y=0;};
struct LayoutGeometry {std::vector<Point> tiles;float width=0,height=0,tileWidth=0,tileHeight=0;};
inline LayoutGeometry groupGeometry(const Options& o) {
    LayoutGeometry g;const auto count=std::clamp(o.groupCount,std::size_t(1),Arsenal::count);
    switch(o.shape) {
    case 1:g.tileWidth=o.showLabels?76.f:68.f;g.tileHeight=o.showLabels?86.f:70.f;break;
    case 2:g.tileWidth=82;g.tileHeight=o.showLabels?116.f:100.f;break;
    case 3:g.tileWidth=88;g.tileHeight=o.showLabels?104.f:88.f;break;
    default:g.tileWidth=o.showCardNames||o.showLabels?156.f:64.f;g.tileHeight=56;break;
    }
    const float dx=g.tileWidth+o.spacing,dy=g.tileHeight+o.spacing;
    constexpr float pi=3.141592654f;
    for(std::size_t i=0;i<count;++i) {
        Point p;
        if(count==1){g.tiles.push_back(p);continue;}
        switch(o.layout) {
        case 1:p={0,float(i)*dy};break;
        case 2: {
            constexpr std::array<Point,8> compact{{{1,0},{2,1},{1,2},{0,1},{0,-1},{2,-1},{0,3},{2,3}}};
            constexpr std::array<Point,8> spaced{{{1,0},{2,1},{1,2},{0,1},{0,0},{2,0},{0,2},{2,2}}};
            const bool tight=!o.showLabels&&(o.shape==1||o.shape==3);
            const float pitch=o.shape==3?(88+o.spacing)*.5f:(74+o.spacing)/std::sqrt(2.f);
            const auto slot=(tight?compact:spaced)[i];p={slot.x*(tight?pitch:dx),slot.y*(tight?pitch:dy)};break;
        }
        case 3: {
            constexpr std::array<Point,8> cross{{{1,1},{0,1},{2,1},{1,0},{1,2},{0,0},{2,0},{0,2}}};
            const auto slot=cross[i];p={slot.x*dx,slot.y*dy};break;
        }
        case 4: {
            const float angle=-pi/2+float(i)*2*pi/float(count);
            const float radius=std::max(1.f,1.414214f/(2*std::sin(pi/float(count))));
            if(o.shape==1&&!o.showLabels) {
                const float r=(74+o.spacing)/(2*std::sin(pi/float(count)));
                p={std::cos(angle)*r,std::sin(angle)*r};
            } else p={(std::cos(angle)+1)*dx*radius,(std::sin(angle)+1)*dy*radius};
            break;
        }
        case 5: {
            constexpr std::array<Point,8> stb{{{1,0},{2,.5f},{1,1},{0,.5f},{0,-.5f},{2,-.5f},{0,1.5f},{2,1.5f}}};
            const auto slot=stb[i];p={slot.x*dx,slot.y*dy};break;
        }
        default:p={float(i)*dx,0};break;
        }
        g.tiles.push_back(p);
    }
    float minX=g.tiles[0].x,minY=g.tiles[0].y;
    for(const auto& p:g.tiles){minX=std::min(minX,p.x);minY=std::min(minY,p.y);}
    for(auto& p:g.tiles){p.x-=minX;p.y-=minY;g.width=std::max(g.width,p.x+g.tileWidth);g.height=std::max(g.height,p.y+g.tileHeight);}
    return g;
}
struct SelectorGeometry {
    float width=420,height=155,centerY=16,ruleY=67,nameY=76,statusY=104,pagerY=132,listPitch=0;
};
inline SelectorGeometry selectorGeometry(const Options& o) {
    SelectorGeometry s;s.width=std::max(420.f,328+o.selectorSpacing*4);
    if(o.selector==2) {
        s.centerY=78+o.selectorSpacing*.5f;s.ruleY=111+o.selectorSpacing*.5f;
        s.nameY=57+o.selectorSpacing*.5f;s.statusY=84+o.selectorSpacing*.5f;s.pagerY=191+o.selectorSpacing;
    } else if(o.selector==3) {
        s.width=320;s.centerY=113+o.selectorSpacing*2;s.ruleY=263+o.selectorSpacing*4;
        s.nameY=s.ruleY+9;s.statusY=s.ruleY+37;s.pagerY=s.ruleY+65;
    } else if(o.selector==4) {
        s.width=320;s.listPitch=48+o.selectorSpacing*.5f;s.centerY=s.listPitch*2-5;
        s.ruleY=s.listPitch*4+30;s.nameY=s.ruleY+9;s.statusY=s.ruleY+37;s.pagerY=s.ruleY+65;
    }
    s.height=s.pagerY+23+(o.showHints?24.f:0.f);return s;
}
struct HudGeometry {LayoutGeometry groups;Point origin,groupOrigin,selectorOrigin;float scale=1,width=0,height=0,selectorHeight=150,selectorWidth=420;};
inline HudGeometry hudGeometry(const Options& o,float screenX,float screenY) {
    HudGeometry g;g.groups=groupGeometry(o);g.width=g.groups.width;
    g.groupOrigin={0,o.shape==0?3.f:0.f};g.height=g.groups.height+g.groupOrigin.y;
    const auto selectorLayout=selectorGeometry(o);g.selectorHeight=selectorLayout.height;g.selectorWidth=selectorLayout.width;
    const float sh=g.selectorHeight+32,resolutionScale=std::clamp(screenY/1080.f,.75f,3.f);
    g.scale=std::max(.05f,std::min({o.scale*resolutionScale,(screenX-16)/std::max(g.width,g.selectorWidth),(screenY-16)/std::max(g.height,sh)}));
    const float w=g.width*g.scale,h=g.height*g.scale,sw=g.selectorWidth*g.scale,soh=sh*g.scale,gap=o.selectorGap*g.scale;
    g.origin={8+std::max(0.f,screenX-16-w)*o.positionX,8+std::max(0.f,screenY-16-h)*o.positionY};
    const auto clamp=[&](Point p){return Point{std::clamp(p.x,8.f,std::max(8.f,screenX-8-sw)),std::clamp(p.y,8.f,std::max(8.f,screenY-8-soh))};};
    Point selector;
    if(o.selectorDock==4)selector={8+std::max(0.f,screenX-16-sw)*o.selectorX,8+std::max(0.f,screenY-16-soh)*o.selectorY};
    else {
        const std::array<Point,4> docks{{{g.origin.x+(w-sw)*.5f,g.origin.y-gap-soh},{g.origin.x+(w-sw)*.5f,g.origin.y+h+gap},
            {g.origin.x-gap-sw,g.origin.y+(h-soh)*.5f},{g.origin.x+w+gap,g.origin.y+(h-soh)*.5f}}};
        const unsigned preferred=std::min(o.selectorDock,3u);
        const std::array<unsigned,4> order{preferred,preferred^1u,(preferred+2)%4,((preferred+2)%4)^1u};
        float best=std::numeric_limits<float>::max();
        for(const auto side:order) {
            const auto p=clamp(docks[side]);
            const float overlapX=std::max(0.f,std::min(p.x+sw,g.origin.x+w)-std::max(p.x,g.origin.x));
            const float overlapY=std::max(0.f,std::min(p.y+soh,g.origin.y+h)-std::max(p.y,g.origin.y));
            const float score=overlapX*overlapY;
            if(score<best){selector=p;best=score;}
            if(score==0)break;
        }
    }
    g.selectorOrigin={(selector.x-g.origin.x)/g.scale,(selector.y-g.origin.y)/g.scale+32};
    return g;
}
struct EditorGeometry {float scale=1,width=1220,height=800;};
inline EditorGeometry editorGeometry(const Options&,float screenX,float screenY) {
    EditorGeometry g;
    g.scale=std::max(.5f,std::min({std::max(1.f,screenY/1080.f),(screenX-24)/760.f,(screenY-24)/560.f}));
    g.width=std::min(1220*g.scale,screenX-24);g.height=std::min(800*g.scale,screenY-24);return g;
}
inline float placementGridStep(float screenHeight) {return 48*std::max(.5f,screenHeight/1080.f);}
inline float snapPosition(float normalized,float extent,float screen,float step) {
    if(normalized<=0)return 0;
    if(normalized>=1)return 1;
    const float travel=screen-16-extent;if(travel<=0||step<=0)return 0;
    const float center=8+normalized*travel+extent*.5f;
    const float snapped=screen*.5f+std::round((center-screen*.5f)/step)*step;
    return std::clamp((snapped-extent*.5f-8)/travel,0.f,1.f);
}
inline HudGeometry fitPreviewGeometry(HudGeometry g,float width,float height,bool selector,bool groups=true) {
    float left=g.origin.x,top=g.origin.y,right=left+g.width*g.scale,bottom=top+(g.height+(selector?0:40))*g.scale;
    if(selector) {
        const float x=g.origin.x+g.selectorOrigin.x*g.scale,y=g.origin.y+(g.selectorOrigin.y-32)*g.scale;
        if(!groups){left=x;top=y;right=x+g.selectorWidth*g.scale;bottom=y+(g.selectorHeight+40)*g.scale;}
        else {left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+g.selectorWidth*g.scale);bottom=std::max(bottom,y+(g.selectorHeight+40)*g.scale);}
    }
    const float margin=std::min(width,height)*.06f;
    const float factor=std::max(.05f,std::min((width-margin*2)/std::max(1.f,right-left),(height-margin*2)/std::max(1.f,bottom-top)));
    g.origin={(width-(right-left)*factor)*.5f+(g.origin.x-left)*factor,(height-(bottom-top)*factor)*.5f+(g.origin.y-top)*factor};
    g.scale*=factor;return g;
}
inline void layoutPreset(Options& o,unsigned preset) {
    o.showLabels=false;
    o.selector=1;o.selectorShape=0;o.selectorSpacing=8;o.spacing=4;
    if(preset==0){o.layout=5;o.shape=2;o.selectorShape=2;}
    else if(preset==1){o.layout=2;o.shape=3;o.selectorShape=3;}
    else {o.layout=4;o.shape=1;o.selector=0;o.selectorShape=1;o.spacing=2;}
}
}

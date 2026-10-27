#pragma once
#include "Model.h"
#include <limits>

namespace quickslot {
struct Point {float x=0,y=0;};
struct Bounds {
    Point min{},max{};bool valid=false;
    float width()const{return max.x-min.x;}
    float height()const{return max.y-min.y;}
    void include(Point p){if(!valid){min=max=p;valid=true;}else{min.x=std::min(min.x,p.x);min.y=std::min(min.y,p.y);max.x=std::max(max.x,p.x);max.y=std::max(max.y,p.y);}}
};
struct HudBounds {Bounds widget,selector,equipment;std::vector<Bounds> tiles;};

inline float hudTextFactor(const Options& o){return o.textScale*std::max(1.f,o.scale)/o.scale;}
struct LayoutGeometry {std::vector<Point> tiles;float width=0,height=0,tileWidth=0,tileHeight=0;};
inline LayoutGeometry groupGeometry(const Options& o) {
    LayoutGeometry g;const auto count=std::clamp(o.groupCount,std::size_t(1),Arsenal::count);
    const float text=hudTextFactor(o);
    switch(o.shape) {
    case 1:g.tileWidth=o.showLabels?76*std::max(1.f,text):68;g.tileHeight=o.showLabels?71+12*text+4:70;break;
    case 2:g.tileWidth=o.showLabels?82*std::max(1.f,text):82;g.tileHeight=o.showLabels?100+12*text+4:100;break;
    case 3:g.tileWidth=o.showLabels?88*std::max(1.f,text):88;g.tileHeight=o.showLabels?88+12*text+4:88;break;
    default:g.tileWidth=o.showCardNames||o.showLabels?59+97*std::max(1.f,text):64;g.tileHeight=std::max(56.f,(o.showCardNames&&o.showLabels?26:14)*text+20);break;
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
    float width=420,height=155,centerY=16,ruleY=67,nameY=76,statusY=104,pagerY=132,hintY=154,listPitch=0,listHeight=46,ringRadiusX=140,ringRadiusY=74;
};
inline SelectorGeometry selectorGeometry(const Options& o) {
    const float text=hudTextFactor(o);
    SelectorGeometry s;s.width=std::max(420.f,328+o.selectorSpacing*4);
    if(o.selector==2) {
        const float extra=std::max(0.f,text-1)*30;
        s.ringRadiusX=(140+o.selectorSpacing*1.25f)*std::max(1.f,text);
        s.centerY=78+o.selectorSpacing*.5f+extra;s.ringRadiusY=74+o.selectorSpacing*.5f+extra;
        s.nameY=s.centerY-21*text;s.statusY=s.nameY+27*text;s.ruleY=s.statusY+19*text;
        s.pagerY=s.centerY+s.ringRadiusY+39;
    } else if(o.selector==3) {
        s.width=320;s.centerY=113+o.selectorSpacing*2;s.ruleY=263+o.selectorSpacing*4;
        s.nameY=s.ruleY+9;s.statusY=s.ruleY+37;s.pagerY=s.ruleY+65;
    } else if(o.selector==4) {
        s.width=320;s.listHeight=std::max(46.f,16*text+14);s.listPitch=s.listHeight+2+o.selectorSpacing*.5f;s.centerY=s.listPitch*2-5;
        s.ruleY=s.centerY+s.listPitch*2+s.listHeight*.5f+12;
    }
    s.width*=std::max(1.f,text);
    if(o.selector!=2){s.nameY=s.ruleY+9;s.statusY=s.nameY+28*text;s.pagerY=s.statusY+13*text+15;}
    s.hintY=s.pagerY+8+12*text+10;
    s.height=o.showHints?s.hintY+13*text+4:s.pagerY+8+12*text+4;return s;
}
struct EquipmentGeometry {
    Point origin,icon,text;
    std::array<Point,5> cells{};
    float width=340,height=184,rowHeight=44,scale=1,cellWidth=340,cellHeight=44,iconSize=30,textWidth=281;
    bool centered=false;
};
inline EquipmentGeometry equipmentGeometry(const Options& o,float screenX,float screenY,unsigned rows=4) {
    EquipmentGeometry g;rows=std::clamp(rows,1u,5u);
    const float t=o.textScale*o.equipmentTextScale,f=std::max(1.f,t),textHeight=36*t;
    float fitWidth=0,fitHeight=0;
    if(o.equipmentStyle==0) {
        g.cellWidth=340*f;g.cellHeight=44*f;
        g.icon={23,g.cellHeight*.5f};g.text={47,g.cellHeight*.5f-18*t};g.textWidth=g.cellWidth-59;

        const float pitch=g.cellHeight+(o.equipmentSpacing-1)*4*f;
        g.width=g.cellWidth;g.height=g.cellHeight+float(rows-1)*pitch+8;
        for(unsigned i=0;i<rows;++i)g.cells[i]={0,4+float(i)*pitch};
        fitWidth=g.width;fitHeight=g.cellHeight+float(rows-1)*(g.cellHeight+12*f)+8;
    } else {
        if(o.equipmentStyle==4) {
            g.cellWidth=280*f;g.cellHeight=textHeight+8;g.iconSize=0;g.text={8,4};g.textWidth=g.cellWidth-16;
        } else {
            g.iconSize=o.equipmentStyle==2?32.f:38.f;
            if(o.equipmentTextPosition<2) {
                g.cellWidth=180*f;g.cellHeight=80+textHeight;g.centered=true;g.textWidth=g.cellWidth-16;
                g.text={g.cellWidth*.5f,o.equipmentTextPosition==0?4.f:76.f};
                g.icon={g.cellWidth*.5f,o.equipmentTextPosition==0?44+textHeight:36.f};
            } else {
                g.cellWidth=300*f;g.cellHeight=std::max(64.f,textHeight)+8;g.textWidth=g.cellWidth-84;
                const bool right=o.equipmentTextPosition==3;
                g.icon={right?36.f:g.cellWidth-36,g.cellHeight*.5f};
                g.text={right?76.f:8.f,(g.cellHeight-textHeight)*.5f};
            }
        }
        const bool horizontal=o.equipmentLayout==1;
        const float gap=(horizontal?12.f:8.f)*o.equipmentSpacing;
        g.width=8+g.cellWidth*(horizontal?rows:1)+(horizontal?rows-1:0)*gap;
        g.height=8+g.cellHeight*(horizontal?1:rows)+(horizontal?0:rows-1)*gap;
        for(unsigned i=0;i<rows;++i)g.cells[i]={4+(horizontal?float(i)*(g.cellWidth+gap):0),4+(horizontal?0:float(i)*(g.cellHeight+gap))};
        const float maxGap=(horizontal?12.f:8.f)*4;
        fitWidth=8+g.cellWidth*(horizontal?rows:1)+(horizontal?rows-1:0)*maxGap;
        fitHeight=8+g.cellHeight*(horizontal?1:rows)+(horizontal?0:rows-1)*maxGap;
    }
    g.rowHeight=g.cellHeight;

    g.scale=std::max(.05f,std::min({o.equipmentScale*std::clamp(screenY/1080.f,.75f,3.f),(screenX-16)/fitWidth,(screenY-16)/fitHeight}));
    g.origin={8+std::max(0.f,screenX-16-g.width*g.scale)*o.equipmentX,8+std::max(0.f,screenY-16-g.height*g.scale)*o.equipmentY};
    return g;
}
struct HudGeometry {LayoutGeometry groups;EquipmentGeometry equipment;Point origin,groupOrigin,selectorOrigin;float scale=1,width=0,height=0,selectorHeight=150,selectorWidth=420,captionHeight=40;};
inline HudGeometry hudGeometry(const Options& o,float screenX,float screenY,unsigned rows=4) {
    HudGeometry g;g.equipment=equipmentGeometry(o,screenX,screenY,rows);g.groups=groupGeometry(o);g.width=g.groups.width;g.captionHeight=15*hudTextFactor(o)+8;
    g.groupOrigin={0,o.shape==0?3.f:0.f};g.height=g.groups.height+g.groupOrigin.y;
    const auto selectorLayout=selectorGeometry(o);g.selectorHeight=selectorLayout.height;g.selectorWidth=selectorLayout.width;
    const float sh=g.selectorHeight+32,resolutionScale=std::clamp(screenY/1080.f,.75f,3.f);
    auto widest=o;widest.spacing=40;widest.selectorSpacing=32;
    const auto groupLimit=groupGeometry(widest);const auto selectorLimit=selectorGeometry(widest);
    g.scale=std::max(.05f,std::min({o.scale*resolutionScale,(screenX-16)/std::max(groupLimit.width,selectorLimit.width),
        (screenY-16)/std::max(groupLimit.height+g.groupOrigin.y,selectorLimit.height+32)}));
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
inline float placementGridStep(const Options& o,float screenHeight) {
    const float size=o.placementSpacing==0?8:o.placementSpacing==1?16:o.placementSpacing==2?32:o.placementGridSize;
    return size*std::max(.5f,screenHeight/1080.f);
}
inline Bounds placementFrame(const Options& o,float width,float height) {
    if(!o.placementFrame)return {{0,0},{width,height},true};
    const float aspect=o.placementFrame==1?16.f/9:o.placementFrame==2?2.f:21.f/9;
    const float w=std::min(width,height*aspect),h=std::min(height,width/aspect);
    return {{(width-w)*.5f,(height-h)*.5f},{(width+w)*.5f,(height+h)*.5f},true};
}
struct SnapAxis {float value=0,guide=0;bool aligned=false;};

inline SnapAxis snapPlacementAxis(float value,float extent,float screen,float lowOffset,float highOffset,
                                  float frameLow,float frameHigh,float step,std::optional<std::array<float,2>> other={}) {
    const float travel=screen-16-extent;
    if(value<=0||value>=1||travel<=0||step<=0)return {std::clamp(value,0.f,1.f)};
    const float start=8+value*travel,center=(frameLow+frameHigh)*.5f;
    float delta=std::numeric_limits<float>::max(),guide=0;
    const std::array anchors{start+lowOffset,start+(lowOffset+highOffset)*.5f,start+highOffset};
    const auto consider=[&](float anchor,float target){if(std::abs(target-anchor)<std::abs(delta)){delta=target-anchor;guide=target;}};
    for(float anchor:anchors){consider(anchor,center+std::round((anchor-center)/step)*step);for(float edge:{frameLow,center,frameHigh})consider(anchor,edge);}

    if(other){float distance=std::min(6.f,step*.33f);for(float anchor:anchors)for(float target:{(*other)[0],((*other)[0]+(*other)[1])*.5f,(*other)[1]})
        if(std::abs(target-anchor)<distance){distance=std::abs(target-anchor);delta=target-anchor;guide=target;}}
    const float snapped=value+delta/travel,clamped=std::clamp(snapped,0.f,1.f);
    return {clamped,guide,snapped==clamped};
}
inline HudGeometry previewGeometry(const Options& o,float screenX,float screenY,float width,float height,bool selector,bool groups=true,bool equipment=false,unsigned rows=4) {
    const auto arrange=[&](const Options& look) {
        auto g=hudGeometry(look,screenX,screenY,rows);g.origin={};g.equipment.origin={};
        if(selector&&groups&&!equipment) {
            const float w=g.width,h=g.height,sw=g.selectorWidth,sh=g.selectorHeight+32,gap=look.selectorGap;
            const std::array<Point,4> docks{{{(w-sw)*.5f,-gap-sh},{(w-sw)*.5f,h+gap},
                {-gap-sw,(h-sh)*.5f},{w+gap,(h-sh)*.5f}}};

            g.selectorOrigin=docks[look.selectorDock<4?look.selectorDock:0];g.selectorOrigin.y+=32;
        }
        return g;
    };
    const auto bounds=[&](const HudGeometry& g) {
        if(equipment)return Bounds{g.equipment.origin,{g.equipment.origin.x+g.equipment.width*g.equipment.scale,
            g.equipment.origin.y+g.equipment.height*g.equipment.scale},true};
        Bounds b;
        if(groups){b.include(g.origin);b.include({g.origin.x+g.width*g.scale,g.origin.y+(g.height+(selector?0:g.captionHeight))*g.scale});}
        if(selector) {
            const Point p{g.origin.x+g.selectorOrigin.x*g.scale,g.origin.y+(g.selectorOrigin.y-32)*g.scale};
            b.include(p);b.include({p.x+g.selectorWidth*g.scale,p.y+(g.selectorHeight+40)*g.scale});
        }
        return b;
    };
    auto g=arrange(o);const auto actual=bounds(g);
    auto widest=o;widest.spacing=40;widest.selectorSpacing=32;widest.equipmentSpacing=4;
    const auto spaced=bounds(arrange(widest));
    widest.scale=1.5f;widest.equipmentScale=2;
    const auto largest=bounds(arrange(widest));
    const float margin=std::min(width,height)*.06f;

    const float factor=std::max(.05f,std::min((width-margin*2)/std::max({1.f,spaced.width(),largest.width()}),
        (height-margin*2)/std::max({1.f,spaced.height(),largest.height()})));
    const auto center=[&](Point p){return Point{(width-actual.width()*factor)*.5f+(p.x-actual.min.x)*factor,
        (height-actual.height()*factor)*.5f+(p.y-actual.min.y)*factor};};
    g.equipment.origin=center(g.equipment.origin);
    g.equipment.scale*=factor;
    g.origin=center(g.origin);
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

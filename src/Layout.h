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
// Shape codes match the HUD tile renderer: circle, rectangle, diamond.
inline float tileDistance(Point direction,float width,float height,unsigned shape,float gap) {
    const float length=std::hypot(direction.x,direction.y);
    const float x=std::abs(direction.x)/length,y=std::abs(direction.y)/length;
    if(shape==1)return width+gap;
    if(shape==3) {
        const float edge=(width+gap*std::sqrt(2.f))/(x+y);
        if(std::abs(x-y)*edge<=width)return edge;
        const float major=std::max(x,y),minor=std::min(x,y);
        return major*width+std::sqrt(std::max(0.f,gap*gap-minor*minor*width*width));
    }
    if(x>0) {const float edge=(width+gap)/x;if(edge*y<=height)return edge;}
    if(y>0) {const float edge=(height+gap)/y;if(edge*x<=width)return edge;}
    const float projection=x*width+y*height;
    return projection+std::sqrt(std::max(0.f,projection*projection-width*width-height*height+gap*gap));
}
struct Arrangement {std::vector<Point> centers;float width=0,height=0;};
inline Arrangement arrangeTiles(const std::vector<Point>& sizes,unsigned layout,float gap,unsigned shape=2,float rotation=0) {
    Arrangement a;if(sizes.empty())return a;
    const auto count=sizes.size();float width=0,height=0;
    for(const auto size:sizes){width=std::max(width,size.x);height=std::max(height,size.y);}
    constexpr float pi=3.141592654f;
    constexpr std::array<Point,8> diamond{{{1,0},{2,1},{1,2},{0,1},{0,-1},{2,-1},{0,3},{2,3}}};
    constexpr std::array<Point,8> cross{{{1,1},{0,1},{2,1},{1,0},{1,2},{0,0},{2,0},{0,2}}};
    constexpr std::array<Point,8> staggered{{{1,0},{2,.5f},{1,1},{0,.5f},{0,-.5f},{2,-.5f},{0,1.5f},{2,1.5f}}};
    float cursor=0;
    for(std::size_t i=0;i<count;++i) {
        Point p;
        if(layout<2) {
            p=layout==0?Point{cursor+sizes[i].x*.5f,height*.5f}:Point{width*.5f,cursor+sizes[i].y*.5f};
            cursor+=(layout==0?sizes[i].x:sizes[i].y)+gap;
        } else if(layout==4) {
            const float angle=-pi*.5f+float(i)*2*pi/float(count)+rotation;
            p={std::cos(angle)*width,std::sin(angle)*height};
        } else {
            const auto slot=(layout==2?diamond:layout==3?cross:staggered)[i];
            const float pad=layout==2&&shape==2?gap/std::sqrt(2.f):layout==2?0.f:gap;
            p={slot.x*(width+pad),slot.y*(height+pad)};
        }
        a.centers.push_back(p);
    }
    if((layout==4||(layout==2&&shape!=2)||(layout>=2&&count==2))&&count>1) {
        float factor=0;
        for(std::size_t i=0;i<count;++i)for(std::size_t j=0;j<i;++j) {
            const Point delta{a.centers[i].x-a.centers[j].x,a.centers[i].y-a.centers[j].y};
            factor=std::max(factor,tileDistance(delta,(sizes[i].x+sizes[j].x)*.5f,(sizes[i].y+sizes[j].y)*.5f,shape,gap)/std::hypot(delta.x,delta.y));
        }
        for(auto& p:a.centers){p.x*=factor;p.y*=factor;}
    }
    Bounds bounds;
    for(std::size_t i=0;i<count;++i){const auto p=a.centers[i],size=sizes[i];bounds.include({p.x-size.x*.5f,p.y-size.y*.5f});bounds.include({p.x+size.x*.5f,p.y+size.y*.5f});}
    for(auto& p:a.centers){p.x-=bounds.min.x;p.y-=bounds.min.y;}
    a.width=bounds.width();a.height=bounds.height();return a;
}
inline LayoutGeometry groupGeometry(const Options& o) {
    LayoutGeometry g;const auto count=std::clamp(o.groupCount,std::size_t(1),Arsenal::count);
    const float text=hudTextFactor(o);Point footprint;unsigned shape=2;
    switch(o.shape) {
    case 1:g.tileWidth=o.showLabels?76*std::max(1.f,text):68;g.tileHeight=o.showLabels?71+12*text+4:70;footprint={58,58};shape=1;break;
    case 2:g.tileWidth=o.showLabels?82*std::max(1.f,text):82;g.tileHeight=o.showLabels?100+12*text+4:100;footprint={82,100};break;
    case 3:g.tileWidth=o.showLabels?88*std::max(1.f,text):88;g.tileHeight=o.showLabels?88+12*text+4:88;footprint={84,84};shape=3;break;
    default:g.tileWidth=o.showCardNames||o.showLabels?59+97*std::max(1.f,text):64;g.tileHeight=std::max(56.f,(o.showCardNames&&o.showLabels?26:14)*text+20);footprint={g.tileWidth,g.tileHeight-7};break;
    }
    if(o.showLabels&&o.shape){footprint={g.tileWidth,g.tileHeight};shape=2;}
    const auto arranged=arrangeTiles(std::vector<Point>(count,footprint),o.layout,o.spacing,shape);
    for(const auto p:arranged.centers)g.tiles.push_back({std::max(0.f,p.x-footprint.x*.5f),std::max(0.f,p.y-footprint.y*.5f)});
    g.width=arranged.width+g.tileWidth-footprint.x;g.height=arranged.height+g.tileHeight-footprint.y;
    return g;
}
inline unsigned selectorTileShape(const Options& o){return o.selectorShape?o.selectorShape:o.selector==0?2u:1u;}
inline float selectorRadius(float distance){return distance<1?40-12*distance:std::max(18.f,28-7*(distance-1));}
inline float selectorStep(float radii,float rise,unsigned shape,float gap) {
    if(shape==2)return radii+gap;
    if(shape==1)return std::sqrt(std::max(0.f,(radii+gap)*(radii+gap)-rise*rise));
    return rise>=gap/std::sqrt(2.f)?radii-rise+gap*std::sqrt(2.f):radii+std::sqrt(std::max(0.f,gap*gap-rise*rise));
}
inline Point selectorOffset(const Options& o,float position) {
    const int whole=int(std::floor(position));const float fraction=position-float(whole),rise=o.selector==1?13.f:0.f;
    const auto between=[&](float a,float b){return selectorStep(selectorRadius(std::abs(a))+selectorRadius(std::abs(b)),
        std::abs(std::abs(a)-std::abs(b))*rise,selectorTileShape(o),o.selectorSpacing);};
    const float central=between(fraction-1,fraction);
    float shift=whole<0?-(1-fraction)*central:fraction*central;
    if(whole>=0)for(int i=0;i<whole;++i)shift+=between(fraction+i,fraction+i+1);
    else for(int i=-1;i>whole;--i)shift-=between(fraction+i,fraction+i-1);
    return o.selector==3?Point{0,shift}:Point{shift,std::abs(position)*rise};
}
struct SelectorGeometry {
    float width=420,height=155,centerY=16,ruleY=67,nameY=76,statusY=104,pagerY=132,hintY=154,listPitch=0,listHeight=46;
    std::vector<Point> ring;
};
inline SelectorGeometry selectorGeometry(const Options& o,std::size_t count=5,float phase=0) {
    const float text=hudTextFactor(o);SelectorGeometry s;
    const float side=count>3?2:count>1?1:0;
    if(o.selector==2) {
        const auto visible=std::clamp(count,std::size_t(1),std::size_t(8));
        std::vector<Point> sizes(visible,{44,44});sizes[0]={58,58};
        constexpr float pi=3.141592654f;const auto shape=selectorTileShape(o);
        const auto ring=arrangeTiles(sizes,4,o.selectorSpacing,shape,phase*2*pi/float(visible));
        const float radius=visible>1?(51*(shape==2?std::sqrt(2.f):1.f)+o.selectorSpacing)/(2*std::sin(pi/float(visible))):0;
        const float extent=2*(radius+29);
        s.width=std::max(260*std::max(1.f,text),extent+24);s.ring=ring.centers;
        Point pivot;for(const auto p:s.ring){pivot.x+=p.x/float(visible);pivot.y+=p.y/float(visible);}
        for(auto& p:s.ring){p.x+=s.width*.5f-pivot.x;p.y+=extent*.5f-24-pivot.y;}
        s.ruleY=extent-24+18;
    } else if(o.selector==3) {
        const float reach=selectorOffset(o,side).y+selectorRadius(side);
        s.width=std::max(240.f,240*text);s.centerY=reach;s.ruleY=reach*2+18;
    } else if(o.selector==4) {
        s.width=320*std::max(1.f,text);s.listHeight=std::max(46.f,16*text+14);s.listPitch=s.listHeight+o.selectorSpacing;s.centerY=s.listPitch*side;
        s.ruleY=s.centerY+s.listPitch*side+s.listHeight*.5f+16;
    } else {
        const auto last=selectorOffset(o,side);
        s.width=std::max(240*std::max(1.f,text),(last.x+selectorRadius(side))*2+48);
        s.ruleY=s.centerY+std::max(40.f,last.y+selectorRadius(side))+16;
    }
    s.nameY=s.ruleY+9;s.statusY=s.nameY+28*text;s.pagerY=s.statusY+13*text+15;
    s.hintY=s.pagerY+8+12*text+10;
    s.height=o.showHints?s.hintY+13*text+4:s.pagerY+8+12*text+4;return s;
}
struct EquipmentMetrics {std::array<float,5> textWidths{};bool measured=false;};
struct EquipmentCell {Point origin,icon,text;float width=0,height=0,textWidth=0;bool centered=false;};
struct EquipmentGeometry {
    Point origin;std::array<EquipmentCell,5> cells{};
    float width=0,height=0,scale=1,iconSize=30;
};
inline EquipmentGeometry equipmentGeometry(const Options& o,float screenX,float screenY,unsigned rows=4,const EquipmentMetrics& metrics={}) {
    EquipmentGeometry g;rows=std::clamp(rows,1u,5u);
    const float t=o.textScale*o.equipmentTextScale,textHeight=36*t;
    const bool labels=o.equipmentShowText||o.equipmentStyle==0||o.equipmentStyle==4;
    g.iconSize=o.equipmentStyle==0?30.f:o.equipmentStyle==4?0.f:o.equipmentStyle==2?32.f:38.f;
    std::vector<Point> sizes;
    for(unsigned i=0;i<rows;++i) {
        auto& cell=g.cells[i];cell.textWidth=labels?std::clamp(metrics.measured?metrics.textWidths[i]:140*t,0.f,220*t):0;
        const float frame=o.equipmentStyle==0?30.f:o.equipmentStyle==4?0.f:56.f;
        if(!labels) {cell.width=cell.height=frame;cell.icon={frame*.5f,frame*.5f};}
        else if(o.equipmentStyle==4){cell.width=cell.textWidth;cell.height=textHeight;cell.text={};}
        else if(o.equipmentStyle>0&&o.equipmentTextPosition<2) {
            cell.width=std::max(frame,cell.textWidth);cell.height=frame+6+textHeight;cell.centered=true;
            cell.icon={cell.width*.5f,o.equipmentTextPosition==0?textHeight+6+frame*.5f:frame*.5f};
            cell.text={cell.width*.5f,o.equipmentTextPosition==0?0.f:frame+6};
        } else {
            cell.width=frame+8+cell.textWidth;cell.height=std::max(frame,textHeight);
            const bool right=o.equipmentStyle==0||o.equipmentTextPosition==3;
            cell.icon={right?frame*.5f:cell.width-frame*.5f,cell.height*.5f};
            cell.text={right?frame+8:0.f,(cell.height-textHeight)*.5f};
        }
        sizes.push_back({cell.width,cell.height});
    }
    if(o.equipmentLayout==1) {
        float width=0;for(const auto size:sizes)width=std::max(width,size.x);
        for(unsigned i=0;i<rows;++i) {
            auto& cell=g.cells[i];cell.width=sizes[i].x=width;
            if(cell.centered)cell.icon.x=cell.text.x=width*.5f;
            else if(o.equipmentStyle>0&&o.equipmentStyle<4&&labels&&o.equipmentTextPosition==2) {
                cell.icon.x=width-28;cell.text.x=width-64-cell.textWidth;
            }
        }
    }
    const auto shape=!labels?(o.equipmentStyle==1?1u:o.equipmentStyle==2?3u:2u):2u;
    const auto arranged=arrangeTiles(sizes,o.equipmentLayout,o.equipmentSpacing,shape);
    g.width=arranged.width+8;g.height=arranged.height+8;
    for(unsigned i=0;i<rows;++i)g.cells[i].origin={4+arranged.centers[i].x-sizes[i].x*.5f,4+arranged.centers[i].y-sizes[i].y*.5f};
    g.scale=std::max(.05f,std::min({o.equipmentScale*std::clamp(screenY/1080.f,.75f,3.f),(screenX-16)/g.width,(screenY-16)/g.height}));
    g.origin={8+std::max(0.f,screenX-16-g.width*g.scale)*o.equipmentX,8+std::max(0.f,screenY-16-g.height*g.scale)*o.equipmentY};
    return g;
}
struct HudGeometry {LayoutGeometry groups;EquipmentGeometry equipment;Point origin,groupOrigin,selectorOrigin;float scale=1,width=0,height=0,selectorHeight=150,selectorWidth=420,captionHeight=40,previewZoom=1;};
inline HudGeometry hudGeometry(const Options& o,float screenX,float screenY,unsigned rows=4,const EquipmentMetrics& metrics={},std::size_t selectorCount=5) {
    HudGeometry g;g.equipment=equipmentGeometry(o,screenX,screenY,rows,metrics);g.groups=groupGeometry(o);g.width=g.groups.width;g.captionHeight=15*hudTextFactor(o)+8;
    g.groupOrigin={0,o.shape==0?3.f:0.f};g.height=g.groups.height+g.groupOrigin.y;
    const auto selectorLayout=selectorGeometry(o,selectorCount);g.selectorHeight=selectorLayout.height;g.selectorWidth=selectorLayout.width;
    const float sh=g.selectorHeight+32,resolutionScale=std::clamp(screenY/1080.f,.75f,3.f);
    g.scale=std::max(.05f,std::min({o.scale*resolutionScale,(screenX-16)/std::max(g.width,g.selectorWidth),
        (screenY-16)/std::max(g.height,g.selectorHeight+32)}));
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
inline HudGeometry previewGeometry(const Options& o,float screenX,float screenY,float width,float height,bool selector,bool groups=true,bool equipment=false,unsigned rows=4,const EquipmentMetrics& metrics={},std::size_t selectorCount=5,float zoom=0) {
    const auto arrange=[&](const Options& look) {
        auto g=hudGeometry(look,screenX,screenY,rows,metrics,selectorCount);g.origin={};g.equipment.origin={};
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
    const float margin=std::min(width,height)*.06f;
    const float factor=zoom>0?zoom:std::min((width-margin*2)/std::max(1.f,actual.width()),(height-margin*2)/std::max(1.f,actual.height()));
    g.previewZoom=factor;
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
inline void equipmentLayoutPreset(Options& o,unsigned preset) {
    o.equipmentShowText=false;o.equipmentSpacing=8;
    if(preset==0){o.equipmentLayout=5;o.equipmentStyle=3;}
    else if(preset==1){o.equipmentLayout=2;o.equipmentStyle=2;}
    else {o.equipmentLayout=4;o.equipmentStyle=1;}
}
}

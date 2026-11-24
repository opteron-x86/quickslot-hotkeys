#pragma once
#include "Model.h"
#include <limits>
#include <span>

namespace quickslot {
struct Point {float x=0,y=0;};
struct Bounds {
    Point min{},max{};bool valid=false;
    float width()const{return max.x-min.x;}
    float height()const{return max.y-min.y;}
    void include(Point p){if(!valid){min=max=p;valid=true;}else{min.x=std::min(min.x,p.x);min.y=std::min(min.y,p.y);max.x=std::max(max.x,p.x);max.y=std::max(max.y,p.y);}}
};
struct HudBounds {Bounds widget,selector,equipment;std::vector<Bounds> tiles,equipmentCells;};

inline float hudTextFactor(const Options& o){return o.textScale;}
inline float selectorTextFactor(const Options& o){return o.selectorTextScale;}
// The room a card keeps at its outer end for its key or button label.
inline constexpr float cardKeyColumn=26;
// tileScales and tileSizes size each tile in the Free arrangement; elsewhere they are empty and every tile shares scale 1 and tileWidth x tileHeight.
struct LayoutGeometry {std::vector<Point> tiles;std::vector<float> tileScales;std::vector<Point> tileSizes;float width=0,height=0,tileWidth=0,tileHeight=0;};
inline float tileScale(const LayoutGeometry& g,std::size_t slot){return slot<g.tileScales.size()?g.tileScales[slot]:1.f;}
inline Point tileSizeFor(const LayoutGeometry& g,std::size_t slot){return slot<g.tileSizes.size()?g.tileSizes[slot]:Point{g.tileWidth,g.tileHeight};}
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
// Layouts: 0 horizontal, 1 vertical, 2 diamond, 3 cross, 4 circle, 5 staggered cross, 6 grid filled row by row.
inline Arrangement arrangeTiles(const std::vector<Point>& sizes,unsigned layout,float gap,unsigned shape=2,float rotation=0,unsigned columns=4) {
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
        } else if(layout==6) {
            const auto wide=std::max(1u,columns);p={float(i%wide)*(width+gap),float(i/wide)*(height+gap)};
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
    if((layout==4||(layout==2&&shape!=2)||(layout>=2&&layout!=6&&count==2))&&count>1) {
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
inline bool unifiedHotbar(const Options& o){return o.unifiedBar&&(o.layout==0||o.layout==1||o.layout==6);}
struct TileMetrics {float tileWidth=0,tileHeight=0;Point footprint{};unsigned shape=2;};
inline TileMetrics tileMetricsFor(const Options& o,unsigned shapeChoice,bool showLabels,bool showKeys) {
    const float text=hudTextFactor(o),size=o.tileSize;TileMetrics m;
    const bool outsideLabel=showLabels&&o.labelPosition!=2;
    switch(shapeChoice) {
    // Circle, square, diamond, hexagon and frameless tiles scale with tileSize; external names keep their own text size.
    case 1:m.tileWidth=std::max(68*size,outsideLabel?76*std::max(1.f,text):0.f);m.tileHeight=outsideLabel?71*size+12*text+4:66*size;m.footprint={58*size,58*size};m.shape=1;break;
    case 2:m.tileWidth=std::max(78*size,outsideLabel?78*std::max(1.f,text):0.f);m.tileHeight=outsideLabel?78*size+12*text+4:78*size;m.footprint={78*size,78*size};m.shape=2;break;
    case 3:m.tileWidth=std::max(88*size,outsideLabel?88*std::max(1.f,text):0.f);m.tileHeight=outsideLabel?88*size+12*text+4:88*size;m.footprint={84*size,84*size};m.shape=3;break;
    case 4:m.tileWidth=std::max(76*size,outsideLabel?78*std::max(1.f,text):0.f);m.tileHeight=outsideLabel?84*size+12*text+4:84*size;m.footprint={72*size,82*size};m.shape=1;break;
    case 5:m.tileWidth=std::max(64*size,outsideLabel?72*std::max(1.f,text):0.f);m.tileHeight=outsideLabel?64*size+12*text+4:64*size;m.footprint={60*size,60*size};m.shape=1;break;
    default:m.tileWidth=(o.showCardNames||showLabels?59+o.cardWidth*std::max(1.f,text):56)+(showKeys?cardKeyColumn:0);m.tileHeight=o.showCardNames&&showLabels?std::max(54.f,26*text+20):std::max(48.f,14*text+20);m.footprint={m.tileWidth,m.tileHeight-4};m.shape=2;break;
    }
    if(outsideLabel&&shapeChoice){m.footprint={m.tileWidth,m.tileHeight};m.shape=2;}
    return m;
}
inline LayoutGeometry groupGeometry(const Options& o) {
    LayoutGeometry g;const auto count=std::clamp(o.groupCount,std::size_t(1),Arsenal::count);
    const auto base=tileMetricsFor(o,o.shape,o.showLabels,o.showKeys);
    g.tileWidth=base.tileWidth;g.tileHeight=base.tileHeight;
    if(o.layout==freeLayout) {
        for(std::size_t slot=0;slot<count;++slot) {
            const auto id=o.groupOrder[slot];
            const auto m=tileMetricsFor(o,effectiveTileShape(o,id),effectiveTileShowLabel(o,id),effectiveTileShowKey(o,id));
            g.tileSizes.push_back({m.tileWidth,m.tileHeight});
        }
    }
    const float gap=unifiedHotbar(o)?0.f:o.spacing;
    const auto arranged=arrangeTiles(std::vector<Point>(count,base.footprint),o.layout==freeLayout?0u:o.layout,gap,base.shape,0,unsigned(std::clamp(o.gridColumns,1.f,8.f)));
    for(const auto p:arranged.centers)g.tiles.push_back({std::max(0.f,p.x-base.footprint.x*.5f),std::max(0.f,p.y-base.footprint.y*.5f)});
    g.width=arranged.width+g.tileWidth-base.footprint.x;g.height=arranged.height+g.tileHeight-base.footprint.y;
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
    float width=420,height=155,centerY=16,ruleY=67,nameY=76,statusY=104,descriptionY=0,pagerY=132,hintY=154,listPitch=0,listHeight=46,listHeaderHeight=0;
    unsigned listVisible=0;
    std::vector<Point> ring;
    // Wheel selectors: the center the slots surround and their distance from it. A segmented wheel is a ring
    // from inner to outer around a hub of hubRadius; the pointed segment reaches out by lift. rotation turns the
    // first slot clockwise from the top, in radians. nameInHub puts the pointed item's name in the hub; otherwise
    // nameBelow puts it under the wheel, and with neither the name is hidden.
    Point hub;float reach=0,inner=0,outer=0,wedgeGap=0,hubRadius=0,lift=0,rotation=0;bool segments=false,nameInHub=false,nameBelow=true;
};
inline SelectorGeometry selectorGeometry(const Options& o,std::size_t count=5,float phase=0) {
    const float text=selectorTextFactor(o);SelectorGeometry s;
    // Strips, arcs and lists show up to selectorVisible+1 neighbors on each side of the selected item.
    const float side=float(std::min<std::size_t>(o.selectorVisible+1,count/2));
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
    } else if(o.selector==wheelSelector) {
        // Every entry keeps a fixed slot around the hub, starting at the top and running clockwise.
        // A segmented wheel has a wedge for every entry; tiles stop at a full group.
        const auto slots=o.wheelStyle==1?std::max(count,std::size_t(1)):std::clamp(count,std::size_t(1),Group::limit);constexpr float pi=3.141592654f;
        const float footprint=selectorTileShape(o)==2?50*std::sqrt(2.f):50;
        s.segments=o.wheelStyle==1;s.rotation=(std::isfinite(o.wheelRotation)?o.wheelRotation:0)*pi/180;
        if(s.segments) {
            // The wheel's size is its outer radius and the ring takes a share of it, so the ring, its icons and the hub
            // grow together. A thickness of 100% fills the wheel to the center.
            // Icons sit in the middle of the ring; in a wheel filled near the center they move out to where segments are
            // wider. A wheel too small for its entries grows until each wedge has room for an icon.
            const float share=std::clamp(o.wheelThickness,.05f,1.f),depth=share<1/1.3f?1-share*.5f:1-share*.42f;
            s.wedgeGap=std::max(0.f,o.wheelGap);
            const float roomy=slots>1?((slots>Group::limit?26.f:40.f)+s.wedgeGap)/(2*std::sin(pi/float(slots)))/depth:0;
            s.outer=std::max(std::isfinite(o.wheelSize)?o.wheelSize:130.f,roomy);
            const float thickness=s.outer*share;s.inner=std::max(0.f,s.outer-thickness);s.reach=s.outer*depth;
            s.hubRadius=std::max(0.f,s.inner-std::max(6.f,s.wedgeGap+3));
            s.lift=o.wheelLift?std::max(4.f,thickness*.1f):0;
            // A name needs a hub wide enough for a line of text; a smaller hub sends it under the wheel.
            s.nameInHub=o.wheelName==0&&s.hubRadius>=34*std::max(1.f,text);
            s.nameBelow=o.wheelName==1||(o.wheelName==0&&!s.nameInHub);
        } else s.reach=slots>1?std::max(o.wheelSize-31,(footprint+o.selectorSpacing)/(2*std::sin(pi/float(slots)))):0;
        const float extent=2*(s.segments?s.outer+s.lift+7:s.reach+31);
        s.width=std::max(260*std::max(1.f,text),extent+24);s.hub={s.width*.5f,extent*.5f-24};
        for(std::size_t i=0;i<slots;++i){const float angle=-pi*.5f+s.rotation+float(i)*2*pi/float(slots);s.ring.push_back({s.hub.x+std::cos(angle)*s.reach,s.hub.y+std::sin(angle)*s.reach});}
        s.ruleY=extent-24+18;
    } else if(o.selector==3) {
        const float reach=selectorOffset(o,side).y+selectorRadius(side);
        s.width=std::max(240.f,240*text);s.centerY=reach;s.ruleY=reach*2+18;
    } else if(o.selector==4) {
        // Compact QuickLoot/SkyUI-style panel: header bar + tight scrollable rows, no redundant repeated title underneath.
        const unsigned maxRows=std::clamp(o.selectorVisible*2u+3u,3u,9u);
        s.listVisible=std::max(1u,std::min(maxRows,unsigned(std::max<std::size_t>(count,1))));
        s.listHeaderHeight=o.selectorFooter?std::max(28.f,14*text+14):0.f;
        s.width=std::max(268.f,268*text);s.listHeight=std::max(32.f,15*text+14);s.listPitch=s.listHeight;
        s.centerY=s.listHeaderHeight+4.f;
        s.ruleY=s.centerY+s.listPitch*float(s.listVisible)+6.f;
    } else {
        const auto last=selectorOffset(o,side);
        s.width=std::max(240*std::max(1.f,text),(last.x+selectorRadius(side))*2+48);
        s.ruleY=s.centerY+std::max(40.f,last.y+selectorRadius(side))+16;
    }
    // Under the selector: the name and its status line (unless the wheel holds them in its hub, hides them, or the compact list displays them inline),
    // the footer with the group and position, then the hints.
    const bool listMode=o.selector==4;
    const bool name=!listMode&&(!s.segments||s.nameBelow);
    const bool footer=!listMode&&o.selectorFooter;
    // With charge shown, a row under the status holds the enchantment charge.
    s.nameY=s.ruleY+9;s.statusY=s.nameY+28*text;const float charge=o.showCharge?14*text*1.1f:0;
    // Two lines describe a potion, food, poison or scroll.
    const float afterName=name?s.statusY+14*text+charge+9:listMode?s.ruleY+(o.showCharge?charge+4:0):s.ruleY-8;
    s.descriptionY=afterName-(name?4:0);
    const float afterDescription=o.showDescriptions?s.descriptionY+2*14*text+8:afterName;
    s.pagerY=afterDescription;
    const float afterFooter=footer?s.pagerY+13*text+10:afterDescription;
    s.hintY=afterFooter;
    s.height=o.showHints?s.hintY+13*text+4:footer?s.pagerY+13*text+6:o.showDescriptions?afterDescription-3:name?s.statusY+14*text+charge+6:listMode?afterName+2:s.ruleY-2;return s;
}
// Placement. An anchored element rests a point of itself on the matching point of the screen area, moved by an
// offset in 1080p pixels, so it grows and shrinks away from that point. An unanchored element's position is a
// proportion of the free screen space. Both keep the element at least 8 pixels inside the screen.
constexpr unsigned unanchored=9;
inline float resolutionScale(float screenY){return std::clamp(screenY/1080.f,.75f,3.f);}
inline Bounds aspectFrame(unsigned choice,float width,float height) {
    if(!choice)return {{0,0},{width,height},true};
    const float aspect=choice==1?16.f/9:choice==2?2.f:21.f/9;
    const float w=std::min(width,height*aspect),h=std::min(height,width/aspect);
    return {{(width-w)*.5f,(height-h)*.5f},{(width+w)*.5f,(height+h)*.5f},true};
}
inline Bounds placementFrame(const Options& o,float width,float height){return aspectFrame(o.placementFrame,width,height);}
inline Bounds anchorArea(const Options& o,float width,float height){return aspectFrame(o.anchorFrame,width,height);}
inline Point anchorPoint(unsigned anchor){return {float(anchor%3)*.5f,float(anchor/3)*.5f};}
inline Point placedCorner(unsigned anchor,float x,float y,float proportionX,float proportionY,Point size,const Options& o,float screenX,float screenY) {
    if(anchor>=unanchored)return {8+std::max(0.f,screenX-16-size.x)*proportionX,8+std::max(0.f,screenY-16-size.y)*proportionY};
    const auto area=anchorArea(o,screenX,screenY);const auto at=anchorPoint(anchor);const float pixel=resolutionScale(screenY);
    return {std::clamp(area.min.x+at.x*(area.width()-size.x)+x*pixel,8.f,std::max(8.f,screenX-8-size.x)),
        std::clamp(area.min.y+at.y*(area.height()-size.y)+y*pixel,8.f,std::max(8.f,screenY-8-size.y))};
}
struct Anchored {unsigned anchor=unanchored;float x=0,y=0;};
// The anchor and offset that put an element's corner where it is; the nearest of the nine anchors unless one is given.
inline Anchored anchorFor(Point corner,Point size,const Options& o,float screenX,float screenY,std::optional<unsigned> anchor={}) {
    const auto area=anchorArea(o,screenX,screenY);const float pixel=resolutionScale(screenY);
    if(!anchor||*anchor>=unanchored) {
        const auto third=[](float center,float low,float span){const float f=span>0?(center-low)/span:.5f;return f<1.f/3?0u:f<2.f/3?1u:2u;};
        anchor=third(corner.x+size.x*.5f,area.min.x,area.width())+3*third(corner.y+size.y*.5f,area.min.y,area.height());
    }
    const auto at=anchorPoint(*anchor);
    return {*anchor,(corner.x-area.min.x-at.x*(area.width()-size.x))/pixel,(corner.y-area.min.y-at.y*(area.height()-size.y))/pixel};
}
struct EquipmentMetrics {std::array<float,6> textWidths{};bool measured=false;};
struct EquipmentCell {Point origin,icon,text;float width=0,height=0,textWidth=0;bool centered=false;};
inline constexpr std::array<EquipmentRole,6> defaultEquipmentRoles{
    EquipmentRole::rightHand,EquipmentRole::leftHand,EquipmentRole::consumable,EquipmentRole::voice,EquipmentRole::ammo,EquipmentRole::readySpell};
struct EquipmentGeometry {
    Point origin;std::array<EquipmentCell,6> cells{};
    std::array<EquipmentRole,6> roles=defaultEquipmentRoles;
    std::array<float,6> cellScales{1,1,1,1,1,1};
    unsigned rowCount=0;
    float width=0,height=0,scale=1,iconSize=30;
};
inline float equipmentCellScale(const EquipmentGeometry& g,std::size_t slot){return slot<g.cellScales.size()?g.cellScales[slot]:1.f;}
inline EquipmentGeometry equipmentGeometry(const Options& o,float screenX,float screenY,unsigned rows=4,const EquipmentMetrics& metrics={},
                                           std::array<EquipmentRole,6> roles=defaultEquipmentRoles) {
    EquipmentGeometry g;rows=std::clamp(rows,1u,6u);g.rowCount=rows;g.roles=roles;
    const float t=o.equipmentTextScale,textHeight=36*t;
    const bool labels=o.equipmentShowText||o.equipmentStyle==0||o.equipmentStyle==4;
    const bool mirroredList=o.mirrorEquipment&&o.equipmentStyle==0;
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
            const bool right=(o.equipmentStyle==0&&!mirroredList)||(o.equipmentStyle>0&&o.equipmentTextPosition==3);
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
            else if(mirroredList) {
                cell.icon.x=width-15;cell.text.x=width-38-cell.textWidth;
            } else if(o.equipmentStyle>0&&o.equipmentStyle<4&&labels&&o.equipmentTextPosition==2) {
                cell.icon.x=width-28;cell.text.x=width-64-cell.textWidth;
            }
        }
    }
    if(o.equipmentLayout==freeLayout) {
        float maxW=1,maxH=1;
        for(const auto sz:sizes){maxW=std::max(maxW,sz.x+8);maxH=std::max(maxH,sz.y+8);}
        g.scale=std::max(.05f,std::min({o.equipmentScale*resolutionScale(screenY),(screenX-16)/maxW,(screenY-16)/maxH}));
        std::vector<Point> corners;Bounds box;
        for(unsigned i=0;i<rows;++i) {
            const auto& place=o.equipmentPlaces[unsigned(roles[i])];
            g.cellScales[i]=place.scale;
            const float own=g.scale*place.scale;
            const Point size{(sizes[i].x+8)*own,(sizes[i].y+8)*own};
            Point c;
            if(place.anchor<unanchored)c=placedCorner(place.anchor,place.x,place.y,0,0,size,o,screenX,screenY);
            else if(corners.empty())c=placedCorner(3,16,-40,0,0,size,o,screenX,screenY);
            else {
                const auto prev=corners.back();
                const float step=(sizes[i-1].y+8)*g.scale*g.cellScales[i-1]+o.equipmentSpacing*g.scale;
                c={prev.x,std::clamp(prev.y+step,8.f,std::max(8.f,screenY-8-size.y))};
            }
            corners.push_back(c);box.include(c);box.include({c.x+size.x,c.y+size.y});
        }
        g.origin=box.min;g.width=std::max(1.f,box.width()/g.scale);g.height=std::max(1.f,box.height()/g.scale);
        for(unsigned i=0;i<rows;++i)g.cells[i].origin={(corners[i].x-box.min.x)/g.scale+4*g.cellScales[i],(corners[i].y-box.min.y)/g.scale+4*g.cellScales[i]};
        return g;
    }
    const auto shape=!labels?(o.equipmentStyle==1?1u:o.equipmentStyle==2?3u:2u):2u;
    const auto arranged=arrangeTiles(sizes,o.equipmentLayout,o.equipmentSpacing,shape,0,unsigned(std::clamp(o.gridColumns,1.f,8.f)));
    g.width=arranged.width+8;g.height=arranged.height+8;
    for(unsigned i=0;i<rows;++i)g.cells[i].origin={4+arranged.centers[i].x-sizes[i].x*.5f,4+arranged.centers[i].y-sizes[i].y*.5f};
    g.scale=std::max(.05f,std::min({o.equipmentScale*resolutionScale(screenY),(screenX-16)/g.width,(screenY-16)/g.height}));
    g.origin=placedCorner(o.equipmentAnchor,o.equipmentOffsetX,o.equipmentOffsetY,o.equipmentX,o.equipmentY,{g.width*g.scale,g.height*g.scale},o,screenX,screenY);
    return g;
}
// selectorOrigin is where the selector's content starts, in widget units from origin; its box begins 32 selector units above.
// focus is the part of the group tiles the selector, item name and notices gather around, in group units: the whole
// widget, or in the Free arrangement the tile of the group in use.
struct HudGeometry {LayoutGeometry groups;EquipmentGeometry equipment;Bounds focus;Point origin,groupOrigin,selectorOrigin;float scale=1,selectorScale=1,width=0,height=0,selectorHeight=150,selectorWidth=420,captionHeight=40,previewZoom=1;};
// Screen corners of the Free arrangement's tiles in display order. An unplaced tile sits beside the previous one.
inline std::vector<Point> freeTileCorners(const Options& o,const LayoutGeometry& groups,float scale,float screenX,float screenY) {
    std::vector<Point> corners;
    for(std::size_t slot=0;slot<groups.tiles.size();++slot) {
        const auto& place=o.tilePlaces[o.groupOrder[slot]];const float own=scale*place.scale;
        const auto ts=tileSizeFor(groups,slot);
        const Point size{ts.x*own,ts.y*own};
        if(place.anchor<unanchored)corners.push_back(placedCorner(place.anchor,place.x,place.y,0,0,size,o,screenX,screenY));
        else if(corners.empty())corners.push_back(placedCorner(8,-8,-8,0,0,size,o,screenX,screenY));
        else {
            const auto previous=corners.back();
            const auto prevTs=tileSizeFor(groups,slot-1);
            const float step=prevTs.x*scale*o.tilePlaces[o.groupOrder[slot-1]].scale+o.spacing*scale;
            corners.push_back({std::clamp(previous.x+step,8.f,std::max(8.f,screenX-8-size.x)),previous.y});
        }
    }
    return corners;
}
inline HudGeometry hudGeometry(const Options& o,float screenX,float screenY,unsigned rows=4,const EquipmentMetrics& metrics={},std::size_t selectorCount=5,std::size_t activeSlot=0,
                               std::array<EquipmentRole,6> equipmentRoles=defaultEquipmentRoles) {
    HudGeometry g;g.equipment=equipmentGeometry(o,screenX,screenY,rows,metrics,equipmentRoles);g.groups=groupGeometry(o);g.width=g.groups.width;g.captionHeight=15*hudTextFactor(o)+8;
    g.groupOrigin={0,o.shape==0&&o.layout!=freeLayout?3.f:o.shape==0?3.f:0.f};g.height=g.groups.height+g.groupOrigin.y;
    const auto selectorLayout=selectorGeometry(o,selectorCount);g.selectorHeight=selectorLayout.height;g.selectorWidth=selectorLayout.width;
    const float sh=g.selectorHeight+32,resolutionScale=quickslot::resolutionScale(screenY);
    const bool free=o.layout==freeLayout;
    float maxFreeW=g.groups.tileWidth,maxFreeH=g.groups.tileHeight;
    if(free)for(std::size_t i=0;i<g.groups.tiles.size();++i){const auto ts=tileSizeFor(g.groups,i);maxFreeW=std::max(maxFreeW,ts.x);maxFreeH=std::max(maxFreeH,ts.y);}
    // Free tiles each keep to the screen, so only one tile has to fit.
    const float fitWidth=free?maxFreeW:g.width,fitHeight=free?maxFreeH+g.groupOrigin.y:g.height;
    // The widget and the selector each have their own scale, kept within the screen.
    g.scale=std::max(.05f,std::min({o.scale*resolutionScale,(screenX-16)/fitWidth,(screenY-16)/fitHeight}));
    g.selectorScale=std::max(.05f,std::min({o.selectorScale*resolutionScale,(screenX-16)/g.selectorWidth,(screenY-16)/sh}));
    g.focus={{0,0},{g.groups.width,g.groups.height},true};
    if(free&&!g.groups.tiles.empty()) {
        // The widget becomes the box around the placed tiles; each tile keeps its own screen position inside it.
        const auto corners=freeTileCorners(o,g.groups,g.scale,screenX,screenY);
        for(std::size_t i=0;i<corners.size();++i)g.groups.tileScales.push_back(o.tilePlaces[o.groupOrder[i]].scale);
        Bounds box;
        for(std::size_t i=0;i<corners.size();++i) {
            const auto c=corners[i];const float own=g.scale*g.groups.tileScales[i];const auto ts=tileSizeFor(g.groups,i);
            box.include(c);box.include({c.x+ts.x*own,c.y+ts.y*own});
        }
        for(std::size_t i=0;i<corners.size();++i)g.groups.tiles[i]={(corners[i].x-box.min.x)/g.scale,(corners[i].y-box.min.y)/g.scale};
        g.groups.width=box.width()/g.scale;g.groups.height=box.height()/g.scale;g.width=g.groups.width;g.height=g.groups.height+g.groupOrigin.y;
        const auto active=std::min(activeSlot,g.groups.tiles.size()-1);const auto focused=g.groups.tiles[active];const float own=g.groups.tileScales[active];
        const auto activeTs=tileSizeFor(g.groups,active);
        g.focus={focused,{focused.x+activeTs.x*own,focused.y+activeTs.y*own},true};
        g.origin={box.min.x-g.groupOrigin.x*g.scale,box.min.y-g.groupOrigin.y*g.scale};
    }
    const float w=g.width*g.scale,h=g.height*g.scale,sw=g.selectorWidth*g.selectorScale,soh=sh*g.selectorScale,gap=o.selectorGap*g.scale;
    if(!free)g.origin=placedCorner(o.anchor,o.offsetX,o.offsetY,o.positionX,o.positionY,{w,h},o,screenX,screenY);
    // A docked selector sits beside the focus: the whole widget, or the tile in use.
    const Point focusCorner{g.origin.x+(g.groupOrigin.x+g.focus.min.x)*g.scale,g.origin.y+(g.groupOrigin.y+g.focus.min.y)*g.scale};
    const float fw=g.focus.width()*g.scale,fh=g.focus.height()*g.scale;
    const auto clamp=[&](Point p){return Point{std::clamp(p.x,8.f,std::max(8.f,screenX-8-sw)),std::clamp(p.y,8.f,std::max(8.f,screenY-8-soh))};};
    Point selector;
    if(o.selectorDock==4)selector=placedCorner(o.selectorAnchor,o.selectorOffsetX,o.selectorOffsetY,o.selectorX,o.selectorY,{sw,soh},o,screenX,screenY);
    else {
        const std::array<Point,4> docks{{{focusCorner.x+(fw-sw)*.5f,focusCorner.y-gap-soh},{focusCorner.x+(fw-sw)*.5f,focusCorner.y+fh+gap},
            {focusCorner.x-gap-sw,focusCorner.y+(fh-soh)*.5f},{focusCorner.x+fw+gap,focusCorner.y+(fh-soh)*.5f}}};

        const unsigned preferred=std::min(o.selectorDock,3u);
        const std::array<unsigned,4> order{preferred,preferred^1u,(preferred+2)%4,((preferred+2)%4)^1u};
        float best=std::numeric_limits<float>::max();
        for(const auto side:order) {
            const auto p=clamp(docks[side]);
            // The selector avoids the tiles: the widget, or in the Free arrangement every placed tile.
            const auto overlap=[&](Point corner,float width,float height) {
                return std::max(0.f,std::min(p.x+sw,corner.x+width)-std::max(p.x,corner.x))*std::max(0.f,std::min(p.y+soh,corner.y+height)-std::max(p.y,corner.y));
            };
            float score=overlap(focusCorner,fw,fh);
            if(free)for(std::size_t i=0;i<g.groups.tiles.size();++i) {
                const float own=g.scale*tileScale(g.groups,i);const auto ts=tileSizeFor(g.groups,i);
                score+=overlap({g.origin.x+(g.groupOrigin.x+g.groups.tiles[i].x)*g.scale,g.origin.y+(g.groupOrigin.y+g.groups.tiles[i].y)*g.scale},ts.x*own,ts.y*own);
            }
            if(score<best){selector=p;best=score;}
            if(score==0)break;
        }
    }
    g.selectorOrigin={(selector.x-g.origin.x)/g.scale,(selector.y-g.origin.y)/g.scale+32*g.selectorScale/g.scale};
    return g;
}
// The selector's box on screen: its top-left corner and its size.
inline Point selectorCorner(const HudGeometry& g){return {g.origin.x+g.selectorOrigin.x*g.scale,g.origin.y+g.selectorOrigin.y*g.scale-32*g.selectorScale};}
inline Point selectorExtent(const HudGeometry& g){return {g.selectorWidth*g.selectorScale,(g.selectorHeight+32)*g.selectorScale};}
// The placeable elements: 0 widget, 1 selector, 2 equipment display, 3..10 group tiles in Free arrangement,
// and 11..16 equipment role cells in Free equipment arrangement.
struct ElementSlot {unsigned Options::*anchor;float Options::*x;float Options::*y;};
inline constexpr ElementSlot elementSlots[]{{&Options::anchor,&Options::offsetX,&Options::offsetY},
    {&Options::selectorAnchor,&Options::selectorOffsetX,&Options::selectorOffsetY},{&Options::equipmentAnchor,&Options::equipmentOffsetX,&Options::equipmentOffsetY}};
inline constexpr int firstTileElement=3;
inline constexpr int firstEquipmentElement=firstTileElement+int(Arsenal::count);
inline bool tileElement(int element){return element>=firstTileElement&&element<firstEquipmentElement;}
inline std::size_t tileSlot(int element){return std::size_t(element-firstTileElement);}
inline bool equipmentElement(int element){return element>=firstEquipmentElement&&element<firstEquipmentElement+int(EquipmentRole::count);}
inline std::size_t equipmentSlot(int element){return std::size_t(element-firstEquipmentElement);}
inline Point elementCorner(const HudGeometry& g,int element) {
    if(equipmentElement(element)&&equipmentSlot(element)<g.equipment.rowCount) {
        const auto s=equipmentSlot(element);const float own=equipmentCellScale(g.equipment,s);
        const auto cell=g.equipment.cells[s];
        return {g.equipment.origin.x+(cell.origin.x-4*own)*g.equipment.scale,g.equipment.origin.y+(cell.origin.y-4*own)*g.equipment.scale};
    }
    if(tileElement(element)&&tileSlot(element)<g.groups.tiles.size()) {
        const auto tile=g.groups.tiles[tileSlot(element)];return {g.origin.x+(g.groupOrigin.x+tile.x)*g.scale,g.origin.y+(g.groupOrigin.y+tile.y)*g.scale};
    }
    return element==2?g.equipment.origin:element==1?selectorCorner(g):g.origin;
}
inline Point elementExtent(const HudGeometry& g,int element) {
    if(equipmentElement(element)&&equipmentSlot(element)<g.equipment.rowCount) {
        const auto s=equipmentSlot(element);const float own=g.equipment.scale*equipmentCellScale(g.equipment,s);
        const auto& cell=g.equipment.cells[s];
        return {(cell.width+8)*own,(cell.height+8)*own};
    }
    if(tileElement(element)){const auto s=tileSlot(element);const float own=g.scale*tileScale(g.groups,s);const auto ts=tileSizeFor(g.groups,s);return {ts.x*own,ts.y*own};}
    return element==2?Point{g.equipment.width*g.equipment.scale,g.equipment.height*g.equipment.scale}:element==1?selectorExtent(g):Point{g.width*g.scale,g.height*g.scale};
}
inline unsigned elementAnchor(const Options& o,int element,const HudGeometry* g=nullptr) {
    if(equipmentElement(element)) {
        const auto s=equipmentSlot(element);
        const auto role=g&&s<g->equipment.rowCount?g->equipment.roles[s]:EquipmentRole(std::min<std::size_t>(s,std::size_t(EquipmentRole::count)-1));
        return o.equipmentPlaces[unsigned(role)].anchor;
    }
    if(tileElement(element))return tileSlot(element)<Arsenal::count?o.tilePlaces[o.groupOrder[tileSlot(element)]].anchor:unanchored;
    return o.*elementSlots[element].anchor;
}
// An element's anchor and offset: the saved ones, or for an unanchored element those that match where it sits now.
inline Anchored elementPlacement(const Options& o,const HudGeometry& g,int element,float screenX,float screenY) {
    if(equipmentElement(element)) {
        const auto s=equipmentSlot(element);
        if(s<g.equipment.rowCount){const auto& t=o.equipmentPlaces[unsigned(g.equipment.roles[s])];if(t.anchor<unanchored)return {t.anchor,t.x,t.y};}
    } else if(tileElement(element)) {
        if(tileSlot(element)<Arsenal::count){const auto& t=o.tilePlaces[o.groupOrder[tileSlot(element)]];if(t.anchor<unanchored)return {t.anchor,t.x,t.y};}
    } else {
        const auto& slot=elementSlots[element];
        if(o.*slot.anchor<unanchored)return {o.*slot.anchor,o.*slot.x,o.*slot.y};
    }
    return anchorFor(elementCorner(g,element),elementExtent(g,element),o,screenX,screenY);
}
inline void placeElement(Options& o,int element,Anchored placement,const HudGeometry* g=nullptr) {
    if(equipmentElement(element)) {
        const auto s=equipmentSlot(element);
        const auto role=g&&s<g->equipment.rowCount?g->equipment.roles[s]:EquipmentRole(std::min<std::size_t>(s,std::size_t(EquipmentRole::count)-1));
        auto& t=o.equipmentPlaces[unsigned(role)];t.anchor=placement.anchor;t.x=placement.x;t.y=placement.y;return;
    }
    if(tileElement(element)){if(tileSlot(element)<Arsenal::count){auto& t=o.tilePlaces[o.groupOrder[tileSlot(element)]];t.anchor=placement.anchor;t.x=placement.x;t.y=placement.y;}return;}
    const auto& slot=elementSlots[element];o.*slot.anchor=placement.anchor;o.*slot.x=placement.x;o.*slot.y=placement.y;
}
inline bool hasUnplacedElements(const Options& o){return o.anchor>=unanchored||o.equipmentAnchor>=unanchored||(o.selectorDock==4&&o.selectorAnchor>=unanchored);}
// Gives every element that still uses an old proportional position the anchor and offset that keep it where it is.
inline bool anchorUnplacedElements(Options& o,float screenX,float screenY) {
    if(screenX<=0||screenY<=0)return false;
    const auto g=hudGeometry(o,screenX,screenY);bool changed=false;
    for(int element=0;element<3;++element) {
        if(element==1&&o.selectorDock!=4)continue;
        if(o.*elementSlots[element].anchor<unanchored)continue;
        placeElement(o,element,anchorFor(elementCorner(g,element),elementExtent(g,element),o,screenX,screenY));changed=true;
    }
    return changed;
}
// Places every visible tile where the given arrangement puts it and switches to the Free arrangement.
inline void freeTilesFrom(Options& o,unsigned arrangement,float screenX,float screenY) {
    auto source=o;source.layout=arrangement==freeLayout?2u:arrangement;
    const auto g=hudGeometry(source,screenX,screenY);
    for(std::size_t slot=0;slot<g.groups.tiles.size();++slot) {
        const int element=firstTileElement+int(slot);
        auto& dst=o.tilePlaces[o.groupOrder[slot]];
        const auto placed=anchorFor(elementCorner(g,element),elementExtent(g,element),o,screenX,screenY);
        dst.anchor=placed.anchor;dst.x=placed.x;dst.y=placed.y;dst.scale=1;
    }
    o.layout=freeLayout;
}
inline void freeEquipmentFrom(Options& o,unsigned arrangement,float screenX,float screenY,unsigned rows=4,const EquipmentMetrics& metrics={},
                              std::array<EquipmentRole,6> roles=defaultEquipmentRoles) {
    auto source=o;source.equipmentLayout=arrangement==freeLayout?1u:arrangement;
    const auto g=hudGeometry(source,screenX,screenY,rows,metrics,5,0,roles);
    for(unsigned slot=0;slot<g.equipment.rowCount;++slot) {
        const int element=firstEquipmentElement+int(slot);
        const auto placed=anchorFor(elementCorner(g,element),elementExtent(g,element),o,screenX,screenY);
        o.equipmentPlaces[unsigned(roles[slot])]={placed.anchor,placed.x,placed.y,1};
    }
    o.equipmentLayout=freeLayout;
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
struct SnapAxis {float value=0,guide=0;bool aligned=false;};

inline SnapAxis snapPlacementAxis(float value,float extent,float screen,float lowOffset,float highOffset,
                                  float frameLow,float frameHigh,float step,std::span<const std::array<float,2>> others) {
    const float travel=screen-16-extent;
    if(value<=0||value>=1||travel<=0||step<=0)return {std::clamp(value,0.f,1.f)};
    const float start=8+value*travel,center=(frameLow+frameHigh)*.5f;
    float delta=std::numeric_limits<float>::max(),guide=0;
    const std::array anchors{start+lowOffset,start+(lowOffset+highOffset)*.5f,start+highOffset};
    const auto consider=[&](float anchor,float target){if(std::abs(target-anchor)<std::abs(delta)){delta=target-anchor;guide=target;}};
    for(float anchor:anchors){consider(anchor,center+std::round((anchor-center)/step)*step);for(float edge:{frameLow,center,frameHigh})consider(anchor,edge);}

    float distance=std::min(6.f,step*.33f);
    for(const auto& other:others) {
        for(float anchor:anchors)for(float target:{other[0],(other[0]+other[1])*.5f,other[1]})
            if(std::abs(target-anchor)<distance){distance=std::abs(target-anchor);delta=target-anchor;guide=target;}
    }
    const float snapped=value+delta/travel,clamped=std::clamp(snapped,0.f,1.f);
    return {clamped,guide,snapped==clamped};
}
inline SnapAxis snapPlacementAxis(float value,float extent,float screen,float lowOffset,float highOffset,
                                  float frameLow,float frameHigh,float step,std::optional<std::array<float,2>> other={}) {
    if(other){const std::array<std::array<float,2>,1> one{*other};return snapPlacementAxis(value,extent,screen,lowOffset,highOffset,frameLow,frameHigh,step,one);}
    return snapPlacementAxis(value,extent,screen,lowOffset,highOffset,frameLow,frameHigh,step,std::span<const std::array<float,2>>{});
}
inline HudGeometry previewGeometry(const Options& o,float screenX,float screenY,float width,float height,bool selector,bool groups=true,bool equipment=false,unsigned rows=4,const EquipmentMetrics& metrics={},std::size_t selectorCount=5,float zoom=0,std::size_t activeSlot=0,
                                   std::array<EquipmentRole,6> equipmentRoles=defaultEquipmentRoles) {
    const auto arrange=[&](const Options& look) {
        auto g=hudGeometry(look,screenX,screenY,rows,metrics,selectorCount,activeSlot,equipmentRoles);g.origin={};g.equipment.origin={};
        if(selector&&groups&&!equipment) {
            // A docked selector sits beside the focus: the whole widget, or in the Free arrangement the tile in use.
            const float ratio=g.selectorScale/g.scale,fx=g.groupOrigin.x+g.focus.min.x,fy=g.groupOrigin.y+g.focus.min.y,w=g.focus.width(),h=g.focus.height(),sw=g.selectorWidth*ratio,sh=(g.selectorHeight+32)*ratio,gap=look.selectorGap;
            const std::array<Point,4> docks{{{fx+(w-sw)*.5f,fy-gap-sh},{fx+(w-sw)*.5f,fy+h+gap},
                {fx-gap-sw,fy+(h-sh)*.5f},{fx+w+gap,fy+(h-sh)*.5f}}};

            g.selectorOrigin=docks[look.selectorDock<4?look.selectorDock:0];g.selectorOrigin.y+=32*ratio;
        }
        return g;
    };
    const auto bounds=[&](const HudGeometry& g) {
        if(equipment)return Bounds{g.equipment.origin,{g.equipment.origin.x+g.equipment.width*g.equipment.scale,
            g.equipment.origin.y+g.equipment.height*g.equipment.scale},true};
        Bounds b;
        if(groups){b.include(g.origin);b.include({g.origin.x+g.width*g.scale,g.origin.y+(g.height+(selector?0:g.captionHeight))*g.scale});}
        if(selector) {
            const auto p=selectorCorner(g);
            b.include(p);b.include({p.x+g.selectorWidth*g.selectorScale,p.y+(g.selectorHeight+40)*g.selectorScale});
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
    g.scale*=factor;g.selectorScale*=factor;return g;
}

inline void layoutPreset(Options& o,unsigned preset) {
    o.showLabels=false;o.unifiedBar=false;o.labelPosition=0;o.activeGroupBoost=0;
    o.selector=1;o.selectorShape=0;o.selectorSpacing=8;o.spacing=4;
    if(preset==0){o.layout=5;o.shape=2;o.selectorShape=2;}
    else if(preset==1){o.layout=2;o.shape=3;o.selectorShape=3;}
    else {o.layout=4;o.shape=1;o.selector=0;o.selectorShape=1;o.spacing=2;}
}
// Card tiles in a row with a strip selector and a list of equipment.
// Every built-in look puts the equipment display in a vertical list at the middle of the left edge.
inline void equipmentListPreset(Options& o) {
    o.equipmentStyle=0;o.equipmentLayout=1;o.equipmentShowText=true;o.equipmentSpacing=8;
    o.equipmentAnchor=3;o.equipmentOffsetX=16;o.equipmentOffsetY=0;
}
inline void cardsPreset(Options& o) {
    o.showLabels=true;o.labelPosition=0;o.unifiedBar=false;o.activeGroupBoost=0;o.layout=0;o.shape=0;o.selector=0;o.selectorShape=0;o.spacing=14;o.selectorSpacing=16;
    equipmentListPreset(o);
}
// Diamond tiles with a wheel of segments for a selector.
inline void wheelPreset(Options& o) {
    o.showLabels=false;o.labelPosition=0;o.unifiedBar=false;o.activeGroupBoost=0;o.layout=2;o.shape=3;o.selector=wheelSelector;o.selectorShape=3;o.wheelStyle=1;o.selectorSpacing=8;o.spacing=4;
    equipmentListPreset(o);
}
// Tight 4-way Souls/D-Pad cross with true 1:1 squares and a compact list selector.
inline void soulsCrossPreset(Options& o) {
    o.showLabels=false;o.labelPosition=0;o.unifiedBar=false;o.layout=5;o.shape=2;o.spacing=4;o.activeGroupBoost=.1f;o.keyPosition=0;
    o.selector=4;o.selectorShape=2;o.selectorSpacing=8;o.anchor=6;o.offsetX=28;o.offsetY=-28;
    equipmentListPreset(o);
}
// Unified MMO-style bottom-center hotbar with labels above and corner hotkeys.
inline void actionBarPreset(Options& o) {
    o.showLabels=true;o.labelPosition=1;o.unifiedBar=true;o.layout=0;o.shape=2;o.spacing=0;o.activeGroupBoost=.08f;o.keyPosition=5;
    o.selector=0;o.selectorShape=2;o.selectorSpacing=8;o.anchor=7;o.offsetX=0;o.offsetY=-22;
    equipmentListPreset(o);
}
inline void equipmentLayoutPreset(Options& o,unsigned){equipmentListPreset(o);}
}

#pragma once

// Measure the drawn outlines independently of Layout.h's placement formulas.
namespace spacing_test {
using Polygon=std::array<Point,4>;
Polygon outline(Point center,Point size,unsigned shape) {
    const float x=center.x,y=center.y,w=size.x*.5f,h=size.y*.5f;
    if(shape==3)return {{{x,y-h},{x+w,y},{x,y+h},{x-w,y}}};
    return {{{x-w,y-h},{x+w,y-h},{x+w,y+h},{x-w,y+h}}};
}
float pointSegment(Point p,Point a,Point b) {
    const float x=b.x-a.x,y=b.y-a.y;
    const float t=std::clamp(((p.x-a.x)*x+(p.y-a.y)*y)/(x*x+y*y),0.f,1.f);
    return std::hypot(p.x-a.x-t*x,p.y-a.y-t*y);
}
float gap(Point a,Point as,Point b,Point bs,unsigned shape) {
    if(shape==1)return std::hypot(a.x-b.x,a.y-b.y)-(as.x+bs.x)*.5f;
    const auto left=outline(a,as,shape),right=outline(b,bs,shape);
    bool separated=false;float distance=1e9f;
    for(const auto& polygon:{left,right})for(unsigned i=0;i<4;++i) {
        const auto p=polygon[i],q=polygon[(i+1)%4];const Point axis{q.y-p.y,p.x-q.x};
        float amin=1e9f,amax=-1e9f,bmin=1e9f,bmax=-1e9f;
        for(const auto v:left){const float d=v.x*axis.x+v.y*axis.y;amin=std::min(amin,d);amax=std::max(amax,d);}
        for(const auto v:right){const float d=v.x*axis.x+v.y*axis.y;bmin=std::min(bmin,d);bmax=std::max(bmax,d);}
        if(std::min(amax,bmax)-std::max(amin,bmin)<.01f)separated=true;
    }
    if(!separated)return -1;
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
        distance=std::min(distance,pointSegment(left[i],right[j],right[(j+1)%4]));
        distance=std::min(distance,pointSegment(right[i],left[j],left[(j+1)%4]));
    }
    return distance;
}
void run() {
    for(unsigned layout=0;layout<6;++layout)for(unsigned shape=1;shape<4;++shape)
        for(unsigned count=2;count<=8;++count)for(float requested:{0.f,8.f,32.f,128.f}) {
            Options o;o.layout=layout;o.shape=shape;o.groupCount=count;o.showLabels=false;o.spacing=requested;
            const auto g=groupGeometry(o);
            const Point size=shape==1?Point{58,58}:shape==2?Point{82,100}:Point{84,84};
            const Point offset{g.tileWidth*.5f,shape==1?37.f:shape==2?50.f:44.f};float nearest=1e9f;
            for(unsigned i=0;i<count;++i)for(unsigned j=0;j<i;++j) {
                const float actual=gap({g.tiles[i].x+offset.x,g.tiles[i].y+offset.y},size,
                    {g.tiles[j].x+offset.x,g.tiles[j].y+offset.y},size,shape);
                check(actual>=requested-.02f,"Group frame outlines maintain the requested minimum gap");nearest=std::min(nearest,actual);
            }
            if(std::abs(nearest-requested)>=.03f)std::cerr<<"Group gap layout="<<layout<<" shape="<<shape<<" count="<<count<<" requested="<<requested<<" actual="<<nearest<<'\n';
            check(std::abs(nearest-requested)<.03f,"The nearest group frames use the full numeric spacing range");
        }
    for(unsigned layout:{0u,1u,3u})for(unsigned shape=1;shape<4;++shape)
        for(float requested:{0.f,8.f,32.f,128.f})for(float animation:{-.6f,-.25f,0.f,.3f,.8f}) {
            Options o;o.selector=layout;o.selectorShape=shape;o.selectorSpacing=requested;
            for(int slot=-2;slot<2;++slot) {
                const float a=float(slot)+animation,b=a+1,ar=selectorRadius(std::abs(a)),br=selectorRadius(std::abs(b));
                check(std::abs(gap(selectorOffset(o,a),{2*ar,2*ar},selectorOffset(o,b),{2*br,2*br},shape)-requested)<.03f,
                    "Selector outlines maintain their gap throughout animated scrolling");
            }
        }
    for(unsigned shape=1;shape<4;++shape)for(unsigned count=2;count<=8;++count)for(float requested:{0.f,32.f,128.f})for(float phase:{0.f,.25f,.6f}) {
        Options o;o.selector=2;o.selectorShape=shape;o.selectorSpacing=requested;
        const auto ring=selectorGeometry(o,count,phase);float nearest=1e9f;
        for(unsigned i=0;i<count;++i)for(unsigned j=0;j<i;++j) {
            const float a=i?44.f:58.f,b=j?44.f:58.f;
            const float actual=gap(ring.ring[i],{a,a},ring.ring[j],{b,b},shape);
            check(actual>=requested-.03f,"Ring selector frames keep the requested minimum gap");nearest=std::min(nearest,actual);
        }
        check(std::abs(nearest-requested)<.03f,"Ring selector spacing uses the visible frame edges");
    }
    EquipmentMetrics metrics;metrics.measured=true;metrics.textWidths={80,120,40,65,30};
    Options equipment;equipment.equipmentStyle=3;equipment.equipmentLayout=0;equipment.equipmentTextPosition=3;
    for(float requested:{0.f,16.f,32.f,128.f}) {
        equipment.equipmentSpacing=requested;const auto g=equipmentGeometry(equipment,3840,2160,5,metrics);
        for(unsigned i=0;i<5;++i) {
            check(g.cells[i].width==64+metrics.textWidths[i],"Equipment row widths follow the measured item labels");
            if(i)check(std::abs(g.cells[i].origin.x-g.cells[i-1].origin.x-g.cells[i-1].width-requested)<.01f,
                "Horizontal equipment uses the exact gap between adjacent content bounds");
        }
    }
    for(unsigned layout=0;layout<6;++layout)for(unsigned style=1;style<4;++style)for(unsigned count:{3u,4u,5u})for(float requested:{0.f,32.f,128.f}) {
        equipment.equipmentLayout=layout;equipment.equipmentStyle=style;equipment.equipmentShowText=false;equipment.equipmentSpacing=requested;
        const auto g=equipmentGeometry(equipment,3840,2160,count);float nearest=1e9f;
        for(unsigned i=0;i<count;++i)for(unsigned j=0;j<i;++j) {
            const auto& a=g.cells[i];const auto& b=g.cells[j];
            const float actual=gap({a.origin.x+a.icon.x,a.origin.y+a.icon.y},{56,56},{b.origin.x+b.icon.x,b.origin.y+b.icon.y},{56,56},style==2?3:style==3?2:1);
            check(actual>=requested-.03f,"Equipment tile arrangements keep frame outlines separated");nearest=std::min(nearest,actual);
        }
        check(std::abs(nearest-requested)<.03f,"Equipment tiles share the widget's spacing behavior");
    }
    for(unsigned oldLayout:{0u,1u})for(unsigned style=0;style<5;++style)for(float oldSpacing:{0.f,1.f,4.f}) {
        auto saved=Json::parse(encodeOptions(Options{}));saved.erase("layoutVersion");saved["equipmentLayout"]=oldLayout;saved["equipmentStyle"]=style;saved["equipmentSpacing"]=oldSpacing;
        const auto old=decodeOptions(saved.dump());
        check(old.equipmentLayout==(style==0?1u:oldLayout==1?0u:1u),"Existing equipment arrangements retain their orientation");
        check(old.equipmentSpacing==oldSpacing*(style==0?4:oldLayout==1?12:8),"Legacy equipment multipliers migrate into numeric gaps");
        const auto again=decodeOptions(encodeOptions(old));
        check(again.equipmentSpacing==old.equipmentSpacing&&again.equipmentLayout==old.equipmentLayout,"Spacing migration runs once");
    }
    for(unsigned layout=0;layout<6;++layout) {
        equipment.equipmentLayout=layout;equipment.equipmentSpacing=81;equipment.equipmentShowText=false;
        const auto restored=decodeOptions(encodeOptions(equipment));Options imported;importAppearance(imported,restored);
        check(imported.equipmentLayout==layout&&imported.equipmentSpacing==81&&!imported.equipmentShowText,
            "All equipment layouts, numeric gaps and name visibility survive save and appearance import");
    }
}
}

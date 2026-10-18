#pragma once
#include "Model.h"

namespace quickslot {
class ScrollMotion {
    bool active_=false;
    std::size_t group_=0,count_=0,index_=0;
    double started_=0;
    float from_=0,to_=0;
    float value(double now)const{const float t=float(std::clamp((now-started_)/.12,0.,1.));const float ease=1-(1-t)*(1-t)*(1-t);return from_+(to_-from_)*ease;}
public:
    float update(const Snapshot& s,const Options& o,double now) {
        if(!s.browsing||!o.animateScroll||s.editorPreview){active_=false;return 0;}
        const auto n=s.rows.size();
        if(!active_||group_!=s.activeGroup||count_!=n||n<2){active_=true;group_=s.activeGroup;count_=n;index_=s.preview;from_=to_=0;started_=now;return 0;}
        if(index_!=s.preview) {
            int delta=int(s.preview)-int(index_);if(delta>int(n)/2)delta-=int(n);if(delta<-int(n)/2)delta+=int(n);
            from_=value(now)-to_-float(delta);to_=0;started_=now;index_=s.preview;
        }
        return -value(now);
    }
};
class IdleFade {
    bool initialized_=false,wasBrowsing_=false;
    std::size_t group_=0,index_=0;
    double activity_=0,notice_=0;
public:
    float update(const Snapshot& s,const Options& o,double now) {
        if(!initialized_||s.browsing||wasBrowsing_||s.activeGroup!=group_||s.selected!=index_||s.noticeUntil!=notice_||!s.visible||s.editorPreview||(s.inCombat&&o.keepCombatVisible))activity_=now;
        initialized_=true;wasBrowsing_=s.browsing;group_=s.activeGroup;index_=s.selected;notice_=s.noticeUntil;
        if(!o.fadeIdle||s.editorPreview||s.browsing)return 1;
        const float t=float(std::clamp((now-activity_-o.fadeDelay)/.35,0.,1.));return 1+(o.idleOpacity-1)*t;
    }
};
}

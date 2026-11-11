#pragma once
#include "Model.h"
#include <map>

namespace quickslot {
enum class WidgetVisibility {always,fade,hide,selecting};
inline WidgetVisibility widgetVisibility(const Options& o) {
    if(!o.showIdle)return WidgetVisibility::selecting;
    if(!o.fadeIdle)return WidgetVisibility::always;
    return o.idleOpacity<=0?WidgetVisibility::hide:WidgetVisibility::fade;
}
inline void setWidgetVisibility(Options& o,WidgetVisibility mode) {
    o.showIdle=mode!=WidgetVisibility::selecting;
    o.fadeIdle=mode==WidgetVisibility::fade||mode==WidgetVisibility::hide;
    if(mode==WidgetVisibility::hide)o.idleOpacity=0;
    else if(mode==WidgetVisibility::fade&&o.idleOpacity<=0)o.idleOpacity=.35f;
}
enum class EquipmentVisibility {off,always,dim};
inline EquipmentVisibility equipmentVisibility(const Options& o){return !o.showEquipment?EquipmentVisibility::off:o.equipmentDim?EquipmentVisibility::dim:EquipmentVisibility::always;}
inline void setEquipmentVisibility(Options& o,EquipmentVisibility mode){o.showEquipment=mode!=EquipmentVisibility::off;o.equipmentDim=mode==EquipmentVisibility::dim;}
struct FadePolicy {
    bool showIdle=true,fadeIdle=false,keepCombatVisible=true,keepDrawnVisible=false;
    float fadeDelay=4,idleOpacity=.35f;
};

inline float easeOut(float t){t=std::clamp(t,0.f,1.f);return 1-(1-t)*(1-t)*(1-t);}
// Display-side easing toward a target. A value rises over `rise` seconds and falls over `fall`;
// zero follows the target at once. After a pause in drawing it starts again from zero.
class Eased {
    float shown_=0;
    double last_=0;
    bool started_=false;
public:
    float update(float target,double now,float rise,float fall=0) {
        double elapsed=started_?now-last_:-1;last_=now;
        // A fresh start begins one frame in, so the first frame already draws.
        if(elapsed<0||elapsed>.5){started_=true;shown_=0;elapsed=1./60;}
        const float step=float(elapsed);
        if(target>shown_)shown_=rise>0?std::min(target,shown_+step/rise):target;
        else if(target<shown_)shown_=fall>0?std::max(target,shown_-step/fall):target;
        return shown_;
    }
    float value()const{return shown_;}
};
// Marks the moment a group's item becomes equipped, changes while equipped, or is used.
class TilePulse {
    bool seen_=false,equipped_=false;
    std::uint32_t form_=0;
    std::uint16_t unique_=0;
    int quantity_=0;
    double started_=-1;
public:
    static constexpr double duration=.45;
    // Returns progress from 0 to 1 while a pulse plays and -1 otherwise.
    float update(bool present,bool equipped,std::uint32_t form,std::uint16_t unique,int quantity,bool stack,double now) {
        const bool changed=form!=form_||unique!=unique_;
        if(seen_&&present&&((equipped&&(!equipped_||changed))||(stack&&!changed&&quantity<quantity_)))started_=now;
        seen_=true;equipped_=present&&equipped;form_=form;unique_=unique;quantity_=quantity;
        const double progress=started_<0?-1:(now-started_)/duration;
        return progress>=0&&progress<1?float(progress):-1;
    }
    void reset(){seen_=false;started_=-1;}
};
// Turns a remaining cooldown into a fraction of the longest value seen for that power.
class CooldownScale {
    std::map<std::uint32_t,float> peak_;
public:
    float fraction(std::uint32_t form,float remaining) {
        if(!(remaining>0)){peak_.erase(form);return 0;}
        auto& peak=peak_[form];peak=std::max(peak,remaining);
        if(peak_.size()>64)peak_.erase(peak_.begin());
        return std::clamp(remaining/peak,0.f,1.f);
    }
};
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
    std::uint64_t interaction_=0;
    double activity_=0,notice_=0;
public:
    float update(const Snapshot& s,const Options& o,double now) {
        return update(s,FadePolicy{o.showIdle,o.fadeIdle,o.keepCombatVisible,o.keepDrawnVisible,o.fadeDelay,o.idleOpacity},now);
    }
    void reset(){initialized_=false;}
    float update(const Snapshot& s,const FadePolicy& o,double now,bool changed=false) {
        if(!s.visible){initialized_=false;return 0;}
        if(!initialized_||changed||s.holding||s.browsing||wasBrowsing_||s.activeGroup!=group_||s.selected!=index_||s.interaction!=interaction_||s.noticeUntil!=notice_||s.editorPreview||
           (s.inCombat&&o.keepCombatVisible)||(s.weaponsDrawn&&o.keepDrawnVisible))activity_=now;
        initialized_=true;wasBrowsing_=s.browsing;group_=s.activeGroup;index_=s.selected;interaction_=s.interaction;notice_=s.noticeUntil;
        if(s.editorPreview||s.holding||s.browsing||now<s.revealUntil)return 1;
        if(!o.showIdle)return 0;
        if(!o.fadeIdle)return 1;
        const float t=float(std::clamp((now-activity_-o.fadeDelay)/.35,0.,1.));return 1+(o.idleOpacity-1)*t;
    }
};
class EquipmentFade {
    struct Key {
        bool present=false;
        std::uint32_t form=0,owner=0;
        std::uint16_t unique=0;
        std::uint64_t recipe=0;
        Kind kind=Kind::sword;
        bool twoHanded=false,available=false,powerUsed=false,cooling=false;
        int quantity=0;
        bool operator==(const Key&)const=default;
    };
    std::array<Key,4> previous_{};
    IdleFade fade_;
public:
    float update(const Snapshot& s,const Options& o,double now) {
        if(!o.showEquipment){fade_.reset();return 0;}
        const auto key=[](const std::optional<Row>& row) {
            if(!row)return Key{};
            const auto& e=row->entry;
            return Key{true,e.form,e.owner,e.unique,e.recipe,e.kind,e.twoHanded,row->available,row->powerUsed,row->cooldown>0,row->quantity};
        };
        const std::array current{key(s.equipment.hands[0]),key(s.equipment.hands[1]),key(s.equipment.consumable),key(s.equipment.voice)};
        const bool changed=current!=previous_;previous_=current;

        return fade_.update(s,FadePolicy{true,o.equipmentDim,true,false,o.equipmentFadeDelay,o.equipmentIdleOpacity},now,changed);
    }
};
}

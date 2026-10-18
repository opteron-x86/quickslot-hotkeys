#pragma once
#include "Model.h"

namespace quickslot {
inline bool activatesVoice(const Entry& entry,const Selection::Commit& c,const Options& o) {
    return isVoice(entry)&&o.voiceHotkey&&(!c.browsed||o.voiceOnRelease);
}
class VoiceAction {
    bool active_=false,released_=false;
    double started_=0;
    float duration_=0;
public:
    enum class Step {idle,held,release};
    bool begin(double now,float duration){if(active_)return false;active_=true;released_=false;started_=now;duration_=std::clamp(duration,.05f,2.5f);return true;}
    bool active()const{return active_;}
    float elapsed(double now)const{return float(std::max(0.,now-started_));}
    Step tick(double now){if(!active_||released_)return Step::idle;if(elapsed(now)<duration_)return Step::held;released_=true;return Step::release;}
    bool cancel(){const bool was=active_;active_=false;return was;}
};
}

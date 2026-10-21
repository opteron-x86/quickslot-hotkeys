#pragma once
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
namespace quickslot {
enum class SelectionClock {full,slow,pause};
inline SelectionClock selectionClock(float speed){return speed==0?SelectionClock::pause:speed<.999f?SelectionClock::slow:SelectionClock::full;}
inline bool selectionPauseAllowsInput(unsigned pauseCount,bool ownPauseOpen){return pauseCount<=unsigned(ownPauseOpen);}

class SelectionTime {
    bool owned_=false,yielded_=false;
    float before_=1,targetBefore_=1,applied_=1,targetApplied_=1;
public:
    std::optional<std::pair<float,float>> update(bool selecting,float speed,float current,float target) {
        const auto close=[](float a,float b){return std::abs(a-b)<.0001f;};
        if(owned_&&(!close(current,applied_)||!close(target,targetApplied_))){owned_=false;yielded_=true;}
        if(!selecting||selectionClock(speed)!=SelectionClock::slow) {
            yielded_=false;if(!owned_)return {};
            owned_=false;return std::pair{before_,targetBefore_};
        }
        if(owned_||yielded_||!std::isfinite(current)||!std::isfinite(target)||current<=0||target<=0)return {};
        before_=current;targetBefore_=target;applied_=current*std::clamp(speed,.1f,1.f);targetApplied_=target*std::clamp(speed,.1f,1.f);owned_=true;
        return std::pair{applied_,targetApplied_};
    }
};
}

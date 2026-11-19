#pragma once
#include <algorithm>
#include <cmath>

namespace quickslot {
// The casting animation a cast plays. The values are the "Animation" component of the Quickslot_Casting condition that
// the casting animation pack's Open Animation Replacer submods test, so they must not change.
enum class CastAnimation : int {none,aimed,self,aimedConcentration,selfConcentration,dualAimed,dualSelf,dualConcentration,ritual,dualSelfConcentration};
// The hand the animation shows: the condition's "Hand" component. Two-handed spells use both hands.
enum class CastSide : int {both,right,left};
inline CastAnimation castAnimationFor(bool self,bool concentration,bool twoHanded) {
    if(concentration)return twoHanded?(self?CastAnimation::dualSelfConcentration:CastAnimation::dualConcentration):(self?CastAnimation::selfConcentration:CastAnimation::aimedConcentration);
    if(twoHanded)return CastAnimation::ritual;
    return self?CastAnimation::self:CastAnimation::aimed;
}
inline bool concentrationAnimation(CastAnimation a) {
    return a==CastAnimation::aimedConcentration||a==CastAnimation::selfConcentration||a==CastAnimation::dualConcentration||a==CastAnimation::dualSelfConcentration;
}
// Seconds the pack's release clip plays, the same in both camera views. A concentration cast ends with the short
// blend back to the idle pose instead.
inline float releaseSeconds(CastAnimation a) {
    switch(a) {
        case CastAnimation::aimed:return .833f;
        case CastAnimation::self:case CastAnimation::ritual:return 1.f;
        case CastAnimation::dualAimed:return .967f;
        case CastAnimation::dualSelf:return .8f;
        case CastAnimation::none:return 0;
        default:return .4f;
    }
}
namespace castTiming {
// The shortest charge of a fire-and-forget cast, and how far ahead of the spell leaving the hand its release clip starts.
inline constexpr float minimumCharge=.25f,releaseLead=.12f;
// A concentration spell raises the hands this long before it starts, and its casting clip is started again this often.
inline constexpr float concentrationWindup=.1f,loopInterval=.5f;
// The shortest time between two casts.
inline constexpr float recovery=.25f;
// The release clip stays replaced this much longer than it plays. Afterwards the graph needs settle seconds before
// the next cast's animation starts the same way.
inline constexpr float lingerMargin=.15f,settle=.3f;
// A press that arrives while a cast or its release is still playing waits this long at most.
inline constexpr float queueAge=3.f;
}

// One cast from the press to the spell leaving the hand, or to the end of a concentration spell. It makes no game
// calls: tick() says what the game side does next, in this order: release (start the release clip), fire (cast the
// spell), restart (start the casting clip again), stop (end the cast now).
class CastSequence {
    bool active_=false,concentration_=false,channeling_=false,held_=false,released_=false;
    float charge_=0,total_=0,loop_=0;
public:
    struct Step {bool release=false,fire=false,restart=false,stop=false;};
    // chargeSeconds: the spell's own charge time. held: the key that keeps a concentration spell going is down.
    void begin(float chargeSeconds,bool concentration,bool held=true) {
        active_=true;concentration_=concentration;channeling_=false;held_=held;released_=false;charge_=loop_=0;
        const float charge=std::isfinite(chargeSeconds)?std::max(0.f,chargeSeconds):0.f;
        total_=concentration?castTiming::concentrationWindup:std::max(charge,castTiming::minimumCharge);
    }
    bool active()const{return active_;}
    bool channeling()const{return channeling_;}
    bool concentration()const{return concentration_;}
    // 0 to 1 while charging, 1 while a concentration spell runs.
    float progress()const{return channeling_||!(total_>0)?1.f:std::clamp(charge_/total_,0.f,1.f);}
    // The key was let go, or tapped again to end a concentration spell.
    void letGo(){held_=false;}
    // The spell went off: a concentration spell runs from here, any other cast is over.
    void fired(){if(concentration_){channeling_=true;loop_=0;}else active_=false;}
    void end(){active_=channeling_=false;}
    Step tick(float seconds) {
        Step step;if(!active_)return step;
        const float dt=std::isfinite(seconds)?std::max(0.f,seconds):0.f;
        if(channeling_) {
            if(!held_){step.stop=true;end();return step;}
            loop_+=dt;if(loop_>=castTiming::loopInterval){loop_=0;step.restart=true;}
            return step;
        }
        charge_+=dt;
        if(concentration_&&!held_){step.stop=true;end();return step;}
        if(!concentration_&&!released_&&total_-charge_<=castTiming::releaseLead){released_=true;step.release=true;}
        if(charge_>=total_)step.fire=true;
        return step;
    }
};
// What the casting animation still plays after a cast: the release clip, which must stay replaced to its end, then the
// moment the graph needs before the next cast starts.
class CastLinger {
    float remaining_=0,sinceEnd_=10;
    bool lingering_=false,stopAtEnd_=false;
public:
    // stopAtEnd: the graph is still in the shout state the cast plays in and leaves it when the clip is over.
    void linger(float clipSeconds,bool stopAtEnd) {
        lingering_=true;stopAtEnd_=stopAtEnd;remaining_=std::max(0.f,clipSeconds)+castTiming::lingerMargin;
    }
    // A new cast took over the animation.
    void restart(){lingering_=false;sinceEnd_=10;}
    bool lingering()const{return lingering_;}
    // Seconds until a new cast's animation starts cleanly.
    float wait()const{return lingering_?remaining_+(stopAtEnd_?castTiming::settle:0.f):std::max(0.f,castTiming::settle-sinceEnd_);}
    enum class Tick {none,endShout,clear};
    // endShout: the release clip is over and the shout state ends now. clear: the clip is over and nothing is left to end.
    Tick tick(float seconds) {
        const float dt=std::isfinite(seconds)?std::max(0.f,seconds):0.f;
        sinceEnd_=std::min(sinceEnd_+dt,10.f);
        if(!lingering_)return Tick::none;
        remaining_-=dt;if(remaining_>0)return Tick::none;
        lingering_=false;sinceEnd_=stopAtEnd_?0.f:castTiming::settle;
        return stopAtEnd_?Tick::endShout:Tick::clear;
    }
};
}

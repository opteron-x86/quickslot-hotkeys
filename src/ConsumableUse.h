#pragma once
#include <algorithm>
#include <string_view>
namespace quickslot {

class ConsumableUse {
    bool pending_=false,poison_=false,sawMenu_=false,animation_=false,expected_=false;
    int before_=0;
    double deadline_=0,nextUse_=0,animationAge_=0,last_=0,settleUntil_=0;
public:
    enum class Result { idle, waiting, decreased, cancelled, unconfirmed };
    bool active()const{return pending_||animation_;}
    bool animating()const{return animation_;}
    bool begin(double now,int count,bool poison,bool animated=false) {
        if(active()||now<nextUse_||count<=0)return false;
        pending_=true;poison_=poison;sawMenu_=false;expected_=animated;before_=count;
        deadline_=now+(animated?3:10);nextUse_=now+.35;last_=now;settleUntil_=0;return true;
    }
    void animation(double now,std::string_view tag,std::string_view payload={}) {
        if((tag=="AnimObjLoad"&&payload=="AnimObjectDrinkPotion")||tag=="DrinkPotionStart"||tag=="DrinkPotion_Slow"||tag=="DrinkPotion_NoMove") {
            animation_=true;animationAge_=0;last_=now;settleUntil_=0;
            if(pending_&&!poison_)deadline_=now+30;
        } else if(animation_&&(tag=="DrinkPotionStop"||tag=="EnableBumper"||tag=="MTState")) {
            animation_=false;nextUse_=std::max(nextUse_,now+.2);
            if(pending_&&!poison_)settleUntil_=now+.25;
        }
    }
    Result observe(double now,int count,bool menuOpen,float gameSpeed=1,bool paused=false) {
        const double delta=std::max(0.,now-last_);last_=now;
        if(animation_&&!paused)animationAge_+=delta*std::clamp(gameSpeed,.01f,10.f);
        if(animation_&&animationAge_>=8){animation_=false;nextUse_=now+.2;}
        if(!pending_)return animation_?Result::waiting:Result::idle;
        if(count<before_) {
            pending_=false;

            if(expected_&&!animation_&&!settleUntil_){animation_=true;animationAge_=5;}
            return Result::decreased;
        }
        if(poison_&&menuOpen){sawMenu_=true;deadline_=now+1;return Result::waiting;}
        if(poison_&&sawMenu_&&!menuOpen){pending_=false;return Result::cancelled;}
        if(paused){deadline_+=delta;return Result::waiting;}
        if((settleUntil_&&now>=settleUntil_)||(!animation_&&now>=deadline_)) {pending_=false;return Result::unconfirmed;}
        return Result::waiting;
    }
    void reset(){*this={};}
};
}

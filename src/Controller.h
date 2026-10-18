#pragma once
#include "Model.h"

namespace quickslot {
enum class PadAction {none,press,release,cancel,previous,next,unequip,editor};
struct PadResult {bool consumed=false;PadAction action=PadAction::none;std::size_t group=0;};
class ControllerInput {
    std::array<bool,unsigned(PadButton::count)> down_{},owned_{};
    std::optional<PadBinding> active_;
    std::size_t group_=0;
    int direction_=0;
    double repeatAt_=0;
public:
    bool down(PadButton b)const{return validPadButton(b)&&down_[unsigned(b)];}
    bool active()const{return active_.has_value();}
    PadBinding binding()const{return active_.value_or(PadBinding{});}
    bool owned(PadButton b)const{return validPadButton(b)&&owned_[unsigned(b)];}
    void cancel(){active_.reset();direction_=0;repeatAt_=0;}
    void begin(PadBinding binding,std::size_t group){active_=binding;group_=group;direction_=0;}
    void reset(){cancel();down_.fill(false);owned_.fill(false);}
    bool synchronize(const std::array<bool,unsigned(PadButton::count)>& physical) {
        const bool lost=active_&&(!physical[unsigned(active_->button)]||(active_->modifier!=PadButton::none&&!physical[unsigned(active_->modifier)]));
        if(lost)cancel();
        for(unsigned i=1;i<physical.size();++i){down_[i]=physical[i];if(!physical[i])owned_[i]=false;}
        return lost;
    }
    bool editorButton(PadButton b,bool isDown,bool fresh) {
        if(b==PadButton::none||!validPadButton(b))return true;
        down_[unsigned(b)]=isDown;auto& owner=owned_[unsigned(b)];
        if(fresh){owner=true;return true;}
        if(!isDown){const auto result=owner;owner=false;return result;}
        return true;
    }
    PadResult button(PadButton b,bool isDown,bool fresh,const Options& o,bool allowed,bool selecting,bool vr=false) {
        if(b==PadButton::none||!validPadButton(b))return {};
        const auto n=unsigned(b);down_[n]=isDown;
        if(!allowed)cancel();
        if(active_&&!isDown&&b==active_->modifier){const bool captured=owned_[n];owned_[n]=false;cancel();return {captured,PadAction::cancel};}
        if(owned_[n]) {
            if(!isDown) {
                owned_[n]=false;
                if(active_&&b==active_->button){const auto g=group_;cancel();return {true,PadAction::release,g};}
            }
            return {true};
        }
        if(!allowed||!o.controllerEnabled||!fresh)return {};
        const auto matches=[&](PadBinding bind){return bind.button!=PadButton::none&&bind.button==b&&(bind.modifier==PadButton::none||down(bind.modifier));};
        if(matches(vr?o.vrEditor:o.controllerEditor)){owned_[n]=true;cancel();return {true,PadAction::editor};}
        const auto groupBinding=[&](bool chord)->std::optional<PadResult> {
            for(std::size_t p=0;p<o.groupCount;++p) {
                const auto g=o.groupOrder[p];const auto bind=(vr?o.vrGroups:o.controllerGroups)[g];
                if((bind.modifier!=PadButton::none)==chord&&matches(bind)) {
                    owned_[n]=true;active_=bind;group_=g;direction_=0;return PadResult{true,PadAction::press,g};
                }
            }
            return {};
        };
        if(const auto group=groupBinding(true))return *group;
        if(selecting) {
            if(b==PadButton::b){owned_[n]=true;cancel();return {true,PadAction::cancel};}
            if(b==PadButton::x){owned_[n]=true;cancel();return {true,PadAction::unequip};}
            if(!vr&&o.controllerBumpers&&(!active_||b!=active_->modifier)&&(b==PadButton::lb||b==PadButton::rb)) {
                owned_[n]=true;return {true,b==PadButton::lb?PadAction::previous:PadAction::next};
            }
        }
        if(const auto group=groupBinding(false))return *group;
        return {};
    }
    int stick(float x,float y,double now,float deadzone) {
        if(!std::isfinite(x)||!std::isfinite(y)){direction_=0;return 0;}
        const float value=std::abs(x)>=std::abs(y)?x:-y;
        const int direction=value>deadzone?1:value<-deadzone?-1:0;
        if(!direction){direction_=0;return 0;}
        if(direction!=direction_){direction_=direction;repeatAt_=now+.35;return direction;}
        if(now>=repeatAt_){repeatAt_=now+.12;return direction;}
        return 0;
    }
};
}

#pragma once
#include "Model.h"

namespace quickslot {
enum class PadAction {none,press,release,cancel,previous,next,unequip,editor,rightHand,leftHand};
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

    void observe(const PadState& physical){down_=physical;}
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
    PadResult button(PadButton b,bool isDown,bool fresh,const Options& o,bool allowed,bool selecting,bool vr=false,bool browse=true,bool editorHotkey=true) {
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
        unsigned modifiers=o.controllerModifiers;
        if(!vr) {
            modifiers|=padButtonMask(o.controllerEditor.modifier);
            for(std::size_t p=0;p<o.groupCount;++p)modifiers|=padButtonMask(o.controllerGroups[o.groupOrder[p]].modifier);
            for(const auto binding:o.controllerActions)modifiers|=padButtonMask(binding.modifier);
        }
        const auto matches=[&](PadBinding bind){
            if(bind.button==PadButton::none||bind.button!=b||(bind.modifier!=PadButton::none&&!down(bind.modifier)))return false;
            if(!vr)for(unsigned i=1;i<down_.size();++i)
                if(down_[i]&&(modifiers&(1u<<i))&&PadButton(i)!=bind.button&&PadButton(i)!=bind.modifier)return false;
            return true;
        };
        if(editorHotkey&&matches(vr?o.vrEditor:o.controllerEditor)){owned_[n]=true;cancel();return {true,PadAction::editor};}
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
            if(vr) {
                if(b==PadButton::b){owned_[n]=true;cancel();return {true,PadAction::cancel};}
                if(b==PadButton::x){owned_[n]=true;cancel();return {true,PadAction::unequip};}
            }else {
                const auto actionMatches=[&](PadBinding binding) {
                    if(binding.button==PadButton::none||binding.button!=b||(binding.modifier!=PadButton::none&&!down(binding.modifier)))return false;
                    if(active_&&(b==active_->button||b==active_->modifier))return false;
                    if(binding.modifier==PadButton::none) {
                        if(o.controllerEditor.modifier==b)return false;
                        for(std::size_t p=0;p<o.groupCount;++p)if(o.controllerGroups[o.groupOrder[p]].modifier==b)return false;
                        for(const auto other:o.controllerActions)if(other.modifier==b)return false;
                    }
                    for(unsigned i=1;i<down_.size();++i)if(down_[i]&&(modifiers&(1u<<i))&&PadButton(i)!=b&&PadButton(i)!=binding.modifier&&
                        (!active_||(PadButton(i)!=active_->button&&PadButton(i)!=active_->modifier)))return false;
                    return true;
                };
                for(bool chord:{true,false})for(unsigned i=0;i<o.controllerActions.size();++i) {
                    const auto binding=o.controllerActions[i];
                    if((binding.modifier!=PadButton::none)!=chord||!actionMatches(binding))continue;
                    const auto action=ControllerAction(i);
                    if((action==ControllerAction::previous||action==ControllerAction::next)&&(!browse||!o.controllerBumpers))continue;
                    owned_[n]=true;
                    if(action==ControllerAction::rightHand||action==ControllerAction::leftHand)return {true,action==ControllerAction::rightHand?PadAction::rightHand:PadAction::leftHand};
                    if(action==ControllerAction::cancel||action==ControllerAction::unequip){cancel();return {true,action==ControllerAction::cancel?PadAction::cancel:PadAction::unequip};}
                    return {true,action==ControllerAction::previous?PadAction::previous:PadAction::next};
                }
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

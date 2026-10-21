#pragma once
#include <optional>

namespace quickslot {

class VRPanelFocus {
    bool previousDesired_=false;
    unsigned settleFrames_=0;
public:
    std::optional<bool> update(bool& desired,bool focused) {
        if(desired!=previousDesired_) {
            previousDesired_=desired;settleFrames_=2;return desired;
        }
        if(settleFrames_&&--settleFrames_)return {};
        desired=focused;previousDesired_=desired;return {};
    }
};

class VRFocus {
    bool previousOpen_=false,pending_=false;
public:
    bool desired=false;
    void update(bool actual,std::optional<bool> reply={}) {
        if(reply){desired=*reply;pending_=false;}
        if(actual!=previousOpen_){desired=actual;previousOpen_=actual;}
    }
    std::optional<bool> request(bool actual) {
        if(desired==actual||pending_)return {};
        pending_=true;return desired;
    }
};
}

#pragma once
#include <optional>

namespace quickslot {
// The helper snapshots focus before dispatching its clients in unspecified
// order. Our HUD callback can therefore see the editor SDK's previous frame.
// Allow two dispatches after a local request before mirroring helper focus.
// Requests themselves are synchronous; no time-based retry can steal focus
// back from another overlay after the user deliberately switches away.
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
// Rendering requests a game-thread editor transition. Keep that request alive
// across frames until the task acknowledges it; a snapshot is not an ack.
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

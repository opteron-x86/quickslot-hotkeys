#pragma once
#include <optional>

namespace quickslot {
class HudMenuState {
    std::optional<bool> pending_;
public:
    std::optional<bool> request(bool visible,bool onStack) {
        if(pending_||visible==onStack)return {};
        pending_=visible;return pending_;
    }
    void processed(){pending_.reset();}
};
}

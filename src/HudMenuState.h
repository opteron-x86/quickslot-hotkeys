#pragma once
#include <optional>

namespace quickslot {
class HudMenuState {
    std::optional<bool> pending_;
public:
    std::optional<bool> request(bool visible,bool onStack,bool consoleOpen=false) {
        if(consoleOpen||pending_||visible==onStack)return {};
        pending_=visible;return pending_;
    }
    bool accept(bool consoleOpen) {
        if(consoleOpen)pending_.reset();
        return !consoleOpen;
    }
    void processed(){pending_.reset();}
};
}

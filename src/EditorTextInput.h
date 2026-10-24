#pragma once
#include <cstdint>
#include <limits>

namespace quickslot {

class EditorTextInput {
    bool owned_=false,wanted_=false;
    std::uint64_t session_=0;
public:
    enum class Change {none,acquire,restore,release};
    void beginSession(std::uint64_t session){session_=session;wanted_=false;}
    bool focus(std::uint64_t session,bool wanted) {
        if(!session_||session!=session_||wanted_==wanted)return false;
        wanted_=wanted;return true;
    }
    Change update(bool editorOpen,bool menuOpen,int count) {
        if(!editorOpen){session_=0;wanted_=false;}
        if(!editorOpen||!menuOpen||!wanted_||count<0) {
            const bool release=owned_&&count>0;
            owned_=false;
            return release?Change::release:Change::none;
        }
        if((owned_&&count>0)||count>=std::numeric_limits<std::int8_t>::max())return Change::none;
        const bool restoring=owned_;
        owned_=true;
        return restoring?Change::restore:Change::acquire;
    }
};
}

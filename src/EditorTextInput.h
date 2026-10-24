#pragma once
#include <cstdint>
#include <limits>

namespace quickslot {

class EditorTextInput {
    bool owned_=false;
public:
    enum class Change {none,acquire,restore,release};
    Change update(bool editorOpen,bool menuOpen,int count) {
        if(!editorOpen||!menuOpen||count<0) {
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

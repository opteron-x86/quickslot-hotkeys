#pragma once
#include "IconPolicy.h"
#include <imgui.h>
#include <functional>

namespace quickslot {
struct ItemIcon {
    ImTextureID original=0,mask=0;
    std::optional<std::uint32_t> rgb;
};
using ItemIconProvider=std::function<std::optional<ItemIcon>(const Entry&)>;
inline ImU32 itemIconTint(const ItemIcon& icon,ImU32 tint,unsigned mode,bool unavailable) {
    if(mode==0&&!unavailable&&icon.rgb) {
        const auto rgb=*icon.rgb;
        return (tint&0xff000000u)|((rgb&0xff)<<16)|(rgb&0xff00)|((rgb>>16)&0xff);
    }
    return atlasTint(tint,mode,unavailable);
}
inline ImTextureID itemIconTexture(const ItemIcon& icon,unsigned mode,bool unavailable) {
    return mode==0&&!unavailable&&!icon.rgb?icon.original:icon.mask;
}
}

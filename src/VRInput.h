#pragma once
#include "ControllerTypes.h"
#include <cstdint>

namespace quickslot {
constexpr bool vrDevice(unsigned device){return device>=3&&device<=8;}

constexpr bool vrMainHand(unsigned device,bool leftHanded){return vrDevice(device)&&((device%2!=0)!=leftHanded);}
constexpr PadButton vrButton(unsigned device,unsigned code,bool leftHanded=false) {
    if(!vrDevice(device))return PadButton::none;
    const bool main=vrMainHand(device,leftHanded);
    switch(code) {
    case 1:return main?PadButton::b:PadButton::y;
    case 7:return main?PadButton::a:PadButton::x;
    case 2:case 34:return main?PadButton::rb:PadButton::lb;
    case 33:return main?PadButton::rt:PadButton::lt;
    case 32:return main?PadButton::rs:PadButton::ls;
    case 35:case 36:return main?PadButton::start:PadButton::back;
    default:return PadButton::none;
    }
}
constexpr bool validVRButton(PadButton button){return button==PadButton::none||(button>=PadButton::a&&button<PadButton::count);}
inline PadState vrPhysicalState(std::uint64_t right,std::uint64_t left,bool leftHanded=false) {
    PadState out{};
    for(const auto device:{3u,4u})for(const auto key:{1u,2u,7u,32u,33u,34u,35u,36u})
        if((device==3?right:left)&(std::uint64_t(1)<<key))out[unsigned(vrButton(device,key,leftHanded))]=true;
    return out;
}
}

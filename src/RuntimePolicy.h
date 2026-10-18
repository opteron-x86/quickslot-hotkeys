#pragma once
#include <cstddef>

namespace quickslot {
struct RuntimeVersion {
    unsigned major,minor,patch,build=0;
    constexpr bool vr()const{return major==1&&minor==4&&patch==15&&build==0;}
    constexpr bool supported()const {
        return vr()||(major==1&&((minor==5&&patch==97)||(minor==6&&patch>=317)||(minor==7&&patch>=99)));
    }
    constexpr std::size_t extraDataSize()const{return major==1&&(minor>6||(minor==6&&patch>=629))?0x20:0x18;}
    constexpr std::size_t pickupSlot()const{return vr()?0xce:0xcc;}
    constexpr std::size_t inputCallOffset()const{return vr()?0x81:0x7b;}
};
template<class Version> constexpr RuntimeVersion runtimeVersion(const Version& v) {
    return {v[0],v[1],v[2],v[3]};
}
}

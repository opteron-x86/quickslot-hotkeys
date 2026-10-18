#pragma once
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

namespace quickslot {
struct CallSite {std::size_t size;std::uintptr_t target;};
template<class ReadPointer>
std::optional<CallSite> decodeCall(std::uintptr_t address,std::span<const std::uint8_t> bytes,ReadPointer readPointer) {
    std::int32_t displacement=0;
    if(bytes.size()>=5&&bytes[0]==0xe8) {
        std::memcpy(&displacement,bytes.data()+1,4);
        return CallSite{5,address+5+displacement};
    }
    if(bytes.size()>=6&&bytes[0]==0xff&&bytes[1]==0x15) {
        std::memcpy(&displacement,bytes.data()+2,4);
        if(auto target=readPointer(address+6+displacement);target&&*target)return CallSite{6,*target};
    }
    return {};
}
}

#pragma once
#include "ItemIdentity.h"
#include <limits>

namespace quickslot {
struct StackIdentity {
    std::uintptr_t token=0;
    int count=1;
    bool hasID=false;
    std::uint32_t owner=0;
    std::uint16_t unique=0;
    bool operator==(const StackIdentity&)const=default;
};
struct InventorySample {
    int count=0;
    std::vector<StackIdentity> stacks;
    int plain()const{int result=count;for(const auto& stack:stacks)result-=stack.count;return result;}
};
struct TransferTicket {IdentityChange identity;InventorySample before;};
struct TransferMatch {bool valid=false,plain=false;std::uintptr_t token=0;};

inline TransferMatch matchIncomingCopy(const InventorySample& before,const InventorySample& after,int requested=1) {
    if(requested!=1||before.count<0||before.count==std::numeric_limits<int>::max()||after.count!=before.count+1||before.plain()<0||after.plain()<0)return {};
    for(const auto& old:before.stacks) {
        const auto it=std::find_if(after.stacks.begin(),after.stacks.end(),[&](const auto& s){return s.token==old.token;});
        if(it==after.stacks.end()||*it!=old)return {};
    }
    const StackIdentity* incoming=nullptr;
    for(const auto& stack:after.stacks) {
        if(std::any_of(before.stacks.begin(),before.stacks.end(),[&](const auto& s){return s.token==stack.token;}))continue;
        if(incoming||!stack.token||stack.count!=1)return {};
        incoming=&stack;
    }
    if(incoming&&before.plain()==after.plain())return {true,false,incoming->token};
    if(!incoming&&after.plain()==before.plain()+1)return {true,true,0};
    return {};
}
}

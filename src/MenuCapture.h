#pragma once
#include "Transfer.h"
namespace quickslot {
inline TransferMatch matchMenuCopy(const InventorySample& live,const std::vector<std::uintptr_t>& row) {
    if(live.count<=0)return {};
    if(row.empty())return live.plain()>0?TransferMatch{true,true,0}:TransferMatch{};
    if(row.size()!=1||!row[0])return {};
    const auto it=std::find_if(live.stacks.begin(),live.stacks.end(),[&](const auto& stack){return stack.token==row[0];});
    if(it==live.stacks.end()||it->count!=1)return {};
    return {true,false,it->token};
}
}

#pragma once
#include "Model.h"
#include <bitset>
#include <cstdint>
#include <optional>

namespace quickslot {
using UsedItemIDs=std::bitset<65536>;

inline std::optional<std::uint16_t> allocateItemID(const UsedItemIDs& used,std::uint16_t start) {
    for(std::size_t attempt=0;attempt<used.size();++attempt) {
        const auto id=std::uint16_t(start+attempt);
        if(!used.test(id))return id;
    }
    return {};
}

struct IdentityChange {
    std::uint32_t form=0,oldOwner=0,newOwner=0;
    std::uint16_t oldID=0,newID=0;
};
inline std::size_t followIdentityChange(Group& group,const IdentityChange& change) {
    if(!change.form||!change.oldOwner||
       (change.oldOwner==change.newOwner&&change.oldID==change.newID))return 0;
    std::size_t count=0;
    for(auto& saved:group.entries)eachLeaf(saved,[&](auto& entry) {
        if(!tracksInstance(entry)||entry.unresolved||entry.form!=change.form||
           entry.owner!=change.oldOwner||entry.unique!=change.oldID)return;
        entry.owner=change.newOwner;entry.unique=change.newID;
        if(!change.newOwner)entry.unresolved=true;
        ++count;
    });
    if(count)++group.revision;
    return count;
}
inline std::size_t followIdentityChange(Arsenal& arsenal,const IdentityChange& change) {
    std::size_t count=0;for(auto& group:arsenal.groups)count+=followIdentityChange(group,change);return count;
}
}

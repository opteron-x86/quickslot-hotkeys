#include "Inventory.h"
#include "Game.h"
#include "RuntimePolicy.h"
#include <windows.h>

namespace quickslot {
InventorySample sampleInventory(RE::PlayerCharacter* player,RE::TESBoundObject* object) {
    InventorySample sample;
    auto inventory=player->GetInventory([&](RE::TESBoundObject& obj){return &obj==object;});
    const auto it=inventory.find(object);if(it==inventory.end())return sample;
    sample.count=it->second.first;
    if(it->second.second&&it->second.second->extraLists)for(auto* extra:*it->second.second->extraLists)if(extra) {
        const auto* id=extra->GetByType<RE::ExtraUniqueID>();
        sample.stacks.push_back({reinterpret_cast<std::uintptr_t>(extra),extra->GetCount(),id!=nullptr,id?id->baseID:0,id?id->uniqueID:std::uint16_t(0)});
    }
    return sample;
}
// Attach an extra-data list to one plain item using the runtime-specific allocation size.
RE::ExtraDataList* materializePlainCopy(RE::PlayerCharacter* player,RE::TESBoundObject* object) {
    if(sampleInventory(player,object).plain()<1)return nullptr;
    auto* changes=player->GetInventoryChanges();if(!changes)return nullptr;
    RE::InventoryEntryData* entry=nullptr;
    if(changes->entryList)for(auto* candidate:*changes->entryList)if(candidate&&candidate->object==object){entry=candidate;break;}

    using Initialize=RE::ExtraDataList*(RE::ExtraDataList*);
    static REL::Relocation<Initialize> initialize{RELOCATION_ID(11437,11583)};
    auto* memory=static_cast<RE::ExtraDataList*>(RE::malloc(runtimeVersion(REL::Module::get().version()).extraDataSize()));if(!memory)return nullptr;
    auto* extra=initialize(memory);
    if(!entry){entry=new RE::InventoryEntryData(object,0);changes->AddEntryData(entry);}

    entry->AddExtraList(extra);changes->changed=true;player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
    return extra;
}
// Assign an inventory ID while reserving IDs used by saved entries.
bool ensureIdentity(RE::PlayerCharacter* player,RE::ExtraDataList* extra,const Arsenal& saved,const Arsenal* draft) {
    if(!extra||extra->GetCount()!=1)return false;
    if(extra->GetByType<RE::ExtraUniqueID>())return true;
    auto* changes=player->GetInventoryChanges();if(!changes)return false;
    const auto owner=player->GetFormID();UsedItemIDs used;
    if(changes->entryList)for(auto* entry:*changes->entryList)if(entry&&entry->extraLists)
        for(auto* list:*entry->extraLists)if(list)if(const auto* id=list->GetByType<RE::ExtraUniqueID>();id&&id->baseID==owner)used.set(id->uniqueID);
    const auto reserve=[&](const Arsenal& a){eachSavedEntry(a,[&](const auto& saved){eachLeaf(saved,[&](const Entry& e){if(tracksInstance(e)&&!e.unresolved&&e.owner==owner)used.set(e.unique);});});};
    reserve(saved);if(draft)reserve(*draft);
    const auto next=allocateItemID(used,changes->GetNextUniqueID());if(!next)return false;
    extra->Add(new RE::ExtraUniqueID(owner,*next));changes->changed=true;player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
    const auto* id=extra->GetByType<RE::ExtraUniqueID>();return id&&id->baseID==owner&&id->uniqueID==*next;
}
namespace {
using Pickup=void(RE::PlayerCharacter*,RE::TESObjectREFR*,std::int32_t,bool,bool);
using Add=void(RE::PlayerCharacter*,RE::TESBoundObject*,RE::ExtraDataList*,std::int32_t,RE::TESObjectREFR*);
REL::Relocation<Pickup> originalPickup;
REL::Relocation<Add> originalAdd;
thread_local unsigned transferDepth=0;
struct Depth {bool outer=transferDepth++==0;~Depth(){--transferDepth;}};
void pickup(RE::PlayerCharacter* player,RE::TESObjectREFR* source,std::int32_t count,bool arg,bool sound) {
    Depth depth;std::optional<TransferTicket> ticket;auto* object=source?source->GetBaseObject():nullptr;
    if(depth.outer&&source&&object&&count==1)try{ticket=Game::get().beforeTransfer(player,object,source,&source->extraList);}catch(const std::exception& e){SKSE::log::error("Pickup inspection failed: {}",e.what());}
    originalPickup(player,source,count,arg,sound);
    if(ticket)try{Game::get().afterTransfer(player,object,*ticket);}catch(const std::exception& e){SKSE::log::error("Pickup recovery failed: {}",e.what());}
}
void add(RE::PlayerCharacter* player,RE::TESBoundObject* object,RE::ExtraDataList* extra,std::int32_t count,RE::TESObjectREFR* source) {
    Depth depth;std::optional<TransferTicket> ticket;
    if(depth.outer&&object&&count==1)try{ticket=Game::get().beforeTransfer(player,object,source,extra);}catch(const std::exception& e){SKSE::log::error("Transfer inspection failed: {}",e.what());}
    originalAdd(player,object,extra,count,source);
    if(ticket)try{Game::get().afterTransfer(player,object,*ticket);}catch(const std::exception& e){SKSE::log::error("Transfer recovery failed: {}",e.what());}
}
bool executable(std::uintptr_t address) {
    MEMORY_BASIC_INFORMATION region{};
    return VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))&&region.State==MEM_COMMIT&&
        !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
}
bool installInventoryHooks() {
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
    const auto* slots=reinterpret_cast<const std::uintptr_t*>(table.address());
    const auto pickupSlot=runtimeVersion(REL::Module::get().version()).pickupSlot();
    if(!executable(slots[pickupSlot])||!executable(slots[0x5a]))return false;

    originalPickup=table.write_vfunc(pickupSlot,pickup);originalAdd=table.write_vfunc(0x5a,add);
    return true;
}
}

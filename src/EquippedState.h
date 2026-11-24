#pragma once
#include "Model.h"

namespace quickslot {
inline std::size_t selectedConsumableGroup(const Arsenal& a) {
    if(a.rememberedConsumable)return a.consumableGroup;
    const auto valid=[&](std::size_t g){return g<Arsenal::count&&a.groups[g].selected<a.groups[g].entries.size()&&isConsumable(a.groups[g].entries[a.groups[g].selected]);};
    if(valid(a.consumableGroup))return a.consumableGroup;
    if(valid(a.active))return a.active;
    for(std::size_t g=0;g<Arsenal::count;++g)if(valid(g))return g;
    return Arsenal::count;
}
inline void rememberConsumable(Arsenal& a,std::size_t group,std::size_t index) {
    if(group<Arsenal::count&&index<a.groups[group].entries.size()&&isConsumable(a.groups[group].entries[index])){a.consumableGroup=group;a.rememberedConsumable=a.groups[group].entries[index];}
}
inline unsigned entryHands(const Entry& e,bool equipped) {
    if(!equipped)return 0;
    if(e.kind==Kind::combo){unsigned mask=0;for(const auto& p:e.parts)mask|=entryHands(p,true);return mask;}
    if(e.kind==Kind::unarmed||e.twoHanded)return 3;
    if(isVoice(e)||isConsumable(e)||wornGear(e)||e.kind==Kind::ammo||e.kind==Kind::smartAmmo)return 0;
    return e.hand==Hand::left?2:1;
}
inline bool twoHandedDisplay(const EquippedDisplay& state){return state.hands[0]&&state.hands[0]->entry.twoHanded;}
inline constexpr const char* equipmentRoleName(EquipmentRole role,bool twoHanded=false) {
    switch(role) {
    case EquipmentRole::rightHand:return twoHanded?"Both hands":"Right hand";
    case EquipmentRole::leftHand:return "Left hand";
    case EquipmentRole::ammo:return "Ammunition";
    case EquipmentRole::consumable:return "Consumable";
    case EquipmentRole::voice:return "Power / shout";
    default:return "Ready spell";
    }
}
template<class Draw> void forEachEquipmentRole(const EquippedDisplay& state,const Options& options,Draw draw,bool includeWhenEquipped=false) {
    const auto emit=[&](EquipmentRole role,const std::optional<Row>& row) {
        const unsigned mode=options.equipmentRoles[unsigned(role)];
        if(mode==2||(mode==1&&!row&&!includeWhenEquipped))return;
        draw(role,equipmentRoleName(role,twoHandedDisplay(state)),row);
    };
    emit(EquipmentRole::rightHand,state.hands[0]);
    if(!twoHandedDisplay(state))emit(EquipmentRole::leftHand,state.hands[1]);
    if(options.showEquipmentAmmo)emit(EquipmentRole::ammo,state.ammo);
    emit(EquipmentRole::consumable,state.consumable);
    emit(EquipmentRole::voice,state.voice);
    if(state.castsReady)emit(EquipmentRole::readySpell,state.readySpell);
}
template<class Draw> void forEachEquipmentRow(const EquippedDisplay& state,const Options& options,Draw draw,bool includeWhenEquipped=false) {
    forEachEquipmentRole(state,options,[&](EquipmentRole,const char* label,const std::optional<Row>& row){draw(label,row);},includeWhenEquipped);
}
inline unsigned equipmentRows(const EquippedDisplay& state,const Options& options={},bool includeWhenEquipped=false) {
    unsigned n=0;
    forEachEquipmentRole(state,options,[&](EquipmentRole,const char*,const std::optional<Row>&){++n;},includeWhenEquipped);
    return n;
}
inline std::array<EquipmentRole,6> activeEquipmentRoles(const EquippedDisplay& state,const Options& options={},bool includeWhenEquipped=false) {
    std::array<EquipmentRole,6> out{EquipmentRole::rightHand,EquipmentRole::leftHand,EquipmentRole::consumable,EquipmentRole::voice,EquipmentRole::ammo,EquipmentRole::readySpell};
    unsigned n=0;
    forEachEquipmentRole(state,options,[&](EquipmentRole role,const char*,const std::optional<Row>&){if(n<out.size())out[n++]=role;},includeWhenEquipped);
    return out;
}
inline bool equipmentDimmed(const Row& row){return !row.available||row.powerUsed||row.cooldown>0;}
inline int compatibleAmmoCount(bool crossbow,std::optional<bool> bolt,int count) {
    return bolt&&*bolt==crossbow?std::max(0,count):0;
}
inline int badgeQuantity(const Entry& entry,int quantity,const WeaponStatus& weapon,const Options& options) {
    return isStack(entry)?std::max(0,quantity):options.showAmmo?weapon.ammunition:-1;
}
inline std::string equipmentTextName(const std::optional<Row>& row,bool iconVisible,const Options& options={}) {
    if(!row)return "—";
    auto name=displayName(row->entry);
    if(!iconVisible) {
        if(isStack(row->entry))name+=" ×"+std::to_string(std::max(0,row->quantity));
        else if(options.showAmmo&&row->weapon.ammunition>=0)name+=" ×"+std::to_string(row->weapon.ammunition);
        else if(row->cooldown>0)name+=" · "+std::to_string(int(std::ceil(row->cooldown)))+tr("s");
        if(options.showPoison&&row->weapon.poisoned)name+=" · "+std::string(tr("Poisoned"));
    }
    return name;
}
inline bool interacting(const Snapshot& state,double now){return state.holding||state.browsing||now<state.revealUntil;}
inline bool showGroupDisplay(const Snapshot& state,const Options& o,double now) {
    return !state.selectorOnly&&!state.equipmentOnly&&(state.editorPreview||interacting(state,now)||o.showIdle);
}
inline bool showEquipmentDisplay(const Snapshot& state,const Options& o,double) {
    return o.showEquipment&&!state.selectorOnly&&(!state.editorPreview||state.placementPreview||state.equipmentOnly);
}
inline float groupVisibility(const Snapshot& state,const Options& o,std::size_t id,double now) {
    const auto& tile=state.groups[id];
    const bool reveal=state.placementPreview||(id==state.activeGroup&&(state.holding||state.browsing||now<state.revealUntil));
    if(reveal)return 1;
    if(o.hideEmptyGroups&&!tile.count)return 0;
    const bool active=tile.equipped||tile.voice||tile.consumable;
    if(active||!o.inactiveGroups)return 1;
    return o.inactiveGroups==1?o.inactiveGroupOpacity:0;
}
inline std::string handMarker(unsigned hands) {return hands==3?tr("L+R"):hands==2?tr("L"):hands==1?tr("R"):"";}
}

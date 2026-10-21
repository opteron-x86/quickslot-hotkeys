#pragma once
#include "Model.h"
#include <tuple>

namespace quickslot {
inline constexpr const char* weaponFamilyNames[]{"One-handed","Two-handed","Sword","Dagger","War axe","Mace","Greatsword","Battleaxe","Warhammer","Bow","Crossbow","Shield"};
inline Icon smartEquipmentIcon(const Entry& e) {
    if(e.kind==Kind::smartAmmo)return e.equipmentType?Icon::bolt:Icon::arrow;
    constexpr Icon icons[]{Icon::sword,Icon::greatsword,Icon::sword,Icon::dagger,Icon::axe,Icon::mace,Icon::greatsword,Icon::battleaxe,Icon::warhammer,Icon::bow,Icon::crossbow,Icon::shield};
    return icons[std::min(e.equipmentType,unsigned(WeaponFamily::count)-1)];
}
inline std::string smartEquipmentName(const Entry& e) {
    return std::string("Best ")+(e.kind==Kind::smartAmmo?(e.equipmentType?"bolts":"arrows"):weaponFamilyNames[std::min(e.equipmentType,unsigned(WeaponFamily::count)-1)]);
}
inline Entry smartEquipmentEntry(bool ammo,unsigned type=0,Hand hand=Hand::right) {
    Entry e;e.kind=ammo?Kind::smartAmmo:Kind::smartWeapon;e.equipmentType=std::min(type,ammo?1u:unsigned(WeaponFamily::count)-1);
    e.twoHanded=!ammo&&smartTwoHanded(e.equipmentType);e.hand=normalizedHand(e,hand);
    e.name=smartEquipmentName(e);e.icon=smartEquipmentIcon(e);return e;
}
struct EquipmentCandidate {
    Entry entry;
    WeaponFamily family=WeaponFamily::sword;
    float damage=0;
    int count=0;
    bool rightEquipped=false,leftEquipped=false;
};
inline bool equipmentMatches(const Entry& rule,const EquipmentCandidate& c) {
    if(!validSmartEquipment(rule)||c.count<=0||c.entry.unresolved||!c.entry.form||!std::isfinite(c.damage)||c.damage<=0)return false;
    if(rule.kind==Kind::smartAmmo)return c.entry.kind==Kind::ammo&&((c.entry.icon==Icon::bolt)==(rule.equipmentType==1));
    if(c.entry.kind==Kind::ammo||unsigned(c.family)>=unsigned(WeaponFamily::count))return false;
    const auto type=WeaponFamily(rule.equipmentType);
    if(type==WeaponFamily::oneHanded)return c.family>=WeaponFamily::sword&&c.family<=WeaponFamily::mace;
    if(type==WeaponFamily::twoHanded)return c.family>=WeaponFamily::greatsword&&c.family<=WeaponFamily::warhammer;
    return type==c.family;
}
inline bool preferredEquipment(const Entry& rule,const EquipmentCandidate& a,const EquipmentCandidate& b) {
    const auto tie=[&](const EquipmentCandidate& c){
        const bool equipped=rule.hand==Hand::left?c.leftEquipped:c.rightEquipped;
        const bool other=rule.hand==Hand::left?c.rightEquipped:c.leftEquipped;
        return std::tuple{!equipped,other,c.entry.form,c.entry.owner,c.entry.unique};
    };
    return a.damage>b.damage||(a.damage==b.damage&&tie(a)<tie(b));
}
inline const EquipmentCandidate* chooseEquipment(const Entry& rule,const std::vector<EquipmentCandidate>& candidates) {
    const EquipmentCandidate* best=nullptr;
    for(const auto& c:candidates)if(equipmentMatches(rule,c))
        if(!best||preferredEquipment(rule,c,*best))best=&c;
    return best;
}
inline bool hasSmartEquipment(const Entry& entry) {
    bool smart=false;eachLeaf(entry,[&](const Entry& part){smart|=isSmartEquipment(part);});return smart;
}
inline bool sameEquipmentCopy(const Entry& a,const Entry& b) {
    return tracksInstance(a)&&tracksInstance(b)&&a.form==b.form&&a.owner==b.owner&&a.unique==b.unique;
}
inline constexpr std::size_t noEquipmentCandidate=std::size_t(-1);
using CombinationChoices=std::array<std::size_t,2>;

// Choose the highest-scoring pair with a separate physical copy for each hand.
inline std::optional<CombinationChoices> chooseCombination(const Entry& combo,const std::vector<EquipmentCandidate>& candidates) {
    if(combo.kind!=Kind::combo||combo.parts.size()!=2||!validPair(combo.parts[0],combo.parts[1]))return {};
    std::array<std::vector<std::size_t>,2> choices;
    for(unsigned hand=0;hand<2;++hand) {
        const auto& rule=combo.parts[hand];const auto& other=combo.parts[1-hand];
        if(!isSmartEquipment(rule)){choices[hand].push_back(noEquipmentCandidate);continue;}
        for(std::size_t i=0;i<candidates.size();++i) {
            const auto& c=candidates[i];
            if(!equipmentMatches(rule,c)||(!isSmartEquipment(other)&&sameEquipmentCopy(c.entry,other)))continue;
            auto& ranked=choices[hand];const auto at=std::find_if(ranked.begin(),ranked.end(),[&](auto j){return preferredEquipment(rule,c,candidates[j]);});
            ranked.insert(at,i);if(ranked.size()>2)ranked.pop_back();
        }
        if(choices[hand].empty())return {};
    }
    std::optional<CombinationChoices> best;double bestScore=-1;
    for(auto right:choices[0])for(auto left:choices[1]) {
        if(right!=noEquipmentCandidate&&left!=noEquipmentCandidate) {
            if(right==left) {if(candidates[right].entry.unique||candidates[right].count<2)continue;}
            else if(candidates[right].entry.unique&&sameEquipmentCopy(candidates[right].entry,candidates[left].entry))continue;
        }
        const double score=(right==noEquipmentCandidate?0.:candidates[right].damage)+(left==noEquipmentCandidate?0.:candidates[left].damage);

        if(!best||score>bestScore){best=CombinationChoices{right,left};bestScore=score;}
    }
    return best;
}
}

#pragma once
#include "Model.h"
namespace quickslot {
inline bool wornForEntry(const Entry& entry,bool worn,bool wornLeft) {
    if(entry.kind==Kind::shield||entry.kind==Kind::torch||entry.twoHanded)return worn||wornLeft;
    return entry.hand==Hand::left?wornLeft:worn;
}

inline std::optional<Hand> objectEquipHand(const Entry& entry,bool remove,bool otherHand=false) {
    if(entry.kind==Kind::torch||entry.kind==Kind::ammo)return {};
    if(entry.twoHanded)return remove?std::optional{Hand::right}:std::nullopt;
    return otherHand?(entry.hand==Hand::left?Hand::right:Hand::left):entry.hand;
}
struct HandOccupancy {bool occupied=false,twoHanded=false;};

inline std::optional<Hand> handClearTarget(const std::array<HandOccupancy,2>& hands,Hand hand) {
    const auto index=std::size_t(hand),other=1-index;
    if(hands[index].occupied)return hand;
    if(hands[other].occupied&&hands[other].twoHanded)return Hand(other);
    return {};
}
inline bool skipEquipmentStep(const Entry& entry,bool remove,bool equipped) {
    return remove?(isHandAction(entry)||!equipped):equipped;
}
enum class EquipmentPhase { equipping, unequipping, movingOut };
enum class EquipmentProgress { wait, complete, equipAfterMove, cancelled, unavailable, timeout };

inline EquipmentProgress equipmentProgress(EquipmentPhase phase,bool valid,bool available,bool equipped,bool otherEquipped,bool expired) {
    if(!valid)return EquipmentProgress::cancelled;
    if(!available)return EquipmentProgress::unavailable;
    if(phase==EquipmentPhase::unequipping&&!equipped)return EquipmentProgress::complete;
    if(phase!=EquipmentPhase::unequipping&&equipped)return EquipmentProgress::complete;
    if(expired)return EquipmentProgress::timeout;
    if(phase==EquipmentPhase::movingOut&&!otherEquipped)return EquipmentProgress::equipAfterMove;
    return EquipmentProgress::wait;
}
}

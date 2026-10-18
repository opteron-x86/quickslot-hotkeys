#pragma once
#include "Model.h"
namespace quickslot {
struct HandOccupancy {bool occupied=false,twoHanded=false;};
// A two-handed item can be reported in just one engine slot, but blocks both.
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
// Engine mutations stay in Game.cpp. This policy enforces cancellation and
// confirmation before the second half of a cross-hand move can execute.
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

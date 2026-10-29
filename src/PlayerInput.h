#pragma once
#include <RE/Skyrim.h>

namespace quickslot {
inline bool playerAIControlled(const RE::PlayerCharacter* player) {
    // SetAIDriven owns this flag independently of ControlMap's movement toggle.
    return player&&player->GetPlayerFlags().aiControlledPackage;
}
}

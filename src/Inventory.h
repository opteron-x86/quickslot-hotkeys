#pragma once
#include "Transfer.h"
#include <RE/Skyrim.h>

namespace quickslot {
InventorySample sampleInventory(RE::PlayerCharacter*,RE::TESBoundObject*);
RE::ExtraDataList* materializePlainCopy(RE::PlayerCharacter*,RE::TESBoundObject*);
bool ensureIdentity(RE::PlayerCharacter*,RE::ExtraDataList*,const Arsenal&,const Arsenal* draft=nullptr);
bool installInventoryHooks();
}

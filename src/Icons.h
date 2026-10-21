#pragma once
#include <cstdint>

namespace quickslot {
enum class Icon : unsigned {
    automatic,
    sword, greatsword, dagger, axe, mace, bow, staff, shield,
    torch, spell, battleaxe, warhammer, crossbow, alteration, conjuration, destruction,
    illusion, restoration, fire, frost, shock, potion, health, magicka,
    stamina, food, poison, scroll, power, shout, arrow, bolt,
    unarmed, shortsword, rapier, saber, scimitar, falchion, katana, nodachi,
    flamberge, stiletto, tanto, hatchet, morningstar, club, spiked_club, maul,
    spear, shortspear, pike, halberd, glaive, naginata, trident, scythe,
    pickaxe, woodcutters_axe, javelin, quarterstaff, claw, whip, gun,
    count
};
inline constexpr unsigned iconColumns=8,iconRows=8,iconCell=256;
inline constexpr unsigned iconCount=unsigned(Icon::count)-1;
static_assert(iconCount<=iconColumns*iconRows);
inline constexpr char itemAtlasPath[]="Quickslot/icons/items.png";
}

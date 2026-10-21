#pragma once
#include "SmartEquipment.h"
#include <optional>

namespace quickslot {
inline Icon baseIcon(const Entry& e) {
    if(e.icon>Icon::automatic&&e.icon<Icon::count)return e.icon;
    if(isSmartEquipment(e))return smartEquipmentIcon(e);
    constexpr Icon kinds[]{Icon::sword,Icon::greatsword,Icon::dagger,Icon::axe,Icon::mace,
        Icon::bow,Icon::staff,Icon::shield,Icon::torch,Icon::spell,Icon::potion,Icon::food,
        Icon::poison,Icon::scroll,Icon::greater_power,Icon::lesser_power,Icon::shout};
    if(e.kind==Kind::smartPotion)return e.resource==1?Icon::magicka:e.resource==2?Icon::stamina:Icon::health;
    if(e.kind==Kind::ammo)return Icon::arrow;
    if(isHandAction(e))return Icon::unarmed;
    return unsigned(e.kind)<std::size(kinds)?kinds[unsigned(e.kind)]:Icon::spell;
}

struct AtlasRegion {float u0,v0,u1,v1;};
inline std::optional<AtlasRegion> atlasRegion(Icon icon) {
    if(icon<=Icon::automatic||icon>=Icon::count)return {};
    const auto cell=unsigned(icon)-1;
    const float u=float(cell%iconColumns)/iconColumns,v=float(cell/iconColumns)/iconRows;
    return AtlasRegion{u,v,u+1.f/iconColumns,v+1.f/iconRows};
}
inline std::uint32_t atlasTint(std::uint32_t themeColor,unsigned mode,bool unavailable) {
    return mode==1||unavailable?themeColor:(themeColor&0xff000000u)|0x00ffffffu;
}
}

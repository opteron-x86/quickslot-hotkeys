#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace quickslot {
enum class MenuPurpose {input,hud};
inline constexpr std::string_view quickLootMenuName="LootMenu";

inline constexpr std::array<std::string_view,23> gameplayBlockingMenus{
    "TweenMenu","InventoryMenu","MagicMenu","FavoritesMenu","MapMenu","StatsMenu","Journal Menu",
    "Book Menu","ContainerMenu","BarterMenu","GiftMenu","Crafting Menu","Sleep/Wait Menu",
    "Lockpicking Menu","Training Menu","MessageBoxMenu","Main Menu","Loading Menu","Dialogue Menu",
    "Console","LevelUp Menu","RaceSex Menu",
    quickLootMenuName
};
template<class Open> bool gameplayMenuOpen(const Open& open,MenuPurpose purpose=MenuPurpose::input) {

    return std::any_of(gameplayBlockingMenus.begin(),gameplayBlockingMenus.end(),[&](auto name){
        return !(purpose==MenuPurpose::hud&&name==quickLootMenuName)&&open(name);
    });
}
}

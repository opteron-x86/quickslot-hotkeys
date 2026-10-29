#pragma once
#include <algorithm>
#include <array>
#include <string_view>

namespace quickslot {
enum class MenuPurpose {input,hud,editor,capture};
inline constexpr std::string_view quickLootMenuName="LootMenu";
inline bool ignoreQuickLoot(MenuPurpose purpose,bool allowInput=false) {
    return purpose==MenuPurpose::hud||(purpose==MenuPurpose::input&&allowInput);
}
inline bool ignoreMenu(std::string_view name,MenuPurpose purpose,bool allowQuickLoot=false) {
    return (purpose==MenuPurpose::capture&&(name=="InventoryMenu"||name=="MagicMenu"))||
        (name==quickLootMenuName&&ignoreQuickLoot(purpose,allowQuickLoot));
}

inline bool menuFlagsBlock(bool usesContext,bool usesCursor,bool pauses,bool modal,MenuPurpose purpose=MenuPurpose::input) {
    // Text-entry helpers can request a cursor without taking over the editor.
    // Gameplay input and the passive HUD still yield to all cursor menus.
    return usesContext||pauses||modal||(purpose!=MenuPurpose::editor&&usesCursor);
}

inline constexpr std::array<std::string_view,24> gameplayBlockingMenus{
    "TweenMenu","InventoryMenu","MagicMenu","FavoritesMenu","MapMenu","StatsMenu","Journal Menu",
    "Book Menu","ContainerMenu","BarterMenu","GiftMenu","Crafting Menu","Sleep/Wait Menu",
    "Lockpicking Menu","Training Menu","MessageBoxMenu","Main Menu","Loading Menu","Dialogue Menu",
    "Console","LevelUp Menu","RaceSex Menu","CustomMenu",
    quickLootMenuName
};
template<class Open> bool gameplayMenuOpen(const Open& open,MenuPurpose purpose=MenuPurpose::input,bool allowQuickLoot=false) {

    return std::any_of(gameplayBlockingMenus.begin(),gameplayBlockingMenus.end(),[&](auto name){
        return !ignoreMenu(name,purpose,allowQuickLoot)&&open(name);
    });
}
}

#pragma once
#include "ControllerTypes.h"
#include <array>

namespace quickslot {
enum class GroupModifier : unsigned {captureRight,captureLeft,move,unequip,remove,reorder,count};
enum class KeyboardAction : unsigned {previous,next,unequip,remove,cancel,count};
enum class ControllerAction : unsigned {cancel,unequip,previous,next,count};
struct KeyBinding {
    unsigned key=0,modifiers=0;
    bool operator==(const KeyBinding&)const=default;
};
inline constexpr std::array<unsigned,unsigned(GroupModifier::count)> defaultGroupModifiers{2,3,2,1,4,2};
inline constexpr std::array<KeyBinding,unsigned(KeyboardAction::count)> defaultSelectorBindings{{{203},{205},{14},{211},{1}}};
inline constexpr std::array<PadBinding,unsigned(ControllerAction::count)> defaultControllerActions{{{PadButton::b},{PadButton::x},{PadButton::lb},{PadButton::rb}}};
inline bool keyBindingMatches(KeyBinding binding,unsigned key,unsigned modifiers) {
    return binding.key&&binding.key==key&&(!binding.modifiers||binding.modifiers==modifiers);
}
}

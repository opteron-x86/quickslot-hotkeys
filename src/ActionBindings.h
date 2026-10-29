#pragma once
#include "ControllerTypes.h"
#include <array>

namespace quickslot {
enum class GroupModifier : unsigned {captureRight,captureLeft,move,unequip,remove,reorder,count};
enum class KeyboardAction : unsigned {previous,next,unequip,remove,cancel,rightHand,leftHand,count};
enum class ControllerAction : unsigned {cancel,unequip,previous,next,rightHand,leftHand,count};
enum class MouseAction : unsigned {cancel,rightHand,leftHand,count};
struct KeyBinding {
    unsigned key=0,modifiers=0;
    bool operator==(const KeyBinding&)const=default;
};
inline constexpr std::array<unsigned,unsigned(GroupModifier::count)> defaultGroupModifiers{2,3,2,1,4,2};
inline constexpr std::array<KeyBinding,unsigned(KeyboardAction::count)> defaultSelectorBindings{{{203},{205},{14},{211},{1}}};
inline constexpr std::array<PadBinding,unsigned(ControllerAction::count)> defaultControllerActions{{{PadButton::b},{PadButton::x},{PadButton::lb},{PadButton::rb}}};
inline constexpr std::array<int,unsigned(MouseAction::count)> defaultMouseActions{1,-1,-1};
inline const char* mouseButtonName(int button) {
    constexpr const char* names[]{"Left mouse","Right mouse","Middle mouse","Mouse 4","Mouse 5"};
    return button>=0&&button<5?names[button]:"None";
}
inline bool keyBindingMatches(KeyBinding binding,unsigned key,unsigned modifiers) {
    return binding.key&&binding.key==key&&(!binding.modifiers||binding.modifiers==modifiers);
}
}

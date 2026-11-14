#pragma once
#include "Model.h"
#include <nlohmann/json.hpp>

namespace quickslot {
// One row for each adjustable number, choice and switch of the HUD's look: its range, step,
// unit and the Appearance page that owns it. The default is the member's value in Options{}.
// Sanitizing, the editor's rows and resets all read these tables. Rows marked stored are also
// read and written by key in the settings file, so a new look setting needs its Options member
// and one row here.
enum class Unit {plain,percent,pixels,seconds,count};
enum class Section {theme,widget,selector,equipment,general};
struct FloatSetting {const char* key;float Options::*value;float low,high,step;Unit unit;Section section;bool stored=false;};
struct ChoiceSetting {const char* key;unsigned Options::*value;unsigned limit;Section section;bool stored=false;};
struct FlagSetting {const char* key;bool Options::*value;Section section;bool stored=false;};

inline constexpr FloatSetting floatSettings[]{
    {"equippedMarkerScale",&Options::equippedMarkerScale,1,2.5f,.05f,Unit::percent,Section::theme},
    {"iconScale",&Options::iconScale,.5f,1.6f,.05f,Unit::percent,Section::theme,true},
    {"backgroundOpacity",&Options::backgroundOpacity,0,1,.05f,Unit::percent,Section::theme,true},
    {"scale",&Options::scale,.25f,3,.05f,Unit::percent,Section::widget},
    {"textScale",&Options::textScale,.5f,3,.05f,Unit::percent,Section::widget},
    {"opacity",&Options::opacity,.1f,1,.05f,Unit::percent,Section::widget},
    {"spacing",&Options::spacing,0,128,1,Unit::pixels,Section::widget},
    {"cardWidth",&Options::cardWidth,60,260,1,Unit::pixels,Section::widget,true},
    {"tileSize",&Options::tileSize,.6f,2,.05f,Unit::percent,Section::widget,true},
    {"gridColumns",&Options::gridColumns,1,8,1,Unit::count,Section::widget,true},
    {"offsetX",&Options::offsetX,-8000,8000,1,Unit::pixels,Section::widget,true},
    {"offsetY",&Options::offsetY,-8000,8000,1,Unit::pixels,Section::widget,true},
    {"positionX",&Options::positionX,0,1,.005f,Unit::plain,Section::widget},
    {"positionY",&Options::positionY,0,1,.005f,Unit::plain,Section::widget},
    {"fadeDelay",&Options::fadeDelay,0,20,.5f,Unit::seconds,Section::widget},
    {"idleOpacity",&Options::idleOpacity,0,1,.05f,Unit::percent,Section::widget},
    {"inactiveGroupOpacity",&Options::inactiveGroupOpacity,.05f,.9f,.05f,Unit::percent,Section::widget},
    {"selectorSpacing",&Options::selectorSpacing,0,128,1,Unit::pixels,Section::selector},
    {"selectorGap",&Options::selectorGap,0,100,1,Unit::pixels,Section::selector},
    {"wheelRadius",&Options::wheelRadius,40,240,1,Unit::pixels,Section::selector,true},
    {"selectorScale",&Options::selectorScale,.25f,3,.05f,Unit::percent,Section::selector,true},
    {"selectorTextScale",&Options::selectorTextScale,.5f,3,.05f,Unit::percent,Section::selector,true},
    {"selectorOpacity",&Options::selectorOpacity,.1f,1,.05f,Unit::percent,Section::selector,true},
    {"selectorOffsetX",&Options::selectorOffsetX,-8000,8000,1,Unit::pixels,Section::selector,true},
    {"selectorOffsetY",&Options::selectorOffsetY,-8000,8000,1,Unit::pixels,Section::selector,true},
    {"selectorX",&Options::selectorX,0,1,.005f,Unit::plain,Section::selector},
    {"selectorY",&Options::selectorY,0,1,.005f,Unit::plain,Section::selector},
    {"equipmentScale",&Options::equipmentScale,.25f,3,.05f,Unit::percent,Section::equipment},
    {"equipmentTextScale",&Options::equipmentTextScale,.5f,3,.05f,Unit::percent,Section::equipment},
    {"equipmentOpacity",&Options::equipmentOpacity,.1f,1,.05f,Unit::percent,Section::equipment,true},
    {"equipmentSpacing",&Options::equipmentSpacing,0,128,1,Unit::pixels,Section::equipment},
    {"equipmentOffsetX",&Options::equipmentOffsetX,-8000,8000,1,Unit::pixels,Section::equipment,true},
    {"equipmentOffsetY",&Options::equipmentOffsetY,-8000,8000,1,Unit::pixels,Section::equipment,true},
    {"equipmentX",&Options::equipmentX,0,1,.005f,Unit::plain,Section::equipment},
    {"equipmentY",&Options::equipmentY,0,1,.005f,Unit::plain,Section::equipment},
    {"equipmentFadeDelay",&Options::equipmentFadeDelay,0,20,.5f,Unit::seconds,Section::equipment},
    {"equipmentIdleOpacity",&Options::equipmentIdleOpacity,.05f,.9f,.05f,Unit::percent,Section::equipment},
};
inline constexpr ChoiceSetting choiceSettings[]{
    {"itemIconColor",&Options::itemIconColor,2,Section::theme},
    {"equippedHighlight",&Options::equippedHighlight,3,Section::theme,true},
    {"anchor",&Options::anchor,9,Section::widget,true},
    {"layout",&Options::layout,6,Section::widget},
    {"shape",&Options::shape,3,Section::widget},
    {"inactiveGroups",&Options::inactiveGroups,2,Section::widget},
    {"selectorAnchor",&Options::selectorAnchor,9,Section::selector,true},
    {"selector",&Options::selector,wheelSelector,Section::selector},
    {"selectorShape",&Options::selectorShape,3,Section::selector},
    {"selectorVisible",&Options::selectorVisible,3,Section::selector,true},
    {"wheelStyle",&Options::wheelStyle,1,Section::selector,true},
    {"selectorDock",&Options::selectorDock,4,Section::selector},
    {"equipmentAnchor",&Options::equipmentAnchor,9,Section::equipment,true},
    {"equipmentStyle",&Options::equipmentStyle,4,Section::equipment},
    {"anchorFrame",&Options::anchorFrame,3,Section::general,true},
    {"equipmentLayout",&Options::equipmentLayout,5,Section::equipment},
    {"equipmentTextPosition",&Options::equipmentTextPosition,3,Section::equipment},
};
inline constexpr FlagSetting flagSettings[]{
    {"pngiiIcons",&Options::pngiiIcons,Section::theme},
    {"coloredMagic",&Options::coloredMagic,Section::theme},
    {"keyIcons",&Options::keyIcons,Section::theme},
    {"customEquippedColor",&Options::customEquippedColor,Section::theme},
    {"customTextColor",&Options::customTextColor,Section::theme,true},
    {"customSelectionColor",&Options::customSelectionColor,Section::theme,true},
    {"mirrorCards",&Options::mirrorCards,Section::widget,true},
    {"showLabels",&Options::showLabels,Section::widget},
    {"showKeys",&Options::showKeys,Section::widget},
    {"showCardNames",&Options::showCardNames,Section::widget},
    {"showPoison",&Options::showPoison,Section::widget},
    {"showAmmo",&Options::showAmmo,Section::widget},
    {"hideEmptyGroups",&Options::hideEmptyGroups,Section::widget},
    {"showIdle",&Options::showIdle,Section::widget},
    {"fadeIdle",&Options::fadeIdle,Section::widget},
    {"keepCombatVisible",&Options::keepCombatVisible,Section::widget},
    {"keepDrawnVisible",&Options::keepDrawnVisible,Section::widget},
    {"selectorScaleLinked",&Options::selectorScaleLinked,Section::selector,true},
    {"showHints",&Options::showHints,Section::selector},
    {"animateScroll",&Options::animateScroll,Section::selector},
    {"showEquipment",&Options::showEquipment,Section::equipment},
    {"equipmentDim",&Options::equipmentDim,Section::equipment},
    {"showEquipmentAmmo",&Options::showEquipmentAmmo,Section::equipment},
    {"equipmentShowText",&Options::equipmentShowText,Section::equipment},
    {"animateHud",&Options::animateHud,Section::general},
    {"uiSounds",&Options::uiSounds,Section::general},
    {"i4Icons",&Options::i4Icons,Section::general},
};

inline const Options& defaultOptions(){static const Options options;return options;}
inline const FloatSetting& setting(float Options::*member) {
    for(const auto& s:floatSettings)if(s.value==member)return s;
    throw std::logic_error("Setting has no table row");
}
inline const ChoiceSetting& setting(unsigned Options::*member) {
    for(const auto& s:choiceSettings)if(s.value==member)return s;
    throw std::logic_error("Setting has no table row");
}
inline bool storedSetting(std::string_view key) {
    for(const auto& s:floatSettings)if(s.stored&&key==s.key)return true;
    for(const auto& s:choiceSettings)if(s.stored&&key==s.key)return true;
    for(const auto& s:flagSettings)if(s.stored&&key==s.key)return true;
    return false;
}
inline float steppedSetting(const FloatSetting& s,float value,int direction) {
    return std::clamp(std::round((value+float(direction)*s.step)/s.step)*s.step,s.low,s.high);
}
inline void sanitizeTable(Options& o) {
    const auto& defaults=defaultOptions();
    for(const auto& s:floatSettings){auto& v=o.*s.value;v=std::isfinite(v)?std::clamp(v,s.low,s.high):defaults.*s.value;}
    for(const auto& s:choiceSettings){auto& v=o.*s.value;v=std::min(v,s.limit);}
}
// Returns one Appearance page to its defaults. Placement, colors and theme choices that are not
// plain table rows are restored here as well.
inline void resetSection(Options& o,Section section) {
    const auto& defaults=defaultOptions();
    for(const auto& s:floatSettings)if(s.section==section)o.*s.value=defaults.*s.value;
    for(const auto& s:choiceSettings)if(s.section==section)o.*s.value=defaults.*s.value;
    for(const auto& s:flagSettings)if(s.section==section)o.*s.value=defaults.*s.value;
    if(section==Section::theme){o.themeAdjustments.erase(o.themeId);o.themeId=defaults.themeId;o.themeAdjustments.erase(o.themeId);o.fontId=defaults.fontId;o.equippedColor=defaults.equippedColor;o.textColor=defaults.textColor;o.selectionColor=defaults.selectionColor;}
}
inline void encodeTable(nlohmann::json& j,const Options& o) {
    for(const auto& s:floatSettings)if(s.stored)j[s.key]=o.*s.value;
    for(const auto& s:choiceSettings)if(s.stored)j[s.key]=o.*s.value;
    for(const auto& s:flagSettings)if(s.stored)j[s.key]=o.*s.value;
}
inline void decodeTable(const nlohmann::json& j,Options& o) {
    const auto& defaults=defaultOptions();
    for(const auto& s:floatSettings)if(s.stored)o.*s.value=j.value(s.key,defaults.*s.value);
    for(const auto& s:choiceSettings)if(s.stored)o.*s.value=j.value(s.key,defaults.*s.value);
    for(const auto& s:flagSettings)if(s.stored)o.*s.value=j.value(s.key,defaults.*s.value);
}
}

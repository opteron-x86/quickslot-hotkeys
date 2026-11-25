#pragma once
#include "Favorites.h"
#include "Localization.h"
#include "Presets.h"
#include <array>
#include <optional>
#include <string>

namespace quickslot {
// A group's name, bindings, place in the order, behavior and color belong to the character. They are stored in its
// saves beside the group's contents, under the same keys the settings file uses.
inline constexpr std::array<const char*,13> groupSetupKeys{"names","keys","controllerGroups","vrGroups","groupCount","groupOrder",
    "groupModes","groupHands","castStyles","separateConsumableUse","autoSort","groupColored","groupColors"};
inline void copyCharacterGroups(Options& target,const Options& source) {
    copyGroupSetup(target,source);target.keys=source.keys;target.controllerGroups=source.controllerGroups;target.vrGroups=source.vrGroups;
}
inline std::string encodeGroupSetup(const Options& options) {
    const auto all=Json::parse(encodeOptions(options));Json setup{{"schema",1}};
    for(const auto* key:groupSetupKeys)setup[key]=all.at(key);
    return setup.dump();
}
// The options with a saved group setup laid over them. A field the record lacks, or holds in a form this build
// cannot read, keeps its current value. Empty when the record itself is unreadable.
inline std::optional<Options> withGroupSetup(const Options& current,const std::string& bytes) {
    try {
        if(bytes.size()>65536)return {};
        const auto setup=Json::parse(bytes);
        if(!setup.is_object()||setup.value("schema",0)!=1)return {};
        auto merged=Json::parse(encodeOptions(current));
        for(const auto* key:groupSetupKeys) {
            if(!setup.contains(key))continue;
            auto trial=merged;trial[key]=setup.at(key);
            try{decodeOptions(trial.dump());merged=std::move(trial);}catch(const std::exception&){}
        }
        auto result=current;copyCharacterGroups(result,decodeOptions(merged.dump()));return result;
    }catch(const std::exception&){return {};}
}
// A character new to Quickslot starts with four groups that follow its Skyrim Favorites.
inline void starterGroups(Options& options,Arsenal& arsenal) {
    using enum FavoriteSource;
    struct Starter {const char* name;unsigned categories;};
    const std::array<Starter,4> starters{{
        {"Weapons",favoriteCategoryBit(weapons)|favoriteCategoryBit(offhand)},
        {"Spells",favoriteCategoryBit(spells)|favoriteCategoryBit(scrolls)},
        {"Potions",favoriteCategoryBit(potions)|favoriteCategoryBit(food)|favoriteCategoryBit(poisons)},
        {"Powers & shouts",favoriteCategoryBit(voice)}}};
    copyCharacterGroups(options,Options{});options.groupCount=starters.size();
    for(std::size_t i=0;i<starters.size();++i) {
        options.names[i]=tr(starters[i].name);
        setFavoriteCategories(arsenal.groups[i],starters[i].categories,Hand::right);
    }
    arsenal.active=0;
}
// Returns Quickslot to what a new installation shows a new character: default settings, the first-run hint and groups
// that follow Skyrim Favorites. Preset files and the character's preset identity stay.
inline void resetToUnplayed(Options& options,Arsenal& arsenal) {
    const auto identity=arsenal.presetIdentity;
    options=Options{};arsenal=Arsenal{};arsenal.presetIdentity=identity;
    starterGroups(options,arsenal);
}
}

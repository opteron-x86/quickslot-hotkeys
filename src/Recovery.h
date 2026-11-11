#pragma once
#include "Settings.h"

namespace quickslot {
// Loading keeps every group, entry and setting that is valid on its own. The
// strict decoders remain the authority on validity; these helpers repair the
// document until the strict decoder accepts it.
struct ArsenalRecovery {Arsenal arsenal;std::size_t dropped=0;};
using FormResolver=std::function<bool(std::uint32_t,std::uint32_t&)>;
inline bool blankArsenal(const Arsenal& a) {
    return a.shortcuts.empty()&&!a.rememberedConsumable&&std::all_of(a.groups.begin(),a.groups.end(),[](const Group& g){
        return g.entries.empty()&&g.manualEntries.empty()&&g.excludedFavorites.empty()&&!g.automatic();});
}
inline std::optional<ArsenalRecovery> recoverArsenal(const std::string& bytes,const FormResolver& resolve) {
    if(bytes.size()>8388608)return {};
    try {return ArsenalRecovery{decodeArsenal(bytes,resolve)};}catch(const std::exception&){}
    auto j=Json::parse(bytes,nullptr,false);
    if(!j.is_object()||!j.contains("schema")||j["schema"]!=arsenalSchema)return {};
    ArsenalRecovery result;
    const auto accepts=[&](const Json& entry){try{decodeEntry(entry,resolve);return true;}catch(const std::exception&){return false;}};
    const auto filter=[&](Json& list,std::size_t limit,const auto& keep) {
        Json kept=Json::array();
        if(list.is_array())for(auto& value:list){if(kept.size()<limit&&keep(value))kept.push_back(std::move(value));else ++result.dropped;}
        list=std::move(kept);
    };
    auto& shortcuts=j["shortcuts"];std::vector<std::pair<std::uint32_t,unsigned>> library;
    filter(shortcuts,Arsenal::shortcutLimit,[&](const Json& value) {
        try {
            const auto entry=decodeEntry(value,resolve);
            if(!isReusable(entry)||!entry.shortcut||std::any_of(library.begin(),library.end(),[&](const auto& known){return known.first==entry.shortcut;}))return false;
            library.emplace_back(entry.shortcut,unsigned(entry.kind));return true;
        }catch(const std::exception&){return false;}
    });
    const auto linked=[&](const Json& value) {
        if(!value.is_object()||!value.contains("shortcut"))return true;
        return std::any_of(library.begin(),library.end(),[&](const auto& known){return value["shortcut"]==known.first&&value.value("kind",Json())==known.second;});
    };
    Json groups=Json::array();
    for(std::size_t i=0;i<Arsenal::count;++i) {
        Json group=j.contains("groups")&&j["groups"].is_array()&&i<j["groups"].size()&&j["groups"][i].is_object()?j["groups"][i]:Json::parse(encode(Group{}));
        group["schema"]=groupSchema;
        filter(group["entries"],Group::limit,[&](const Json& value){return accepts(value)&&linked(value);});
        if(!group.contains("selected")||!group["selected"].is_number_unsigned())group["selected"]=0;
        const auto valid=[&]{try{decode(group.dump(),resolve);return true;}catch(const std::exception&){return false;}};
        if(group.contains("favorites")&&!valid()) {
            auto& favorites=group["favorites"];
            if(favorites.is_object()) {
                if(favorites.contains("excluded"))filter(favorites["excluded"],Group::exclusionLimit,accepts);
                if(favorites.contains("remembered")&&!valid()){favorites.erase("remembered");++result.dropped;}
            }
            if(!valid()){group.erase("favorites");++result.dropped;}
        }
        if(!valid()){result.dropped+=group["entries"].size();group=Json::parse(encode(Group{}));}
        groups.push_back(std::move(group));
    }
    j["groups"]=std::move(groups);
    if(!j.contains("active")||!j["active"].is_number_unsigned()||j["active"].get<std::uint64_t>()>=Arsenal::count)j["active"]=0;
    if(j.contains("consumableGroup")&&!j["consumableGroup"].is_number_unsigned())j.erase("consumableGroup");
    if(!j.contains("presetIdentity")||!j["presetIdentity"].is_string()||j["presetIdentity"].get_ref<const std::string&>().size()>64)j["presetIdentity"]="";
    try {result.arsenal=decodeArsenal(j.dump(),resolve);return result;}catch(const std::exception&){}
    if(j.contains("rememberedConsumable")){j.erase("rememberedConsumable");++result.dropped;}
    try {result.arsenal=decodeArsenal(j.dump(),resolve);return result;}catch(const std::exception&){return {};}
}

struct OptionsRecovery {Options options;std::vector<std::string> reset;};
inline std::optional<OptionsRecovery> recoverOptions(const std::string& bytes) {
    if(bytes.size()>65536)return {};
    const auto j=Json::parse(bytes,nullptr,false);
    if(!j.is_object()||!j.contains("schema")||j["schema"]!=1)return {};
    auto defaults=Json::parse(encodeOptions(Options{}));
    // Files written before layout version 2 carry equipment values in the earlier scale.
    if(!j.contains("layoutVersion"))defaults.erase("layoutVersion");
    auto candidate=defaults;OptionsRecovery result;
    for(const auto& [key,value]:j.items()) {
        if(key=="schema")continue;
        auto trial=defaults;trial[key]=value;
        try {decodeOptions(trial.dump());candidate[key]=value;}catch(const std::exception&){result.reset.push_back(key);}
    }
    try {result.options=decodeOptions(candidate.dump());return result;}catch(const std::exception&){return {};}
}
}

#pragma once
#include "Model.h"
#include <stdexcept>

namespace quickslot {
template<class A> auto* libraryShortcut(A& a,std::uint32_t id) {
    const auto it=std::find_if(a.shortcuts.begin(),a.shortcuts.end(),[&](const auto& e){return e.shortcut==id;});
    return it==a.shortcuts.end()?nullptr:&*it;
}
inline bool sameDefinition(const Entry& a,const Entry& b) {
    if(a.kind!=b.kind||a.name!=b.name||a.form!=b.form||a.owner!=b.owner||a.unique!=b.unique||a.hand!=b.hand||
       a.twoHanded!=b.twoHanded||a.spell!=b.spell||a.unresolved!=b.unresolved||a.recipe!=b.recipe||
       a.equipmentType!=b.equipmentType||a.resource!=b.resource||a.effect!=b.effect||a.policy!=b.policy||a.exclusions!=b.exclusions||a.parts.size()!=b.parts.size())return false;
    for(std::size_t i=0;i<a.parts.size();++i)if(!sameDefinition(a.parts[i],b.parts[i]))return false;
    return true;
}
inline std::uint32_t rememberShortcut(Arsenal& a,Entry entry) {
    if(!isReusable(entry))return 0;
    for(const auto& saved:a.shortcuts)if(sameDefinition(saved,entry))return saved.shortcut;
    if(a.shortcuts.size()>=Arsenal::shortcutLimit)return 0;
    std::uint64_t id=1;for(const auto& saved:a.shortcuts)id=std::max(id,std::uint64_t(saved.shortcut)+1);
    if(id>UINT32_MAX)return 0;
    entry.shortcut=std::uint32_t(id);for(auto& part:entry.parts)part.shortcut=0;
    a.shortcuts.push_back(std::move(entry));return std::uint32_t(id);
}

inline void collectShortcuts(Arsenal& a) {
    for(auto& group:a.groups)for(auto& entry:group.savedEntries())if(isReusable(entry)) {
        if(!entry.shortcut)entry.shortcut=rememberShortcut(a,entry);
        if(const auto* saved=libraryShortcut(a,entry.shortcut))entry=*saved;
    }
}
inline std::size_t shortcutUses(const Arsenal& a,std::uint32_t id) {
    if(!id)return 0;
    std::size_t count=0;for(const auto& g:a.groups)count+=std::any_of(g.savedEntries().begin(),g.savedEntries().end(),[&](const auto& e){return e.shortcut==id;});return count;
}
inline bool updateShortcut(Arsenal& a,std::uint32_t id,Entry entry) {
    auto* saved=libraryShortcut(a,id);if(!saved||!isReusable(entry)||(entry.kind!=saved->kind&&!(isHandAction(entry)&&isHandAction(*saved))))return false;
    entry.shortcut=id;for(auto& part:entry.parts)part.shortcut=0;
    *saved=entry;
    for(auto& group:a.groups) {
        bool changed=false;for(auto& value:group.savedEntries())if(value.shortcut==id){value=entry;changed=true;}
        if(changed)++group.revision;
    }
    return true;
}
inline bool addShortcut(Arsenal& a,std::size_t group,std::uint32_t id,bool autoSort=false) {
    const auto* saved=libraryShortcut(a,id);if(!saved||group>=Arsenal::count)return false;
    auto& target=a.groups[group];
    if(target.automatic())return false;
    const auto it=std::find_if(target.entries.begin(),target.entries.end(),[&](const auto& e){return e.shortcut==id;});
    if(it!=target.entries.end()){target.selected=std::size_t(it-target.entries.begin());++target.revision;return true;}
    if(target.entries.size()>=Group::limit)return false;

    target.entries.push_back(*saved);target.selected=target.entries.size()-1;++target.revision;
    if(autoSort)target.sortByName();
    return true;
}
inline bool deleteShortcut(Arsenal& a,std::uint32_t id) {
    if(!libraryShortcut(a,id))return false;
    for(auto& g:a.groups) {
        auto& entries=g.savedEntries();auto& selected=g.automatic()?g.manualSelected:g.selected;
        for(std::size_t i=entries.size();i-->0;)if(entries[i].shortcut==id) {
            entries.erase(entries.begin()+std::ptrdiff_t(i));if(selected>i)--selected;
            selected=entries.empty()?0:std::min(selected,entries.size()-1);++g.revision;
        }
    }
    std::erase_if(a.shortcuts,[&](const auto& e){return e.shortcut==id;});return true;
}
inline bool shortcutMatches(const Entry& entry,const std::string& search,int filter) {
    if(filter==11&&entry.kind!=Kind::combo)return false;
    if(filter==12&&entry.kind!=Kind::smartPotion&&!isSmartEquipment(entry))return false;
    if(filter==13&&!isHandAction(entry))return false;
    if(textContains(displayName(entry),search))return true;
    for(const auto& part:entry.parts)if(textContains(displayName(part),search))return true;
    return false;
}
}

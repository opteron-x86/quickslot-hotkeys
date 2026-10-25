#pragma once
#include "Model.h"
#include <tuple>
#include <set>

namespace quickslot {
inline const char* favoriteSourceName(FavoriteSource source) {
    constexpr const char* names[]{"Manual","All Favorites","Favorite weapons","Favorite spells","Favorite potions","Favorite poisons","Favorite food","Favorite ammo","Favorite scrolls","Favorite powers / shouts","Favorite shields / torches"};
    return unsigned(source)<unsigned(FavoriteSource::count)?names[unsigned(source)]:names[0];
}
inline bool favoriteMatches(FavoriteSource source,const Entry& e) {
    if(e.unresolved||!e.form||isReusable(e))return false;
    switch(source) {
    case FavoriteSource::all:return true;
    case FavoriteSource::weapons:return e.kind<=Kind::staff;
    case FavoriteSource::spells:return e.kind==Kind::spell;
    case FavoriteSource::potions:return e.kind==Kind::potion;
    case FavoriteSource::poisons:return e.kind==Kind::poison;
    case FavoriteSource::food:return e.kind==Kind::food;
    case FavoriteSource::ammo:return e.kind==Kind::ammo;
    case FavoriteSource::scrolls:return e.kind==Kind::scroll;
    case FavoriteSource::voice:return isVoice(e);
    case FavoriteSource::offhand:return e.kind==Kind::shield||e.kind==Kind::torch;
    default:return false;
    }
}
inline bool setFavoriteSource(Group& group,FavoriteSource source,Hand hand) {
    if(source>=FavoriteSource::count||unsigned(hand)>1||(source==group.source&&hand==group.favoriteHand))return false;
    if(!group.automatic()&&source!=FavoriteSource::manual){group.manualEntries=std::move(group.entries);group.manualSelected=group.selected;}
    else if(group.automatic()&&group.selected<group.entries.size())group.rememberedFavorite=group.entries[group.selected];
    if(source==FavoriteSource::manual&&group.automatic()){group.entries=std::move(group.manualEntries);group.manualEntries.clear();group.selected=group.manualSelected;}
    else if(source!=FavoriteSource::manual){group.entries.clear();group.selected=0;}
    group.source=source;group.favoriteHand=hand;++group.revision;return true;
}
inline bool refreshFavorites(Group& group,const std::vector<Entry>& favorites) {
    if(!group.automatic())return false;
    if(group.selected<group.entries.size())group.rememberedFavorite=group.entries[group.selected];
    std::vector<Entry> entries;
    std::set<std::tuple<std::uint32_t,std::uint32_t,std::uint16_t,std::uint64_t,Kind>> seen;
    for(auto entry:favorites) {
        if(!favoriteMatches(group.source,entry)||std::any_of(group.excludedFavorites.begin(),group.excludedFavorites.end(),[&](const auto& e){return sameFavorite(e,entry);}))continue;
        entry.hand=normalizedHand(entry,group.favoriteHand);entry.shortcut=0;
        if(seen.emplace(entry.form,entry.owner,entry.unique,entry.recipe,entry.kind).second)entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){
        const auto an=foldedText(a.name),bn=foldedText(b.name);
        return an!=bn?an<bn:std::tie(a.form,a.owner,a.unique,a.recipe,a.kind)<std::tie(b.form,b.owner,b.unique,b.recipe,b.kind);
    });
    std::size_t selected=0;
    if(group.rememberedFavorite)for(std::size_t i=0;i<entries.size();++i)if(sameFavorite(entries[i],*group.rememberedFavorite)){selected=i;break;}
    const bool changed=entries.size()!=group.entries.size()||!std::equal(entries.begin(),entries.end(),group.entries.begin(),group.entries.end(),[](const auto& a,const auto& b){return a.same(b);});
    group.entries=std::move(entries);group.selected=selected;
    if(changed)++group.revision;
    return changed;
}
inline bool hasFavoriteGroups(const Arsenal& a){return std::any_of(a.groups.begin(),a.groups.end(),[](const auto& g){return g.automatic();});}
}

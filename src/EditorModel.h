#pragma once
#include "Model.h"
#include "SmartConsumables.h"
#include "Storage.h"
#include <cctype>

namespace quickslot {

struct CatalogItem {Entry entry;std::uint64_t id=0;int count=1;bool equipped=false,addable=true;std::string detail;};
struct EditorState {
    bool open=false,soulsSlowdown=false,controller=false,vr=false;
    std::uint64_t session=0,revision=0,catalogVersion=0,restoreSerial=0;
    bool canUndo=false,canRevert=false,settingsPending=false;
    std::string saveError;
    Arsenal draft;
    Options options;
    std::vector<CatalogItem> catalog;
    std::array<std::uint64_t,2> equippedCandidates{};
    bool equippedPair=true;
    std::vector<PotionCandidate> potions;
    std::vector<EquipmentCandidate> equipment;
    std::array<float,3> deficits{};
    std::array<std::vector<Row>,Arsenal::count> rows;
    EquippedDisplay equippedDisplay;
    std::string message;
    std::vector<std::string> presets;
};
enum class EditAction {select,add,remove,reorder,move,hand,settings,close,undo,revert,refresh,addGroup,removeGroup,clearGroup,groupUp,groupDown,saveShortcut,exportPreset,importPreset,refreshPresets,saveCombination,addShortcut,deleteShortcut,sortGroup,favoriteSource,resetFavoriteExclusions};
struct EditCommand {
    EditAction action=EditAction::select;
    std::uint64_t session=0,revision=0,catalogVersion=0,candidate=0;
    std::size_t group=0,index=0,target=0;
    Hand hand=Hand::right;
    FavoriteSource source=FavoriteSource::manual;
    Options options;
    bool hasSettings=false,continuous=false;
    Entry shortcut;
    std::array<std::uint64_t,2> components{};
    bool replace=false;
    std::uint32_t libraryID=0;
    std::string presetName;
    bool appearanceOnly=true;
};
inline bool matchingEdit(const EditorState& s,const EditCommand& c) {
    return s.open&&c.session==s.session&&c.revision==s.revision&&c.group<Arsenal::count;
}
inline const CatalogItem* catalogItem(const EditorState& s,std::uint64_t id) {
    const auto it=std::find_if(s.catalog.begin(),s.catalog.end(),[&](const auto& c){return c.id==id;});
    return it==s.catalog.end()?nullptr:&*it;
}
inline bool handCandidate(const CatalogItem& c,Hand hand) {
    return c.addable&&pairable(c.entry)&&(hand==Hand::left||(c.entry.kind!=Kind::shield&&c.entry.kind!=Kind::torch&&!smartShield(c.entry)));
}
inline std::string combinationName(const Entry& e) {
    if(e.parts.size()!=2)return "Combination";
    return e.parts[0].name==e.parts[1].name?e.parts[0].name+" · Both hands":e.parts[0].name+" + "+e.parts[1].name;
}
inline bool validCombinationDraft(const EditorState& s,const EditCommand& c) {
    if(c.group>=Arsenal::count||c.catalogVersion!=s.catalogVersion||c.shortcut.kind!=Kind::combo||c.shortcut.parts.size()!=2)return false;
    const auto* original=c.libraryID?libraryShortcut(s.draft,c.libraryID):c.replace&&c.index<s.draft.groups[c.group].entries.size()?&s.draft.groups[c.group].entries[c.index]:nullptr;
    if((c.replace||c.libraryID)&&(!original||original->kind!=Kind::combo))return false;
    for(unsigned hand=0;hand<2;++hand) {
        if(c.components[hand]) {const auto* item=catalogItem(s,c.components[hand]);if(!item||!handCandidate(*item,Hand(hand)))return false;}
        else {
            const auto& e=c.shortcut.parts[hand];
            if(e.kind==Kind::smartWeapon&&pairable(e)&&e.hand==Hand(hand)&&!e.shortcut)continue;
            if(e.kind==Kind::emptyHand&&validHandAction(e)&&e.hand==Hand(hand))continue;
            if(!original||!sameDefinition(e,original->parts.at(hand)))return false;
        }
    }
    if(c.components[0]&&c.components[0]==c.components[1]) {
        const auto& item=*catalogItem(s,c.components[0]);
        if(tracksInstance(item.entry)&&(item.entry.unique||item.count<2))return false;
        if(item.entry.kind==Kind::scroll&&item.count<2)return false;
    }
    return true;
}

template<class Capture> std::optional<Entry> composeCombination(const EditorState& s,const EditCommand& c,Capture capture) {
    if(!validCombinationDraft(s,c))return {};
    auto entry=c.shortcut;
    for(unsigned hand=0;hand<2;++hand)if(c.components[hand]) {
        auto part=capture(c.components[hand],Hand(hand));if(!part)return {};entry.parts[hand]=*part;
    }
    if(!validPair(entry.parts[0],entry.parts[1]))return {};
    entry.name=cleanName(entry.name.empty()?combinationName(entry):entry.name);
    return entry;
}
inline bool editStructure(Arsenal& a,Options& o,const EditCommand& c) {
    if(c.action==EditAction::favoriteSource&&c.group<Arsenal::count)return setFavoriteSource(a.groups[c.group],c.source,c.hand);
    if(c.action==EditAction::resetFavoriteExclusions&&c.group<Arsenal::count) {
        auto& g=a.groups[c.group];if(g.excludedFavorites.empty())return false;g.excludedFavorites.clear();++g.revision;return true;
    }
    if(c.action==EditAction::addShortcut&&c.group<Arsenal::count)return addShortcut(a,c.group,c.libraryID,o.autoSort[c.group]);
    if(c.action==EditAction::sortGroup&&c.group<Arsenal::count)return a.groups[c.group].sortByName();
    if(c.action==EditAction::deleteShortcut)return deleteShortcut(a,c.libraryID);
    if(c.action==EditAction::addGroup)return addGroup(a,o);
    if(c.action==EditAction::removeGroup)return removeGroup(a,o,c.group);
    if(c.action==EditAction::groupUp||c.action==EditAction::groupDown)return reorderGroup(o,c.group,c.action==EditAction::groupUp?-1:1);
    if(c.action==EditAction::clearGroup&&c.group<Arsenal::count){auto& g=a.groups[c.group];if(g.automatic()||g.entries.empty())return false;g.entries.clear();g.selected=0;++g.revision;return true;}
    if(c.action!=EditAction::saveShortcut||c.group>=Arsenal::count)return false;
    const auto& e=c.shortcut;
    if(isSmartEquipment(e)){if(!validSmartEquipment(e))return false;}
    else if(isHandAction(e)){if(!validHandAction(e))return false;}
    else if(e.kind==Kind::smartPotion){if(e.resource>2||e.effect>1||e.policy>2||(e.effect&&e.policy==2)||e.exclusions.size()>128)return false;}
    else if(e.kind!=Kind::combo||e.parts.size()!=2||!validPair(e.parts[0],e.parts[1]))return false;
    auto& g=a.groups[c.group];
    if(c.libraryID)return updateShortcut(a,c.libraryID,e);
    if(g.automatic())return false;
    if(c.replace){
        if(c.index>=g.entries.size())return false;
        collectShortcuts(a);const auto id=g.entries[c.index].shortcut;
        return id&&updateShortcut(a,id,e);
    }
    if(g.entries.size()>=Group::limit)return false;
    const auto id=rememberShortcut(a,e);return id&&addShortcut(a,c.group,id,o.autoSort[c.group]);
}
inline bool editGroup(Arsenal& a,const Options& o,const EditCommand& c) {
    if(c.group>=Arsenal::count)return false;
    auto& group=a.groups[c.group];
    if(c.action==EditAction::select){a.active=c.group;return true;}
    if(c.index>=group.entries.size())return false;
    if(c.action==EditAction::reorder)return group.reorder(c.index,c.target);
    if(c.action==EditAction::move&&c.target<Arsenal::count)return a.move(c.group,c.index,c.target,o.autoSort[c.target]);
    if(c.action==EditAction::remove) {
        const auto remembered=group.selected;group.selected=c.index;
        if(!group.remove()){group.selected=remembered;return false;}
        if(remembered!=c.index)group.selected=remembered>c.index?remembered-1:remembered;
        return true;
    }
    if(c.action==EditAction::hand) {
        if(group.automatic())return false;
        auto& entry=group.entries[c.index];if(!flexibleHand(entry))return false;
        auto changed=entry;changed.hand=c.hand;
        for(std::size_t i=0;i<group.entries.size();++i)if(i!=c.index&&group.entries[i].same(changed))return false;
        changed.hand=c.hand;if(isHandAction(changed))changed.name=handActionName(changed);
        if(entry.shortcut)return updateShortcut(a,entry.shortcut,changed);
        entry=changed;++group.revision;return true;
    }
    return false;
}
inline bool catalogMatches(const CatalogItem& c,const std::string& query,int filter) {
    if(filter>=10)return false;
    if(filter==1&&unsigned(c.entry.kind)>unsigned(Kind::staff))return false;
    if(filter==2&&c.entry.kind!=Kind::spell)return false;
    if(filter==3&&c.entry.kind!=Kind::shield&&c.entry.kind!=Kind::torch)return false;
    if(filter==4&&c.entry.kind!=Kind::potion)return false;
    if(filter==5&&c.entry.kind!=Kind::food)return false;
    if(filter==6&&c.entry.kind!=Kind::poison)return false;
    if(filter==7&&c.entry.kind!=Kind::scroll)return false;
    if(filter==8&&!isVoice(c.entry))return false;
    if(filter==9&&c.entry.kind!=Kind::ammo)return false;
    return textContains(c.entry.name,query);
}
enum class EditorInputType {key,character,move,button,wheel,pad,stick};
struct EditorInput {EditorInputType type;unsigned code=0;bool down=false;float x=0,y=0;};

inline bool captureEditorButton(bool& owned,bool down,bool fresh) {
    if(fresh){owned=true;return true;}
    if(!down){const bool captured=owned;owned=false;return captured;}
    return true;
}
}

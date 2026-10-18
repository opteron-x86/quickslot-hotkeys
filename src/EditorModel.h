#pragma once
#include "Model.h"
#include "SmartConsumables.h"
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
    std::vector<PotionCandidate> potions;
    std::array<float,3> deficits{};
    std::array<std::vector<Row>,Arsenal::count> rows;
    std::string message;
    std::vector<std::string> presets;
};
enum class EditAction {select,add,remove,reorder,move,hand,settings,close,undo,revert,refresh,addGroup,removeGroup,clearGroup,groupUp,groupDown,saveShortcut,exportPreset,importPreset,refreshPresets};
struct EditCommand {
    EditAction action=EditAction::select;
    std::uint64_t session=0,revision=0,catalogVersion=0,candidate=0;
    std::size_t group=0,index=0,target=0;
    Hand hand=Hand::right;
    Options options;
    bool hasSettings=false,continuous=false;
    Entry shortcut;
    bool replace=false;
    std::string presetName;
    bool appearanceOnly=true;
};
inline bool matchingEdit(const EditorState& s,const EditCommand& c) {
    return s.open&&c.session==s.session&&c.revision==s.revision&&c.group<Arsenal::count;
}
inline bool editStructure(Arsenal& a,Options& o,const EditCommand& c) {
    if(c.action==EditAction::addGroup)return addGroup(a,o);
    if(c.action==EditAction::removeGroup)return removeGroup(a,o,c.group);
    if(c.action==EditAction::groupUp||c.action==EditAction::groupDown)return reorderGroup(o,c.group,c.action==EditAction::groupUp?-1:1);
    if(c.action==EditAction::clearGroup&&c.group<Arsenal::count){auto& g=a.groups[c.group];if(g.entries.empty())return false;g.entries.clear();g.selected=0;++g.revision;return true;}
    if(c.action!=EditAction::saveShortcut||c.group>=Arsenal::count)return false;
    const auto& e=c.shortcut;
    if(isHandAction(e)){if(!validHandAction(e))return false;}
    else if(e.kind==Kind::smartPotion){if(e.resource>2||e.effect>1||e.policy>2||(e.effect&&e.policy==2)||e.exclusions.size()>128)return false;}
    else if(e.kind!=Kind::combo||e.parts.size()!=2||!validPair(e.parts[0],e.parts[1]))return false;
    auto& g=a.groups[c.group];
    if(c.replace){if(c.index>=g.entries.size())return false;g.entries[c.index]=e;++g.revision;return true;}
    if(g.entries.size()>=Group::limit)return false;
    g.add(e);return true;
}
inline bool editGroup(Arsenal& a,const EditCommand& c) {
    if(c.group>=Arsenal::count)return false;
    auto& group=a.groups[c.group];
    if(c.action==EditAction::select){a.active=c.group;return true;}
    if(c.index>=group.entries.size())return false;
    if(c.action==EditAction::reorder)return group.reorder(c.index,c.target);
    if(c.action==EditAction::move)return a.move(c.group,c.index,c.target);
    if(c.action==EditAction::remove) {
        const auto remembered=group.selected;group.selected=c.index;group.remove();
        if(remembered!=c.index)group.selected=remembered>c.index?remembered-1:remembered;
        return true;
    }
    if(c.action==EditAction::hand) {
        auto& entry=group.entries[c.index];if(!flexibleHand(entry))return false;
        auto changed=entry;changed.hand=c.hand;
        for(std::size_t i=0;i<group.entries.size();++i)if(i!=c.index&&group.entries[i].same(changed))return false;
        entry.hand=c.hand;if(isHandAction(entry))entry.name=handActionName(entry);++group.revision;return true;
    }
    return false;
}
inline bool catalogMatches(const CatalogItem& c,const std::string& query,int filter) {
    if(filter==1&&unsigned(c.entry.kind)>unsigned(Kind::staff))return false;
    if(filter==2&&c.entry.kind!=Kind::spell)return false;
    if(filter==3&&c.entry.kind!=Kind::shield&&c.entry.kind!=Kind::torch)return false;
    if(filter==4&&c.entry.kind!=Kind::potion)return false;
    if(filter==5&&c.entry.kind!=Kind::food)return false;
    if(filter==6&&c.entry.kind!=Kind::poison)return false;
    if(filter==7&&c.entry.kind!=Kind::scroll)return false;
    if(filter==8&&!isVoice(c.entry))return false;
    if(filter==9&&c.entry.kind!=Kind::ammo)return false;
    return std::search(c.entry.name.begin(),c.entry.name.end(),query.begin(),query.end(),
        [](unsigned char a,unsigned char b){return std::tolower(a)==std::tolower(b);})!=c.entry.name.end();
}
enum class EditorInputType {key,character,move,button,wheel,pad,stick};
struct EditorInput {EditorInputType type;unsigned code=0;bool down=false;float x=0,y=0;};
inline bool captureEditorButton(bool& owned,bool down,bool fresh) {
    if(fresh){owned=true;return true;}
    if(!down){const bool captured=owned;owned=false;return captured;}
    return true;
}
}

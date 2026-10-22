#include "LiveEditor.h"
#include "Presets.h"
#include <iostream>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* label,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(label)+" at "+std::to_string(at.line()));++checks;
}
int main()try {
    Arsenal a;Options o;EditCommand c;c.action=EditAction::saveShortcut;c.shortcut=smartEquipmentEntry(false,unsigned(WeaponFamily::mace));
    check(editStructure(a,o,c),"Create shared smart weapon");const auto id=a.groups[0].entries[0].shortcut;
    check(id&&a.shortcuts.size()==1&&libraryShortcut(a,id),"Definition retained in library");
    c.action=EditAction::addShortcut;c.libraryID=id;c.group=1;
    check(editStructure(a,o,c)&&a.groups[1].entries[0].shortcut==id,"Reuse definition in another group");
    check(editStructure(a,o,c)&&a.groups[1].entries.size()==1,"Adding again selects existing use");
    EditHistory history;history.begin(a,o);history.prepare(a,o);
    c.action=EditAction::saveShortcut;c.shortcut=smartEquipmentEntry(false,unsigned(WeaponFamily::sword),Hand::left);
    check(editStructure(a,o,c),"Edit library definition");history.finish(true,false,0);
    check(a.groups[0].entries[0].hand==Hand::left&&a.groups[1].entries[0].equipmentType==unsigned(WeaponFamily::sword),"Every group updates");
    check(history.undo(a,o)&&a.groups[0].entries[0].equipmentType==unsigned(WeaponFamily::mace)&&a.groups[1].entries[0].hand==Hand::right,"Undo restores all uses atomically");
    c={};c.action=EditAction::remove;c.group=0;c.index=0;
    check(editGroup(a,o,c)&&libraryShortcut(a,id)&&a.groups[1].entries.size()==1,"Remove leaves definition and other group intact");
    c.action=EditAction::clearGroup;c.group=1;check(editStructure(a,o,c),"Clear final use");
    check(shortcutUses(a,id)==0&&a.shortcuts.size()==1,"Unused definition stays reusable");
    check(addShortcut(a,2,id),"Re-add orphan definition");
    const auto identity=[](auto old,auto& mapped){mapped=old;return true;};
    const auto bytes=encodeArsenal(a);auto restored=decodeArsenal(bytes,identity);
    check(encodeArsenal(restored)==bytes&&restored.groups[2].entries[0].shortcut==id,"Save preserves shared links");
    auto bad=Json::parse(bytes);bad["groups"][2]["entries"][0]["shortcut"]=999u;
    bool rejected=false;try{decodeArsenal(bad.dump(),identity);}catch(...){rejected=true;}check(rejected,"Missing definition rejected");
    bad=Json::parse(bytes);bad["shortcuts"].push_back(bad["shortcuts"][0]);rejected=false;
    try{decodeArsenal(bad.dump(),identity);}catch(...){rejected=true;}check(rejected,"Duplicate IDs rejected");
    c={};c.action=EditAction::saveShortcut;c.group=0;c.shortcut=smartEntry(0,0,2);
    check(editStructure(a,o,c),"Smart potion can be shared");const auto potion=a.groups[0].entries[0].shortcut;check(addShortcut(a,3,potion),"Reuse smart potion");
    c.replace=true;c.index=0;c.shortcut.exclusions.push_back({0x555,0});
    check(editStructure(a,o,c)&&a.groups[3].entries[0].exclusions.size()==1,"Editing from a group updates all exclusions");
    c={};c.action=EditAction::saveShortcut;c.shortcut=unarmedEntry();check(editStructure(a,o,c),"Create shared hand action");
    const auto hand=a.groups[0].entries.back().shortcut;check(addShortcut(a,4,hand),"Reuse hand action");
    c.libraryID=hand;c.shortcut=emptyHandEntry(Hand::left);check(editStructure(a,o,c)&&a.groups[4].entries[0].kind==Kind::emptyHand,"Hand action can switch between both and single hand");
    Entry right;right.form=0x123;right.owner=0x14;right.unique=42;right.name="Tempered sword";
    Entry left;left.form=0x456;left.spell=true;left.kind=Kind::spell;left.hand=Hand::left;left.name="Flames";
    Entry combo;combo.kind=Kind::combo;combo.name="Fire and steel";combo.parts={right,left};
    c={};c.action=EditAction::saveShortcut;c.shortcut=combo;check(editStructure(a,o,c),"Create combination with exact copy");
    const auto pair=a.groups[0].entries.back().shortcut;check(addShortcut(a,5,pair),"Reuse combination");
    check(followIdentityChange(a,{0x123,0x14,0x55,42,9})==3,"Identity event reaches library and both uses");
    check(libraryShortcut(a,pair)->parts[0].owner==0x55&&a.groups[5].entries[0].parts[0].unique==9,"Exact identity stays consistent");
    check(shortcutMatches(*libraryShortcut(a,pair),"flAMes",11),"Search matches combination components");
    check(!shortcutMatches(*libraryShortcut(a,pair),"",12)&&shortcutMatches(*libraryShortcut(a,potion),"",12),"Filters distinguish combos and smart selections");
    check(shortcutMatches(*libraryShortcut(a,hand),"",13)&&!shortcutMatches(*libraryShortcut(a,hand),"",11),"Hand-action filter");
    EditorState state;state.open=true;state.catalogVersion=1;state.draft=a;c={};c.action=EditAction::saveCombination;c.catalogVersion=1;c.libraryID=pair;c.shortcut=*libraryShortcut(a,pair);
    check(validCombinationDraft(state,c),"Existing library combination edits without inventory recapture");
    c.shortcut.parts[0].unique=99;check(!validCombinationDraft(state,c),"Library editor rejects a substituted copy");
    a.presetIdentity="one";Arsenal other;other.presetIdentity="two";
    auto imported=importPreset(exportPreset(a,o,false,[](auto id){return Json{{"file","Test.esp"},{"local",id}};}),other,o,false,[](const auto&,auto local){return local;});
    check(libraryShortcut(imported.arsenal,pair)->parts[0].unresolved&&imported.arsenal.groups[5].entries[0].parts[0].unresolved,"Foreign character cannot rebind library exact-copy items");
    check(!libraryShortcut(imported.arsenal,pair)->parts[1].unresolved&&libraryShortcut(imported.arsenal,id),"Spells and smart rules remain portable");
    check(encodeArsenal(decodeArsenal(encodeArsenal(imported.arsenal),identity))==encodeArsenal(imported.arsenal),"Foreign library survives save and reload");
    auto legacyShape=Json::parse(encodeArsenal(a));legacyShape.erase("shortcuts");for(auto& g:legacyShape["groups"])for(auto& e:g["entries"])e.erase("shortcut");
    auto collected=decodeArsenal(legacyShape.dump(),identity);
    check(collected.groups[0].entries.back().shortcut==collected.groups[5].entries[0].shortcut,"Existing matching shortcuts collect into one definition");
    history.begin(a,o);history.prepare(a,o);check(deleteShortcut(a,pair)&&!libraryShortcut(a,pair)&&a.groups[5].entries.empty(),"Delete removes library definition and all uses");history.finish(true,false,1);
    check(history.undo(a,o)&&libraryShortcut(a,pair)&&a.groups[5].entries.size()==1,"Undo restores deleted definition and every membership");
    auto full=a;for(unsigned i=0;full.groups[6].entries.size()<Group::limit;++i){Entry e;e.form=900+i;full.groups[6].add(e);}
    const auto fullBefore=encodeArsenal(full);check(!addShortcut(full,6,id)&&encodeArsenal(full)==fullBefore,"Full target cannot mutate definition or group");
    std::cout<<"PASS: "<<checks<<" shared shortcut, persistence, identity, preset and undo checks\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

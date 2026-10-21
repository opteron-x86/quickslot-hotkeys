#include "EditorModel.h"
#include "Presets.h"
#include "VoiceAction.h"
#include "IconPolicy.h"
#include "LiveEditor.h"
#include <random>
#include <iostream>
#include <limits>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()){if(!value)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));++checks;}
int main()try {
    const auto identity=[](std::uint32_t id,std::uint32_t& out){out=id;return true;};
    std::vector<EquipmentCandidate> choices;
    for(unsigned type=2;type<unsigned(WeaponFamily::count);++type) {
        EquipmentCandidate c;c.entry.form=type;c.entry.owner=20;c.entry.unique=std::uint16_t(type);c.entry.kind=type==unsigned(WeaponFamily::shield)?Kind::shield:type>=9?Kind::bow:Kind::sword;c.entry.twoHanded=smartTwoHanded(type);
        c.family=WeaponFamily(type);c.count=1;c.damage=float(type*3);choices.push_back(c);
    }
    for(unsigned type=0;type<unsigned(WeaponFamily::count);++type) {
        const auto rule=smartEquipmentEntry(false,type,Hand::left);check(validSmartEquipment(rule),"Every weapon family has a valid rule");
        check(!tracksInstance(rule)&&!isInventory(rule),"Rules are not physical inventory instances");
        check(pairable(rule)==!rule.twoHanded,"Only one-handed rules can be combination components");
        auto automatic=rule;automatic.icon=Icon::automatic;
        check(baseIcon(automatic)==smartEquipmentIcon(rule)&&baseIcon(automatic)!=Icon::spell,"Smart rules retain family icons without native keyword overrides");
        const auto* best=chooseEquipment(rule,choices);check(best,"An appropriate family is found");
        check(unsigned(best->family)==(type==0?5:type==1?8:type),"Families rank their own physical damage, excluding ranged from melee");
        auto bytes=encodeEntry(rule);const auto restored=decodeEntry(bytes,identity);check(rule.same(restored)&&validSmartEquipment(restored),"Weapon rule survives save/load");
        auto invalid=rule;invalid.form=1;check(!validSmartEquipment(invalid)&&!chooseEquipment(invalid,choices),"Forged form references are not smart rules");
    }
    auto rule=smartEquipmentEntry(false,2);auto a=choices[0],b=a;b.entry.unique=99;b.damage+=20;
    std::vector<EquipmentCandidate> copies{a,b};check(chooseEquipment(rule,copies)==&copies[1],"Tempered copy's effective damage wins over base copy");
    copies[0].damage=copies[1].damage;copies[1].rightEquipped=true;
    check(chooseEquipment(rule,copies)==&copies[1],"Equal damage preserves current equipped copy");
    rule.hand=Hand::left;copies[1].rightEquipped=false;copies[1].leftEquipped=true;
    check(chooseEquipment(rule,copies)==&copies[1],"Tie preference follows chosen hand");
    copies[1].count=0;check(chooseEquipment(rule,copies)==&copies[0],"Dropped or exhausted winner is replaced");
    copies[0].damage=std::numeric_limits<float>::quiet_NaN();check(!chooseEquipment(rule,copies),"Invalid and empty candidates cannot win");
    EquipmentCandidate arrow,bolt;arrow.entry.form=30;arrow.entry.kind=Kind::ammo;arrow.entry.icon=Icon::arrow;arrow.damage=9;arrow.count=20;
    bolt=arrow;bolt.entry.form=31;bolt.entry.icon=Icon::bolt;bolt.damage=99;
    const std::vector<EquipmentCandidate> ammo{arrow,bolt};
    check(chooseEquipment(smartEquipmentEntry(true),ammo)==&ammo[0],"Bolts cannot replace arrows even with higher damage");
    check(chooseEquipment(smartEquipmentEntry(true,1),ammo)==&ammo[1],"Best bolt slot selects bolts");
    for(bool isAmmo:{false,true}) {
        auto e=smartEquipmentEntry(isAmmo);Group group;check(group.add(e),"Smart rule can be added");check(!group.add(e),"Duplicate smart rules select existing entry");
        check(decode(encode(group),identity).entries[0].same(e),"Smart group round trip");
        auto encoded=encodeEntry(e);encoded["equipmentType"]=999u;bool rejected=false;
        try{decodeEntry(encoded,identity);}catch(...){rejected=true;}check(rejected,"Invalid equipment type is rejected on import");
        Arsenal arsenal;Options options;EditCommand edit;edit.action=EditAction::saveShortcut;edit.shortcut=e;
        check(editStructure(arsenal,options,edit),"Unified editor accepts smart equipment");
        const auto preset=exportPreset(arsenal,options,false,[](auto){throw std::runtime_error("Rule unexpectedly needs a plugin reference");return Json{};});
        const auto loaded=importPreset(preset,{},options,false,[](auto&,auto){return 0u;});
        check(loaded.unresolved==0&&loaded.arsenal.groups[0].entries[0].same(e),"Smart rules transfer between characters without item-copy references");
    }
    const auto sword=smartEquipmentEntry(false,unsigned(WeaponFamily::sword));
    const auto mace=smartEquipmentEntry(false,unsigned(WeaponFamily::mace),Hand::left);
    const auto shield=smartEquipmentEntry(false,unsigned(WeaponFamily::shield));
    check(shield.hand==Hand::left&&!shield.twoHanded&&!flexibleHand(shield)&&baseIcon(shield)==Icon::shield,"Best shield is an offhand rule with a shield icon");
    auto badShield=shield;badShield.hand=Hand::right;check(!validSmartEquipment(badShield)&&!pairable(badShield),"A smart shield cannot be forced into the right hand");
    Entry combo;combo.kind=Kind::combo;combo.name="Smart pair";combo.parts={sword,mace};
    auto plan=chooseCombination(combo,choices);
    check(plan&&choices[(*plan)[0]].family==WeaponFamily::sword&&choices[(*plan)[1]].family==WeaponFamily::mace,"Sword plus mace picks each requested family");
    combo.parts={smartEquipmentEntry(false,unsigned(WeaponFamily::axe)),shield};plan=chooseCombination(combo,choices);
    check(plan&&choices[(*plan)[1]].family==WeaponFamily::shield,"Weapon plus shield resolves both components");
    check(hasSmartEquipment(combo),"Nested rules request the equipment inventory scan");
    check(sameDefinition(decodeEntry(encodeEntry(combo),identity),combo),"Nested smart rules survive save/load without form identities");
    auto broken=combo;broken.parts[0]=smartEquipmentEntry(true);
    for(const auto& entry:{broken.parts[0],smartEquipmentEntry(false,unsigned(WeaponFamily::bow)),smartEquipmentEntry(false,unsigned(WeaponFamily::twoHanded))}) {
        broken.parts[0]=entry;bool rejected=false;try{decodeEntry(encodeEntry(broken),identity);}catch(...){rejected=true;}
        check(rejected&&!chooseCombination(broken,choices),"Ammo and two-handed smart rules are rejected in combinations");
    }
    EquipmentCandidate first=choices[0],second=first;first.damage=40;second.damage=30;second.entry.unique=99;
    std::vector<EquipmentCandidate> pair{first,second};
    combo.parts={sword,sword};combo.parts[1].hand=Hand::left;
    plan=chooseCombination(combo,pair);check(plan&&(*plan)[0]==0&&(*plan)[1]==1,"Two identical smart rules reserve the strongest two distinct copies");
    pair.pop_back();check(!chooseCombination(combo,pair),"One physical sword cannot satisfy both hands");
    pair[0].entry.owner=0;pair[0].entry.unique=0;pair[0].count=2;
    plan=chooseCombination(combo,pair);check(plan&&(*plan)[0]==0&&(*plan)[1]==0,"Two plain copies can be individually captured for both hands");
    pair[0].count=1;check(!chooseCombination(combo,pair),"A depleted plain stack cannot be reused twice");
    pair[0].count=2;check(bool(chooseCombination(combo,pair)),"Restocking restores the same saved rules");
    pair={first,second};combo.parts[1]=first.entry;combo.parts[1].hand=Hand::left;
    plan=chooseCombination(combo,pair);check(plan&&(*plan)[0]==1&&(*plan)[1]==noEquipmentCandidate,"A fixed weapon reserves its exact copy before smart ranking");
    auto otherFamily=choices[3];otherFamily.damage=20;pair={first,otherFamily};
    combo.parts={smartEquipmentEntry(false),sword};combo.parts[1].hand=Hand::left;
    plan=chooseCombination(combo,pair);check(plan&&(*plan)[0]==1&&(*plan)[1]==0,"Best one-handed does not consume the only sword needed by the other hand");
    pair={first,second};pair[0].damage=pair[1].damage;pair[0].leftEquipped=true;pair[1].rightEquipped=true;
    combo.parts={sword,sword};combo.parts[1].hand=Hand::left;
    plan=chooseCombination(combo,pair);check(plan&&(*plan)[0]==1&&(*plan)[1]==0,"Equal-ranked copies stay in their current hands");
    pair[1].entry=pair[0].entry;check(!chooseCombination(combo,pair),"Duplicate unique identities cannot stand in for two physical copies");

    std::mt19937 rng(20);
    for(unsigned trial=0;trial<400;++trial) {
        pair.clear();
        for(unsigned i=0;i<12;++i){auto c=first;c.entry.form=i+1;c.entry.unique=std::uint16_t(i+1);c.family=WeaponFamily(2+rng()%4);c.damage=float(1+rng()%50);c.count=rng()%4?1:0;pair.push_back(c);}
        const unsigned types[]{0,2,3,4,5};combo.parts={smartEquipmentEntry(false,types[rng()%5]),smartEquipmentEntry(false,types[rng()%5],Hand::left)};
        float maximum=-1;for(std::size_t r=0;r<pair.size();++r)for(std::size_t l=0;l<pair.size();++l)
            if(r!=l&&equipmentMatches(combo.parts[0],pair[r])&&equipmentMatches(combo.parts[1],pair[l]))maximum=std::max(maximum,pair[r].damage+pair[l].damage);
        plan=chooseCombination(combo,pair);
        check(bool(plan)==(maximum>=0)&&(!plan||pair[(*plan)[0]].damage+pair[(*plan)[1]].damage==maximum),"Planner finds the strongest compatible pair without sharing copies");
    }
    EditorState editor;editor.open=true;editor.catalogVersion=3;combo.parts={sword,shield};
    EditCommand build;build.action=EditAction::saveCombination;build.catalogVersion=3;build.shortcut=combo;
    unsigned captures=0;
    auto composed=composeCombination(editor,build,[&](auto,auto)->std::optional<Entry>{++captures;return {};});
    check(composed&&captures==0,"Smart combinations are created directly, without existing group entries or item capture");
    EditCommand save=build;save.action=EditAction::saveShortcut;save.shortcut=*composed;
    check(editStructure(editor.draft,editor.options,save),"Smart combination enters the shared shortcut library");
    const auto id=editor.draft.groups[0].entries.back().shortcut;addShortcut(editor.draft,1,id);
    EditHistory history;history.begin(editor.draft,editor.options);history.prepare(editor.draft,editor.options);
    auto changed=*libraryShortcut(editor.draft,id);changed.parts[0]=smartEquipmentEntry(false,unsigned(WeaponFamily::axe));
    updateShortcut(editor.draft,id,changed);history.finish(true,false,0);
    check(editor.draft.groups[1].entries[0].parts[0].equipmentType==unsigned(WeaponFamily::axe),"Editing a smart component updates every group using the combination");
    check(history.undo(editor.draft,editor.options)&&editor.draft.groups[1].entries[0].parts[0].same(sword),"Undo restores the shared smart combination");
    const auto exported=exportPreset(editor.draft,editor.options,false,[](auto)->Json{throw std::runtime_error("Smart pair unexpectedly requested a form reference");});
    const auto imported=importPreset(exported,{},editor.options,false,[](auto&,auto){return 0u;});
    check(!imported.unresolved&&sameDefinition(imported.arsenal.groups[0].entries[0],editor.draft.groups[0].entries[0]),"Smart combinations transfer across characters with no captured item references");
    std::cout<<"PASS: "<<checks<<" smart equipment and tap checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

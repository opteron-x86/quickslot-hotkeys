#include "Recovery.h"
#include "SmartConsumables.h"
#include <iostream>
using namespace quickslot;
unsigned checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F> void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
int main()try {
    const auto identity=[](std::uint32_t old,std::uint32_t& mapped){mapped=old;return true;};
    Entry sword;sword.form=0x456;sword.owner=0x14;sword.unique=39;sword.name="Named sword";
    Entry axe;axe.form=0x789;axe.owner=0x14;axe.unique=40;axe.kind=Kind::axe;axe.name="War axe";
    Entry ammo;ammo.form=0x123;ammo.name="Steel Arrow";ammo.kind=Kind::ammo;
    Entry spell;spell.form=0x100;spell.kind=Kind::spell;spell.spell=true;spell.name="Healing";
    Entry pair;pair.kind=Kind::combo;pair.parts={sword,spell};pair.parts[1].hand=Hand::left;pair.name="Sword and healing";
    Arsenal arsenal;arsenal.presetIdentity="character-a";arsenal.active=1;
    arsenal.groups[0].add(sword);arsenal.groups[0].add(axe);arsenal.groups[0].add(ammo);
    arsenal.groups[1].add(smartEntry(0,0,2));arsenal.groups[2].add(pair);
    collectShortcuts(arsenal);
    const auto saved=encodeArsenal(arsenal);

    auto intact=recoverArsenal(saved,identity);
    check(intact&&!intact->dropped&&encodeArsenal(intact->arsenal)==saved,"A valid record loads unchanged");
    check(!blankArsenal(arsenal)&&blankArsenal(Arsenal{}),"Blank detection separates fresh state from saved groups");

    auto bad=Json::parse(saved);bad["groups"][0]["entries"][2]["hand"]=1u;
    rejects([&]{decodeArsenal(bad.dump(),identity);},"The strict decoder still rejects an invalid entry");
    auto repaired=recoverArsenal(bad.dump(),identity);
    check(repaired&&repaired->dropped==1,"One invalid entry is the only loss");
    check(repaired->arsenal.groups[0].entries.size()==2&&repaired->arsenal.groups[0].entries[0].same(sword)&&repaired->arsenal.groups[0].entries[1].same(axe),"Valid entries in the affected group survive in order");
    check(repaired->arsenal.groups[1].entries.size()==1&&repaired->arsenal.groups[2].entries.size()==1,"Other groups survive");
    check(repaired->arsenal.active==1&&repaired->arsenal.presetIdentity=="character-a"&&repaired->arsenal.shortcuts.size()==arsenal.shortcuts.size(),"Active group, lineage and shortcut library survive");
    check(repaired->arsenal.groups[0].selected<repaired->arsenal.groups[0].entries.size(),"The remembered index stays inside the repaired group");

    bad=Json::parse(saved);bad["shortcuts"][0]["kind"]=999u;
    repaired=recoverArsenal(bad.dump(),identity);
    check(repaired&&repaired->arsenal.shortcuts.size()+1==arsenal.shortcuts.size(),"An invalid shortcut definition is removed from the library");
    std::size_t entries=0;for(const auto& g:repaired->arsenal.groups)entries+=g.entries.size();
    check(entries==arsenal.size()-1&&repaired->dropped==2,"Entries linked to a removed shortcut leave their groups");

    bad=Json::parse(saved);bad["groups"][3]="broken";bad["active"]=99u;bad["presetIdentity"]=5;
    repaired=recoverArsenal(bad.dump(),identity);
    check(repaired&&repaired->arsenal.groups[3].entries.empty()&&repaired->arsenal.groups[0].entries.size()==3,"A malformed group becomes empty while its neighbors survive");
    check(repaired->arsenal.active==0&&repaired->arsenal.presetIdentity.empty(),"Invalid header values fall back to defaults");

    bad=Json::parse(saved);bad["groups"][0]["favorites"]={{"source",77u},{"hand",0u},{"excluded",Json::array()}};
    repaired=recoverArsenal(bad.dump(),identity);
    check(repaired&&!repaired->arsenal.groups[0].automatic()&&repaired->arsenal.groups[0].entries.size()==3,"An invalid Favorites link returns the group to its manual entries");

    bad=Json::parse(saved);bad["rememberedConsumable"]=encodeEntry(sword);
    repaired=recoverArsenal(bad.dump(),identity);
    check(repaired&&!repaired->arsenal.rememberedConsumable&&repaired->arsenal.size()==arsenal.size(),"An invalid selected consumable is cleared alone");

    bad=Json::parse(saved);bad["schema"]=arsenalSchema+1;
    check(!recoverArsenal(bad.dump(),identity),"A record from another schema is left for the build that wrote it");
    check(!recoverArsenal("{not json",identity)&&!recoverArsenal("[]",identity),"Unreadable records are reported as unreadable");

    Options options;options.themeId="oathvein";options.scale=1.25f;options.names[1]="Magic";options.showHints=true;options.groupCount=5;
    const auto settings=encodeOptions(options);
    auto restored=recoverOptions(settings);
    check(restored&&restored->reset.empty()&&encodeOptions(restored->options)==settings,"Valid settings load unchanged");

    auto partial=Json::parse(settings);partial.erase("showHints");partial.erase("controllerGroups");partial.erase("themeAdjustments");
    rejects([&]{decodeOptions(partial.dump());},"The strict settings decoder still requires its keys");
    restored=recoverOptions(partial.dump());
    check(restored&&restored->reset.empty(),"Missing keys take defaults");
    check(restored->options.themeId=="oathvein"&&restored->options.scale==1.25f&&restored->options.names[1]=="Magic"&&restored->options.groupCount==5,"Present settings survive missing keys");
    check(!restored->options.showHints&&restored->options.controllerGroups==Options{}.controllerGroups,"Missing settings use their defaults");

    partial=Json::parse(settings);partial["scale"]="large";partial["controllerGroups"]=Json::array({1,2});partial["futureSetting"]=true;
    restored=recoverOptions(partial.dump());
    check(restored&&restored->reset.size()==2,"Each unreadable setting is reported");
    check(restored->options.scale==Options{}.scale&&restored->options.themeId=="oathvein"&&restored->options.names[1]=="Magic","Unreadable settings reset alone");

    auto legacy=Json::parse(settings);legacy.erase("layoutVersion");legacy.erase("showHints");legacy["equipmentStyle"]=0u;legacy["equipmentLayout"]=0u;legacy["equipmentSpacing"]=2.f;
    restored=recoverOptions(legacy.dump());
    auto expected=legacy;expected["showHints"]=false;
    check(restored&&encodeOptions(restored->options)==encodeOptions(decodeOptions(expected.dump())),"Earlier layout values keep their migration");

    check(!recoverOptions("{oops")&&!recoverOptions("[]")&&!recoverOptions(R"({"schema":2})"),"Unreadable or newer settings files are reported as unreadable");
    std::cout<<"recovery: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

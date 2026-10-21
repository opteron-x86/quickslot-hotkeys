#include "EquipmentAction.h"
#include "EditorModel.h"
#include "ItemIdentity.h"
#include "Presets.h"
#include "LiveEditor.h"
#include <iostream>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(why)+" at line "+std::to_string(at.line()));
    ++checks;
}
template<class F> void rejects(F f,const char* why){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,why);}
int main()try {
    const auto identity=[](std::uint32_t id,std::uint32_t& out){out=id;return true;};
    const auto noForms=[](std::uint32_t,std::uint32_t&)->bool{throw std::runtime_error("Hand action requested a form lookup");};
    const auto right=emptyHandEntry(Hand::right),left=emptyHandEntry(Hand::left),unarmed=unarmedEntry();
    check(unsigned(Kind::ammo)==19&&unsigned(Kind::emptyHand)==20&&unsigned(Kind::unarmed)==21,"New kinds do not renumber saved equipment");
    Group group;
    for(const auto& action:{right,left,unarmed}) {
        check(validHandAction(action)&&!isInventory(action)&&!tracksInstance(action)&&!isStack(action)&&!isConsumable(action)&&!isVoice(action),"Actions have no inventory, spell or consumable identity");
        check(group.add(action),"Each action occupies its own slot");
        check(!group.add(action),"Adding the same action selects the existing slot");
        check(!selectsConsumable(action,{0,0,0,0,true},Options{}),"Release executes hand actions");
        check(rowStatus({action,true,true,{},0,0,false,{}}).find("Empty")!=std::string::npos,"Satisfied actions show empty instead of equipped");
    }
    check(group.entries.size()==3,"Three distinct actions survive deduplication");
    check(decode(encode(group),noForms).entries[2].same(unarmed),"Actions reload without form resolution");
    check(flexibleHand(left)&&!flexibleHand(unarmed)&&normalizedHand(unarmed,Hand::left)==Hand::right,"Empty hand can switch sides; unarmed always means both");
    check(!right.same(left)&&!right.same(unarmed),"Different targets never alias");
    auto corrupted=right;corrupted.form=123;check(!validHandAction(corrupted)&&!pairable(corrupted),"Hand actions cannot carry a hidden inventory reference");
    for(const auto& action:{right,left,unarmed})for(const auto& key:{"form","owner","unique","recipe","spell","twoHanded","unresolved","parts","exclusions","resource","effect","policy"}) {
        Group g;g.add(action);auto bad=Json::parse(encode(g));auto& e=bad["entries"][0];
        const std::string field=key;
        if(field=="spell"||field=="twoHanded"||field=="unresolved")e[key]=true;
        else if(field=="parts"||field=="exclusions")e[key]=Json::array();else e[key]=1u;
        rejects([&]{decode(bad.dump(),identity);},"Malformed action payload rejected");
    }
    auto bad=Json::parse(encode(group));bad["entries"][2]["hand"]=1u;rejects([&]{decode(bad.dump(),identity);},"Unarmed rejects a single-hand payload");
    bad=Json::parse(encode(group));bad["schema"]=4;rejects([&]{decode(bad.dump(),identity);},"Old schemas cannot masquerade as action data");
    rejects([&]{decodeArsenal(encode(group),noForms);},"A standalone group is rejected as a complete save");

    Entry sword;sword.form=0x1234;sword.owner=0x14;sword.unique=17;sword.name="Tempered sword";
    Entry shield;shield.form=0x4567;shield.owner=0x14;shield.unique=18;shield.name="Shield";shield.kind=Kind::shield;shield.hand=Hand::left;
    check(validPair(sword,left)&&validPair(right,shield)&&validPair(right,left),"Combinations accept either or both empty hands");
    check(!validPair(unarmed,left)&&!validPair(sword,unarmed)&&!validPair(left,shield),"A combination cannot embed an unarmed action or reversed hand");
    auto twoHanded=sword;twoHanded.twoHanded=true;check(!validPair(twoHanded,left),"Empty hand does not make a two-handed combination valid");
    Entry combo;combo.kind=Kind::combo;combo.name="Sword only";combo.parts={sword,left};
    Group combos;combos.add(combo);const auto reloaded=decode(encode(combos),identity);
    check(reloaded.entries[0].same(combo)&&reloaded.entries[0].parts[0].unique==17,"Combination preserves exact sword and empty-hand action");
    bad=Json::parse(encode(combos));bad["entries"][0]["parts"][1]=encodeEntry(unarmed);
    rejects([&]{decode(bad.dump(),identity);},"Nested unarmed is rejected on import/load");
    auto missing=decode(encode(combos),[](auto,auto&){return false;});
    check(missing.entries[0].parts[0].unresolved&&!missing.entries[0].parts[1].unresolved,"Missing gear does not make the empty action unresolved");

    Arsenal arsenal;Options options;EditCommand command;command.action=EditAction::saveShortcut;command.shortcut=unarmed;
    check(editStructure(arsenal,options,command),"Editor adds a direct action");command.shortcut=right;check(editStructure(arsenal,options,command),"Editor adds an empty right hand");
    command.action=EditAction::hand;command.index=1;command.hand=Hand::left;
    check(editGroup(arsenal,command)&&arsenal.groups[0].entries[1].name=="Empty left hand","Changing hand updates action and label together");
    command.action=EditAction::saveShortcut;command.shortcut=combo;check(editStructure(arsenal,options,command),"Editor adds gear plus empty hand");
    command.shortcut=corrupted;check(!editStructure(arsenal,options,command),"Editor refuses malformed action state");
    auto before=encodeArsenal(arsenal);EditHistory history;history.begin(arsenal,options);history.prepare(arsenal,options);
    command.shortcut=right;command.replace=true;command.index=0;check(editStructure(arsenal,options,command),"Action edit replaces only its slot");history.finish(true,false,1);
    check(history.undo(arsenal,options)&&encodeArsenal(arsenal)==before,"Undo restores action types and combination identities");
    arsenal.presetIdentity="source";Arsenal target;target.presetIdentity="other";
    const auto preset=exportPreset(arsenal,options,false,[](auto id){return Json{{"file","Example.esp"},{"local",id}};});
    const auto imported=importPreset(preset,target,options,false,[](const auto&,auto id){return id;});
    check(imported.unresolved==1&&!imported.arsenal.groups[0].entries[0].unresolved&&!imported.arsenal.groups[0].entries[1].unresolved,"Actions remain portable when exact equipment cannot cross characters");
    check(imported.arsenal.groups[0].entries[2].parts[0].unresolved&&validHandAction(imported.arsenal.groups[0].entries[2].parts[1]),"Foreign combination retains empty-hand component");
    Arsenal onlyActions;onlyActions.groups[0]=group;
    const auto portable=exportPreset(onlyActions,options,false,[](auto)->Json{throw std::runtime_error("Action exported a fake form");});
    check(Json::parse(portable)["references"].empty(),"Action-only preset has no plugin requirements");
    check(importPreset(portable,target,options,false,[](const auto&,auto)->std::uint32_t{throw std::runtime_error("Action imported a fake form");}).unresolved==0,"Action-only preset imports with no form lookup");

    const std::array<HandOccupancy,2> empty{},dual{{{true,false},{true,false}}},bow{{{true,true},{false,false}}},leftRitual{{{false,false},{true,true}}},shieldOnly{{{false,false},{true,false}}};
    check(!handClearTarget(empty,Hand::right)&&!handClearTarget(empty,Hand::left),"Already empty hands generate no native action");
    check(handClearTarget(dual,Hand::right)==Hand::right&&handClearTarget(dual,Hand::left)==Hand::left,"Independent equipped hands target only the requested side");
    check(!handClearTarget(shieldOnly,Hand::right)&&handClearTarget(shieldOnly,Hand::left)==Hand::left,"Clearing empty right hand preserves a left shield");
    check(handClearTarget(bow,Hand::left)==Hand::right&&handClearTarget(bow,Hand::right)==Hand::right,"Clearing either hand removes a two-handed weapon held in right slot");
    check(handClearTarget(leftRitual,Hand::right)==Hand::left,"Two-handed magic in left slot also blocks the right hand");
    Entry torch;torch.kind=Kind::torch;torch.hand=Hand::left;
    check(!objectEquipHand(torch,false)&&!objectEquipHand(torch,true),"Torch equip and removal use native light slots");
    check(wornForEntry(torch,true,false)&&wornForEntry(torch,false,true)&&!wornForEntry(torch,false,false),"Torch confirmation recognizes either worn marker, not an unworn copy");
    {
        auto light=torch;light.form=0x1d4ec;light.name="Torch";
        check(isStack(light)&&!tracksInstance(light)&&!isConsumable(light),"Torches follow stock while retaining equipment behavior");
        check(validPair(sword,light)&&!validPair(light,sword),"Torch remains a left-hand combination item");
        check(!selectsConsumable(light,{0,0,0,0,true},Options{}),"Browsing a torch equips it on release");
        for(unsigned state=0;state<3;++state) {
            auto old=light;old.owner=state==1?0xff001234:0x14;old.unique=23;
            if(state==2){old.owner=0;old.unique=0;old.unresolved=true;}
            Group saved;saved.add(sword);saved.add(old);
            const auto loaded=decode(encode(saved),[&](auto id,auto& out){
                if(id==light.form){out=id+0x1000000;return true;}
                if(id==sword.form||id==sword.owner){out=id;return true;}
                throw std::runtime_error("Torch must not resolve a discarded copy owner");
            });
            const auto& restored=loaded.entries[1];
            check(loaded.selected==1&&restored.form==light.form+0x1000000&&!restored.unresolved&&!restored.owner&&!restored.unique,"Saved torch repairs from its base form without changing selection");
            check(loaded.entries[0].same(sword),"Repair leaves exact weapon identity unchanged");
            check(decode(encode(loaded),identity).entries[1].same(restored),"Repaired torch stays stable on subsequent loads");
        }
        Group stock;stock.add(light);const auto initial=encode(stock);
        check(followIdentityChange(stock,{light.form,0x14,0,23,0})==0&&encode(stock)==initial,"Dropping the final torch cannot invalidate its shortcut");
        check(followIdentityChange(stock,{light.form,0,0x14,0,24})==0&&encode(stock)==initial,"Picking up a new torch identity keeps the same shortcut");
        check(!stock.add(light)&&stock.entries.size()==1,"Restocked torches do not create duplicate shortcuts");
        auto otherLight=light;++otherLight.form;check(stock.add(otherLight),"A different torch base form remains a separate choice");
        Row depleted{light,false,false,{},0};check(rowStatus(depleted)=="Empty","Depletion shows recoverable stock instead of a missing copy");
        Row held{light,true,true,{},3};check(rowStatus(held)=="Left hand · ×3 · Equipped","Held torch shows its hand, equipped state and current stock");
        auto missing=decode(encode(stock),[](auto,auto&){return false;});
        check(missing.entries[0].unresolved&&!missing.entries[0].form,"Missing torch plugin clears the base reference");
        check(decode(encode(missing),identity).entries[0].unresolved,"A lost base reference cannot bind a reused form ID");
        Group exact;exact.add(sword);check(followIdentityChange(exact,{sword.form,sword.owner,0,sword.unique,0})==1,"Weapons still process removed copy identities");
        check(decode(encode(exact),identity).entries[0].unresolved,"Weapon with erased identity is not restored from base form alone");
        for(const auto* field:{"spell","twoHanded","hand","recipe"}) {
            auto data=Json::parse(encode(stock));auto& entry=data["entries"][0];
            if(std::string(field)=="hand")entry[field]=0u;
            else if(std::string(field)=="recipe")entry[field]=1u;
            else entry[field]=true;
            rejects([&]{decode(data.dump(),identity);},"Torch repair rejects a malformed hand, spell or recipe");
        }
        Arsenal shared;Options opts;Entry pair;pair.kind=Kind::combo;pair.name="Sword and torch";pair.parts={sword,light};
        EditCommand add;add.action=EditAction::saveShortcut;add.shortcut=pair;
        check(editStructure(shared,opts,add),"Create reusable sword-and-torch combination");
        const auto shortcut=shared.groups[0].entries[0].shortcut;
        check(addShortcut(shared,1,shortcut),"Reuse torch combination in another group");
        eachSavedEntry(shared,[](Entry& entry){eachLeaf(entry,[](Entry& leaf){if(leaf.kind==Kind::torch){leaf.unresolved=true;leaf.owner=leaf.unique=0;}});});
        shared=decodeArsenal(encodeArsenal(shared),identity);
        check(libraryShortcut(shared,shortcut)->parts[1].same(light)&&shared.groups[0].entries[0].parts[1].same(light)&&shared.groups[1].entries[0].parts[1].same(light),"Loading repairs the library and every use of a depleted torch combination");
        check(followIdentityChange(shared,{light.form,0x14,0,23,0})==0,"Later torch identity removal leaves all combination uses intact");
        shared.presetIdentity="source";Arsenal foreign;foreign.presetIdentity="other";
        const auto exported=exportPreset(shared,opts,false,[](auto id){return Json{{"file","Test.esp"},{"local",id}};});
        const auto imported=importPreset(exported,foreign,opts,false,[](const auto&,auto id){return id;});
        const auto& importedPair=imported.arsenal.groups[0].entries[0];
        check(importedPair.parts[1].same(light)&&importedPair.parts[0].unresolved,"Foreign presets retain torch stock but cannot substitute a specific sword");
        EditHistory undo;undo.begin(shared,opts);undo.prepare(shared,opts);
        check(deleteShortcut(shared,shortcut),"Remove the shared torch combination");undo.finish(true,false,1);
        check(undo.undo(shared,opts)&&libraryShortcut(shared,shortcut)->parts[1].same(light)&&shared.groups[1].entries[0].parts[1].same(light),"Undo restores torch stock in the definition and all groups");
    }
    Entry hammer;hammer.kind=Kind::greatsword;hammer.twoHanded=true;hammer.hand=Hand::left;
    check(!objectEquipHand(hammer,false)&&objectEquipHand(hammer,true)==Hand::right,"Clearing a two-handed weapon targets its worn slot from either hand");
    Entry dagger;dagger.kind=Kind::dagger;dagger.hand=Hand::left;
    check(objectEquipHand(dagger,true)==Hand::left&&objectEquipHand(dagger,true,true)==Hand::right,"One-handed removal and cross-hand moves preserve explicit hand targeting");
    check(wornForEntry(dagger,false,true)&&!wornForEntry(dagger,true,false),"A different right-hand copy cannot satisfy a left-hand shortcut");
    Entry ammo;ammo.kind=Kind::ammo;check(!objectEquipHand(ammo,false)&&!objectEquipHand(ammo,true),"Ammo retains native equipment slot");
    check(equipmentSequence(unarmed)&&!equipmentSequence(right)&&equipmentStep(unarmed,0).same(right)&&equipmentStep(unarmed,1).same(left),"Unarmed clears right then left through confirmed sequence");
    check(!skipEquipmentStep(left,false,false)&&skipEquipmentStep(left,false,true),"Empty-hand combination executes only while occupied");
    check(skipEquipmentStep(left,true,false)&&skipEquipmentStep(left,true,true),"Unequipping a combination never executes its empty-hand component");
    check(!skipEquipmentStep(sword,true,true)&&skipEquipmentStep(sword,true,false),"Normal combination removal still targets matching equipped gear");
    using P=EquipmentProgress;using Phase=EquipmentPhase;
    check(equipmentProgress(Phase::equipping,true,true,false,false,false)==P::wait,"Clearing waits for the engine to report empty");
    check(equipmentProgress(Phase::equipping,true,true,true,false,false)==P::complete,"Next hand starts only after the first is clear");
    check(equipmentProgress(Phase::equipping,true,true,false,false,true)==P::timeout,"A blocked clear stops instead of repeating native unequip");
    check(equipmentProgress(Phase::equipping,false,true,true,false,false)==P::cancelled,"Cancellation wins over apparent completion");
    Selection selection;selection.press(0,group);selection.cycleAvailable(1,group,{true,true,true});auto commit=selection.release(group);
    check(commit&&commit->browsed,"Actions participate in carousel selection");selection.cancel();check(!selection.accepts(*commit,group),"Cancelled action cannot proceed to another hand");
    selection.press(1,group);commit=selection.release(group);group.remove();check(!selection.accepts(*commit,group),"Changing action list invalidates pending requests");
    std::cout<<"PASS: "<<checks<<" hand action, combination, persistence and cancellation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

#pragma once
#include "LiveEditor.h"
#include "Presets.h"
namespace quickslot::fixture {
inline void rows(EditorState& s) {
    for(std::size_t i=0;i<Arsenal::count;++i){s.rows[i].clear();for(const auto& e:s.draft.groups[i].entries) {
        int count=0;for(const auto& c:s.catalog)if(c.entry.form==e.form)count=c.count;
        s.rows[i].push_back({e,!e.unresolved&&(!isStack(e)||count>0),false,{},isStack(e)?count:0});
    }}
}
inline EditorState editor() {
    EditorState s;s.open=true;s.session=1;s.revision=1;s.catalogVersion=1;s.options.names[0]="Arsenal";s.options.names[1]="Offhand";s.options.names[2]="Magic";
    const std::pair<const char*,Kind> examples[]={{"Imperial Sword (Superior)",Kind::sword},{"Iron Mace",Kind::mace},{"Steel Dagger",Kind::dagger},{"Long Bow (Fine)",Kind::bow},{"Flames",Kind::spell},{"Healing",Kind::spell},{"Iron Shield",Kind::shield},{"Torch",Kind::torch},{"Chain Lightning",Kind::spell},{"Enchanted Dragonbone Greatsword of the Inferno",Kind::greatsword}};
    unsigned id=1;
    for(const auto& [name,kind]:examples){Entry e;e.form=0x123400+id;e.name=name;e.kind=kind;e.spell=kind==Kind::spell;if(tracksInstance(e)){e.owner=0x14;e.unique=std::uint16_t(id);}e.twoHanded=kind==Kind::greatsword||kind==Kind::bow;e.hand=normalizedHand(e,Hand::right);s.catalog.push_back({e,id,1,id==1,true,{}});++id;}
    for(const auto& [name,kind]:std::vector<std::pair<const char*,Kind>>{{"Potion of Healing",Kind::potion},{"Vegetable Soup",Kind::food},{"Weak Poison",Kind::poison},{"Scroll of Fireball",Kind::scroll},{"Battle Cry",Kind::power},{"Night Eye",Kind::lesserPower},{"Unrelenting Force",Kind::shout},{"Steel Arrow",Kind::ammo}}) {
        Entry e;e.form=0x123400+id;e.name=name;e.kind=kind;e.spell=kind==Kind::power||kind==Kind::lesserPower;
        s.catalog.push_back({e,id,isStack(e)?7:1,false,true,{}});++id;
    }
    s.draft.groups[0].add(s.catalog[0].entry);s.draft.groups[0].add(s.catalog[1].entry);s.draft.groups[0].add(s.catalog[3].entry);
    auto left=s.catalog[2].entry;left.hand=Hand::left;s.draft.groups[1].add(left);
    s.draft.groups[2].add(s.catalog[4].entry);s.draft.groups[2].add(s.catalog[8].entry);
    s.draft.groups[0].selected=0;
    s.equippedCandidates={1,0};
    for(unsigned i=0;i<3;++i){PotionCandidate c;c.entry=s.catalog[10].entry;c.entry.form+=i*100;c.entry.name=i==0?"Potion of Healing":i==1?"Potion of Vigorous Healing":"Rare crafted potion";c.count=7;c.restore[0]=float(25u<<i);c.regeneration[0]=float(25u<<i);s.potions.push_back(c);}
    for(const auto& item:s.catalog)if(unsigned(item.entry.kind)<=unsigned(Kind::mace)||item.entry.kind==Kind::bow||item.entry.kind==Kind::ammo||item.entry.kind==Kind::shield) {
        EquipmentCandidate c;c.entry=item.entry;c.count=item.count;c.damage=item.entry.kind==Kind::sword?26.f:item.entry.kind==Kind::mace?22.f:14.f;
        c.family=item.entry.kind==Kind::shield?WeaponFamily::shield:item.entry.kind==Kind::mace?WeaponFamily::mace:item.entry.kind==Kind::dagger?WeaponFamily::dagger:item.entry.kind==Kind::greatsword?WeaponFamily::greatsword:item.entry.kind==Kind::bow?WeaponFamily::bow:WeaponFamily::sword;
        c.rightEquipped=item.equipped;if(c.entry.kind==Kind::ammo)c.entry.icon=Icon::arrow;s.equipment.push_back(c);
    }
    s.deficits={60,30,80};rows(s);
    s.equippedDisplay.hands[0]=Row{s.catalog[0].entry,true,true};
    s.equippedDisplay.hands[1]=Row{s.catalog[6].entry,true,true};
    s.equippedDisplay.consumable=Row{s.catalog[10].entry,true,false,{},7};
    s.equippedDisplay.voice=Row{s.catalog[16].entry,true,true,{},0,18};
    return s;
}
inline bool apply(EditorState& s,const EditCommand& c,EditHistory& history) {
    if(c.action==EditAction::close&&c.session==s.session){if(c.hasSettings)s.options=c.options;s.open=false;return true;}
    if(!matchingEdit(s,c))return false;
    static std::map<std::string,std::string> presets;
    if(c.action==EditAction::exportPreset){presets[c.presetName]=exportPreset(s.draft,s.options,c.appearanceOnly,[](std::uint32_t id){return Json{{"file","Fixture.esp"},{"local",id}};});if(std::find(s.presets.begin(),s.presets.end(),c.presetName)==s.presets.end())s.presets.push_back(c.presetName);return true;}
    if(c.action==EditAction::refreshPresets)return true;
    if(c.action==EditAction::importPreset){auto result=importPreset(presets.at(c.presetName),s.draft,s.options,c.appearanceOnly,[](const auto&,std::uint32_t id){return id;});history.prepare(s.draft,s.options);s.draft=result.arsenal;s.options=result.options;history.finish(true,false,0);s.canUndo=history.canUndo();s.canRevert=history.canRevert();++s.revision;++s.restoreSerial;rows(s);return true;}
    if(c.action==EditAction::undo||c.action==EditAction::revert) {
        const auto changed=c.action==EditAction::undo?history.undo(s.draft,s.options):history.revert(s.draft,s.options);
        if(!changed)return false;++s.restoreSerial;
    }else {
        history.prepare(s.draft,s.options);
        if(c.action==EditAction::settings)s.options=c.options;
        else if(c.action==EditAction::refresh)++s.catalogVersion;
        else if(c.action==EditAction::add) {
            if(c.catalogVersion!=s.catalogVersion){history.finish(false,false,0);return false;}
            auto it=std::find_if(s.catalog.begin(),s.catalog.end(),[&](const auto& v){return v.id==c.candidate;});
            if(it==s.catalog.end()){history.finish(false,false,0);return false;}
            auto e=it->entry;e.hand=normalizedHand(e,c.hand);s.draft.groups[c.group].add(e);
        }else if(c.action==EditAction::saveCombination) {
            const auto pair=composeCombination(s,c,[&](std::uint64_t id,Hand hand)->std::optional<Entry>{const auto* item=catalogItem(s,id);if(!item)return {};auto e=item->entry;e.hand=hand;return e;});
            if(!pair){history.finish(false,false,0);return false;}
            auto save=c;save.action=EditAction::saveShortcut;save.shortcut=*pair;
            if(!editStructure(s.draft,s.options,save)){history.finish(false,false,0);return false;}
        }else if(!editStructure(s.draft,s.options,c)&&!editGroup(s.draft,c)){history.finish(false,false,0);return false;}
        history.finish(c.action!=EditAction::select&&c.action!=EditAction::refresh,c.continuous,double(s.revision));
    }
    normalizeGroupModes(s.options,s.draft);s.canUndo=history.canUndo();s.canRevert=history.canRevert();++s.revision;rows(s);return true;
}
}

#include "Controller.h"
#include "GameplayMenus.h"
#include "EquippedState.h"
#include "HudMotion.h"
#include "Layout.h"
#include "MenuAPI.h"
#include "EditorLaunch.h"
#include "EditorTextInput.h"
#include "Presets.h"
#include "LiveEditor.h"
#include "VoiceAction.h"
#include <iostream>
#include <source_location>

using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));
    ++checks;
}
int main()try {
    {
        Options keys;keys.groupRemoveHotkey=false;
        check(groupKeyAction(keys,keyModifier::alt)==GroupKeyAction::pass,"Existing disabled Alt editing still passes through by default");
        keys.allowUnusedModifiers=true;
        check(groupKeyAction(keys,keyModifier::alt)==GroupKeyAction::select,"Opt-in lets an Alt-walking user select groups without removing entries");
        keys.groupRemoveHotkey=true;assignGroupModifier(keys,GroupModifier::remove,keyModifier::ctrl|keyModifier::alt);
        check(groupKeyAction(keys,keyModifier::alt)==GroupKeyAction::select&&groupKeyAction(keys,keyModifier::ctrl|keyModifier::alt)==GroupKeyAction::remove,"Remapping remove frees Alt while the explicit editing chord still works");
        check(groupKeyAction(keys,keyModifier::ctrl)==GroupKeyAction::captureRight,"An unrelated remapping preserves capture");
        assignGroupModifier(keys,GroupModifier::remove,keyModifier::shift);
        check(keys.groupModifiers[unsigned(GroupModifier::unequip)]==(keyModifier::ctrl|keyModifier::alt),"Reusing an editing modifier swaps the existing action");
        check(assignSelectorBinding(keys,KeyboardAction::previous,{16,keyModifier::alt}),"Navigation can use a custom keyboard chord");
        check(selectorKeyAction(keys,16,keyModifier::alt).action==SelectorKeyAction::browse&&selectorKeyAction(keys,16,keyModifier::ctrl|keyModifier::alt).action==SelectorKeyAction::reorder,"Custom navigation supports normal browsing and the reorder modifier");
        check(selectorKeyAction(keys,16,0u).action==SelectorKeyAction::pass,"The unmodified custom key remains available to gameplay");
        check(!assignSelectorBinding(keys,KeyboardAction::cancel,{2})&&!assignSelectorBinding(keys,KeyboardAction::cancel,{56}),"Action bindings cannot steal a group key or bind a modifier alone");
        check(!assignGroupKey(keys,0,16),"Group bindings cannot steal a custom action key");
        check(assignSelectorBinding(keys,KeyboardAction::cancel,{16})&&keys.selectorBindings[unsigned(KeyboardAction::previous)].key==1,"An unmodified action cannot shadow another action's chord on the same key");
        check(assignSelectorBinding(keys,KeyboardAction::remove,{14})&&keys.selectorBindings[unsigned(KeyboardAction::unequip)].key==211,"Duplicate action chords swap instead of leaving an unreachable action");
        keys.controllerActions[unsigned(ControllerAction::unequip)]={PadButton::y,PadButton::x};
        const auto saved=decodeOptions(encodeOptions(keys));
        check(saved.groupModifiers==keys.groupModifiers&&saved.selectorBindings==keys.selectorBindings&&saved.controllerActions==keys.controllerActions&&saved.allowUnusedModifiers,"New bindings and modifier policy survive settings reload");
        importAppearance(keys,Options{});
        check(keys.selectorBindings==saved.selectorBindings&&keys.groupModifiers==saved.groupModifiers&&keys.controllerActions==saved.controllerActions&&keys.allowUnusedModifiers,"Appearance presets preserve remapped actions");
        auto legacy=Json::parse(encodeOptions(Options{}));for(const auto key:{"groupModifiers","selectorBindings","controllerActions","allowUnusedModifiers"})legacy.erase(key);
        const auto migrated=decodeOptions(legacy.dump());
        check(migrated.groupModifiers==defaultGroupModifiers&&migrated.selectorBindings==defaultSelectorBindings&&migrated.controllerActions==defaultControllerActions&&!migrated.allowUnusedModifiers,"Old settings retain all original controls and pass-through behavior");
        legacy["groupRemoveHotkey"]=false;legacy["selectorUnequipHotkey"]=false;
        auto disabled=decodeOptions(legacy.dump());
        check(!groupActionEnabled(disabled,GroupModifier::remove)&&selectorBinding(disabled,KeyboardAction::unequip)==KeyBinding{},"Older disabled shortcuts appear as None after migration");
        check(!selectorKeyReserved(disabled,14)&&selectorKeyAction(disabled,14,false).action==SelectorKeyAction::pass,"A disabled legacy key is free for another binding and passes through");
        check(assignSelectorBinding(disabled,KeyboardAction::unequip,{14})&&selectorKeyAction(disabled,14,false).action==SelectorKeyAction::unequip,"Rebinding a migrated disabled action enables it without a hidden toggle");
        assignGroupModifier(disabled,GroupModifier::remove,keyModifier::shift);
        check(disabled.groupRemoveHotkey&&!disabled.groupUnequipHotkey&&groupKeyAction(disabled,keyModifier::shift)==GroupKeyAction::remove,"Assigning an occupied modifier from None moves the previous action to None");
        assignGroupModifier(disabled,GroupModifier::remove,0);disabled.allowUnusedModifiers=true;
        check(groupKeyAction(disabled,keyModifier::shift)==GroupKeyAction::select,"A cleared group modifier permits normal selection with the unused-modifier opt-in");
        assignSelectorBinding(disabled,KeyboardAction::remove,{});
        assignSelectorBinding(disabled,KeyboardAction::unequip,{211});
        check(selectorKeyAction(disabled,211,false).action==SelectorKeyAction::unequip&&!disabled.selectorRemoveHotkey,"A cleared action key can be reassigned without reviving the old action");
        const auto disabledSaved=decodeOptions(encodeOptions(disabled));
        check(!disabledSaved.groupRemoveHotkey&&!disabledSaved.selectorRemoveHotkey&&selectorBinding(disabledSaved,KeyboardAction::unequip).key==211,"None and reassigned bindings survive save/load together");
        auto invalid=keys;invalid.selectorBindings[0]={999,999};invalid.controllerActions[0]={PadButton(999)};sanitizeOptions(invalid);
        check(invalid.selectorBindings[0]==KeyBinding{}&&invalid.controllerActions[0]==PadBinding{},"Invalid saved action bindings are disabled safely");
    }
    const auto shortcutFlags=[](const Options& o){return std::array{
        o.groupCaptureRightHotkey,o.groupCaptureLeftHotkey,o.groupMoveHotkey,o.groupUnequipHotkey,
        o.groupRemoveHotkey,o.selectorReorderHotkey,o.selectorUnequipHotkey,o.selectorRemoveHotkey};};
    for(unsigned mask=0;mask<256;++mask) {
        Options keys;
        keys.groupCaptureRightHotkey=mask&1;keys.groupCaptureLeftHotkey=mask&2;keys.groupMoveHotkey=mask&4;
        keys.groupUnequipHotkey=mask&8;keys.groupRemoveHotkey=mask&16;
        keys.selectorReorderHotkey=mask&32;keys.selectorUnequipHotkey=mask&64;keys.selectorRemoveHotkey=mask&128;
        const auto expected=shortcutFlags(keys);
        check(groupKeyAction(keys,0)==GroupKeyAction::select,"Bare group keys work with every combination of editing switches");
        check(groupKeyAction(keys,keyModifier::shift)==(keys.groupUnequipHotkey?GroupKeyAction::unequip:GroupKeyAction::pass),"Disabled Shift shortcut passes through without plain group activation");
        check(groupKeyAction(keys,keyModifier::alt)==(keys.groupRemoveHotkey?GroupKeyAction::remove:GroupKeyAction::pass),"Disabled Alt shortcut passes through without deleting or selecting");
        check(groupKeyAction(keys,keyModifier::shift|keyModifier::alt)==(keys.groupRemoveHotkey?GroupKeyAction::remove:GroupKeyAction::pass),"Alt retains precedence and cannot fall through to Shift unequip");
        for(const unsigned extra:{0u,keyModifier::alt}) {
            check(groupKeyAction(keys,keyModifier::ctrl|extra)==(keys.groupCaptureRightHotkey?GroupKeyAction::captureRight:GroupKeyAction::pass),"Right capture can be disabled independently without falling through");
            check(groupKeyAction(keys,keyModifier::ctrl|keyModifier::shift|extra)==(keys.groupCaptureLeftHotkey?GroupKeyAction::captureLeft:GroupKeyAction::pass),"Left capture can be disabled independently without falling through");
            for(const unsigned shift:{0u,keyModifier::shift})
                check(groupKeyAction(keys,keyModifier::ctrl|shift|extra,true)==(keys.groupMoveHotkey?GroupKeyAction::move:GroupKeyAction::pass),"Moving is independent of capture and never falls back to adding an item");
        }
        check(menuCaptureHand(keys,keyModifier::ctrl)==(keys.groupCaptureRightHotkey?std::optional{Hand::right}:std::nullopt),"Right capture switch also governs Inventory and Magic");
        check(menuCaptureHand(keys,keyModifier::ctrl|keyModifier::shift)==(keys.groupCaptureLeftHotkey?std::optional{Hand::left}:std::nullopt),"Left capture switch also governs Inventory and Magic");
        check(!menuCaptureHand(keys,0)&&!menuCaptureHand(keys,keyModifier::shift),"Unmodified and Shift-only menu bindings belong to Skyrim");
        keys.menuCapture=false;
        check(!menuCaptureHand(keys,keyModifier::ctrl)&&!menuCaptureHand(keys,keyModifier::ctrl|keyModifier::shift),"Menu-only opt-out still overrides enabled capture shortcuts");
        check(groupKeyAction(keys,keyModifier::ctrl)==(keys.groupCaptureRightHotkey?GroupKeyAction::captureRight:GroupKeyAction::pass),"Menu-only opt-out preserves gameplay capture preferences");
        for(const unsigned scan:{203u,205u}) {
            const auto plain=selectorKeyAction(keys,scan,false),ctrl=selectorKeyAction(keys,scan,true);
            check(plain.action==SelectorKeyAction::browse&&plain.delta==(scan==203?-1:1),"Plain arrows retain browsing with editing disabled");
            check(ctrl.action==(keys.selectorReorderHotkey?SelectorKeyAction::reorder:SelectorKeyAction::pass),"Disabled Ctrl-arrow reordering passes through without browsing");
            check(ctrl.delta==(keys.selectorReorderHotkey?plain.delta:0),"Disabled reordering cannot queue a movement");
        }
        check(selectorKeyAction(keys,14,false).action==(keys.selectorUnequipHotkey?SelectorKeyAction::unequip:SelectorKeyAction::pass),"Backspace editing is independently optional");
        check(selectorKeyAction(keys,211,false).action==(keys.selectorRemoveHotkey?SelectorKeyAction::remove:SelectorKeyAction::pass),"Delete editing is independently optional");
        check(selectorUsesCtrl(keys)==bool(mask&36),"Ctrl itself passes through when moving and reordering are disabled, even if capture remains enabled");
        check(shortcutFlags(decodeOptions(encodeOptions(keys)))==expected,"All editing switches round-trip independently");
        importAppearance(keys,Options{});
        check(shortcutFlags(keys)==expected,"Appearance presets preserve every local editing switch");
    }
    Options all;setEditingShortcuts(all,false);
    check(shortcutFlags(all)==std::array<bool,8>{},"Disable all includes every editing shortcut");
    setEditingShortcuts(all,true);
    check(shortcutFlags(all)==shortcutFlags(Options{}),"Enable all restores the existing default shortcuts");
    auto oldSettings=Json::parse(encodeOptions(Options{}));
    for(const auto field:{"groupCaptureRightHotkey","groupCaptureLeftHotkey","groupMoveHotkey","groupUnequipHotkey","groupRemoveHotkey","selectorReorderHotkey","selectorUnequipHotkey","selectorRemoveHotkey"})oldSettings.erase(field);
    check(shortcutFlags(decodeOptions(oldSettings.dump()))==shortcutFlags(Options{}),"Older settings retain every existing editing shortcut");
    Options o;Arsenal a;auto& group=a.current();
    Entry sword;sword.form=1;sword.name="Sword";group.add(sword);
    Entry spell;spell.form=2;spell.kind=Kind::spell;spell.spell=true;spell.name="Flames";group.add(spell);
    Entry potion;potion.form=3;potion.kind=Kind::potion;potion.name="Healing";group.add(potion);
    group.selected=0;
    check(nextEntry(group,{},false)==1,"Tap advances");
    check(nextEntry(group,{true,false,true},true)==2,"Unavailable entry skipped");
    check(nextEntry(group,{false,false,false},true)==0,"All unavailable retains selection");
    group.selected=2;check(nextEntry(group,{},false)==0,"Cycle wraps");group.selected=0;
    GroupRecall recall;
    const auto tap=[&](std::size_t id,Group& g,const std::vector<bool>& available=std::vector<bool>{},bool skip=false){return g.selected=cycleEntry(g,available,skip,recall.activate(id));};
    check(tap(0,group)==0&&tap(0,group)==1,"First press recalls; next press advances");
    Group second=group;second.selected=2;
    check(tap(1,second)==2&&tap(0,group)==1&&tap(0,group)==2,"Switching hotkeys retains each cycle group's selection");
    recall.activate(2);
    check(tap(0,group)==2&&tap(0,group)==0,"Returning from a browse group recalls, then wraps normally");
    recall.reset();check(tap(0,group)==0,"Loading or editing starts with recall");
    recall.activate(1);group.selected=1;
    check(tap(0,group,{true,false,true},false)==1,"Recall keeps an unavailable choice when skipping is disabled");
    recall.activate(1);check(tap(0,group,{true,false,true},true)==2,"Skipping unavailable recalls the next available choice");
    recall.activate(1);check(tap(0,group,{false,false,false},true)==2,"All unavailable keeps the remembered slot");
    Group empty;check(tap(3,empty)==0&&tap(3,empty)==0,"Empty groups never produce an out-of-range cycle");
    {
        Options protectedOptions;
        check(protectedOptions.cycleKeepOtherHand,"Cycling protection starts enabled");
        auto legacy=Json::parse(encodeOptions(protectedOptions));legacy.erase("cycleKeepOtherHand");
        check(decodeOptions(legacy.dump()).cycleKeepOtherHand,"Existing settings enable cycling protection");
        protectedOptions.cycleKeepOtherHand=false;
        check(!decodeOptions(encodeOptions(protectedOptions)).cycleKeepOtherHand,"Opting out survives saved settings");
        importAppearance(protectedOptions,Options{});
        check(!protectedOptions.cycleKeepOtherHand,"Appearance imports preserve cycling protection preference");
        const auto setup=exportPreset(Arsenal{},protectedOptions,false,[](auto){return Json{};});
        check(!importPreset(setup,Arsenal{},Options{},false,[](const auto&,auto id){return id;}).options.cycleKeepOtherHand,"Complete presets restore the cycling protection preference");

        Group weapons;auto held=sword;held.form=100;held.owner=20;held.unique=1;held.hand=Hand::left;
        auto spare=held;spare.unique=2;auto dagger=held;dagger.form=101;dagger.kind=Kind::dagger;dagger.unique=3;
        weapons.add(held);weapons.add(spare);weapons.add(dagger);weapons.selected=0;
        const std::vector<bool> blocked{movesOtherHandWeapon(held,false,true),movesOtherHandWeapon(spare,false,false),movesOtherHandWeapon(dagger,true,false)};
        check(blocked==std::vector<bool>({true,false,false}),"Only the exact occupied copy is blocked; same-form spare and target-hand copy remain eligible");
        for(bool skip:{false,true}) {
            check(cycleEntry(weapons,{true,true,true},skip,true,blocked)==1,"Recall skips an occupied copy with either unavailable-item setting");
            check(cycleEntry(weapons,{true,true,true},skip,false,blocked)==1,"Advance skips an occupied copy");
            weapons.selected=2;
            check(cycleEntry(weapons,{true,true,true},skip,false,blocked)==1,"Wraparound skips the opposite-hand copy");
            weapons.selected=0;
        }
        check(cycleEntry(weapons,{true,false,true},true,true,blocked)==2,"Protection composes with skipping unavailable entries");
        check(cycleEntry(weapons,{true,false,true},false,true,blocked)==1,"Unavailable entries retain their configured cycling behavior");
        check(!cycleEntry(weapons,{true,false,false},true,true,blocked),"No eligible available weapon produces no cycle action");
        weapons.selected=1;
        check(cycleEntry(weapons,{false,false,false},true,false,blocked)==1,"Unavailable unoccupied selection retains the existing fallback");
        check(!cycleEntry(weapons,{true,true,true},false,false,{true,true,true}),"An all-occupied group cannot produce a transfer");
        check(!cycleEntry(empty,{},false,true,{}),"Empty protected groups cannot produce a commit");
        for(bool skip:{false,true})for(bool remember:{false,true})for(std::size_t selected=0;selected<3;++selected) {
            weapons.selected=selected;
            check(cycleEntry(weapons,{true,false,true},skip,remember,{false,false,false})==cycleEntry(weapons,{true,false,true},skip,remember),"Unblocked groups preserve recall and advance behavior");
        }
        weapons.selected=0;
        Group favorites;setFavoriteSource(favorites,FavoriteSource::weapons,Hand::left);
        refreshFavorites(favorites,{dagger,spare,held});
        check(favorites.entries.size()==3&&favorites.entries[0].same(held)&&favorites.entries[1].same(spare),"Favorites preserve separate copies and their requested hand");
        check(cycleEntry(favorites,{true,true,true},false,true,blocked)==1,"Favorites-linked groups skip the occupied copy after generating their entries");
        Selection protectedSelection;auto previous=protectedSelection.requestCycle(weapons,0,1);
        auto latest=protectedSelection.requestCycle(weapons,0,2);
        check(!protectedSelection.accepts(*previous,weapons)&&protectedSelection.accepts(*latest,weapons),"Rapid protected taps retain only the latest equipment request");
        protectedSelection.cancel();check(!protectedSelection.accepts(*latest,weapons),"A blocked tap can cancel queued equipment");
        protectedOptions.cycleKeepOtherHand=true;
        for(bool pad:{false,true}) {
            latest->controller=pad;
            check(keepOtherHandWhenCycling(protectedOptions,*latest),"Keyboard and controller cycle commits receive protection");
            check(!keepOtherHandWhenCycling(protectedOptions,*latest,true),"Explicit unequip remains available");
            latest->hand=Hand::left;
            check(!keepOtherHandWhenCycling(protectedOptions,*latest),"An explicit browsing hand choice can transfer the copy");
            latest->hand.reset();latest->cycling=false;latest->browsed=true;
            check(!keepOtherHandWhenCycling(protectedOptions,*latest),"Browse selection keeps deliberate hand-transfer behavior");
            latest->cycling=true;latest->browsed=false;
        }
        protectedOptions.cycleKeepOtherHand=false;check(!keepOtherHandWhenCycling(protectedOptions,*latest),"Disabling protection permits normal cycle transfers");
        check(!movesOtherHandWeapon(held,false,false)&&movesOtherHandWeapon(held,false,true),"A fresh equipment check detects a copy occupied after selection");
        check(!movesOtherHandWeapon(held,true,true),"A weapon already in its requested hand stays eligible");
        for(auto kind:{Kind::sword,Kind::dagger,Kind::axe,Kind::mace,Kind::staff}) {auto item=held;item.kind=kind;check(movesOtherHandWeapon(item,false,true),"One-handed physical weapon families receive protection");}
        for(auto kind:{Kind::spell,Kind::shield,Kind::torch,Kind::ammo,Kind::combo,Kind::emptyHand,Kind::unarmed}) {auto item=held;item.kind=kind;check(!movesOtherHandWeapon(item,false,true),"Spells and deliberate multi-hand actions retain their behavior");}
        auto twoHanded=held;twoHanded.twoHanded=true;check(!movesOtherHandWeapon(twoHanded,false,true),"Two-handed equipment retains both-hand behavior");
        auto spellCopy=held;spellCopy.spell=true;check(!movesOtherHandWeapon(spellCopy,false,true),"Spells remain usable in both hands");
    }
    group.selected=0;
    for(const bool pad:{false,true}) {
        Selection selection;
        selection.press(10,group);selection.tick(10.1);
        auto c=selection.release(group);c->controller=pad;
        check(!c->browsed&&selectionAction(potion,*c,o)==SelectionAction::use,"Short tap uses current consumable");
        check(selectionAction(sword,*c,o)==SelectionAction::equip,"Short tap equips current gear");
        selection.press(20,group);selection.tick(20.3);c=selection.release(group);c->controller=pad;
        check(c->browsed&&selectionAction(potion,*c,o)==SelectionAction::select,"Hold release readies consumable");
        check(selectionAction(spell,*c,o)==SelectionAction::equip,"Hold release equips spell normally");
        check(!selection.release(group),"Release cannot repeat");
        c=selection.requestCycle(group,0,nextEntry(group,{},false));c->controller=pad;
        check(c->cycling&&!selection.held()&&!selection.canBrowse(),"Cycle commits without a held selector");
        selection.tick(999);selection.cycle(1,group);
        check(!selection.browsing()&&selection.index()==c->index&&!selection.release(group),"Holding, scrolling and release cannot add a cycle action");
        check(selectionAction(spell,*c,o)==SelectionAction::equip,"Cycle auto-equips");
        check(selectionAction(potion,*c,o)==SelectionAction::select,"Defensive cycle handling never consumes");
        Entry shout;shout.kind=Kind::shout;check(!activatesVoice(shout,*c,o),"Cycle readies a shout without firing it");
        const auto previous=*c;c=selection.requestCycle(group,0,2);
        check(!selection.accepts(previous,group)&&selection.accepts(*c,group),"Rapid cycles invalidate earlier queued equipment");
        selection.cancel();check(!selection.accepts(*c,group),"Menu/focus cancellation invalidates pending cycle");
    }
    for(bool controller:{false,true})for(double threshold:{.1,.18,.8}) {
        Selection gesture;gesture.holdSeconds=threshold;GroupRecall memory;
        Group items=group;items.selected=0;
        const auto finish=[&](double now)->std::optional<Selection::Commit> {
            gesture.tick(now);auto result=gesture.release(items);
            if(result){result->controller=controller;const bool first=memory.activate(0);if(result->cycling)result->index=cycleEntry(items,{true,true,true},true,first);items.selected=result->index;}
            return result;
        };
        gesture.press(1,items,0,true);gesture.tick(1+threshold*.5);
        check(gesture.held()&&!gesture.browsing()&&items.selected==0,"Combined mode waits for release and keeps the current selection while pressed");
        auto first=finish(1+threshold*.5);check(first&&first->cycling&&!first->browsed&&first->index==0,"First combined tap recalls the remembered item");
        gesture.press(2,items,0,true);auto second=finish(2+threshold*.5);
        check(second&&second->cycling&&second->index==1&&!gesture.accepts(*first,items)&&!finish(3),"Another tap advances once and invalidates the previous queued action");
        gesture.press(4,items,0,true);gesture.tick(4+threshold+.001);
        check(gesture.browsing()&&gesture.index()==1&&items.selected==1,"A hold browses the current item before any equip occurs");
        gesture.cycle(-1,items);auto held=finish(5);
        check(held&&held->browsed&&!held->cycling&&held->index==0,"Hold browsing can move backwards and commits once on release");
        check(keepOtherHandWhenCycling(o,*second)&&!keepOtherHandWhenCycling(o,*held),"Combined taps protect the opposite hand while explicit browsing chooses the highlighted copy");
        gesture.press(6,items,0,true);gesture.cycle(1,items);auto early=finish(6+threshold*.5);
        check(early&&early->browsed&&!early->cycling&&early->index==1,"Navigation before the hold threshold explicitly begins browsing");
        gesture.press(7,items,0,true);gesture.cancel();check(!finish(8),"Focus or menu cancellation prevents a combined tap or hold from committing");
        gesture.press(9,items,0,true);++items.revision;check(!finish(9+threshold*.5),"Membership changes invalidate a deferred tap");
        Options saved;saved.groupModes[0]=GroupMode::cycleBrowse;
        check(decodeOptions(encodeOptions(saved)).groupModes[0]==GroupMode::cycleBrowse,"Combined mode persists in settings");
        const auto preset=exportPreset(Arsenal{},saved,false,[](auto id){return Json{{"file","Test.esp"},{"local",id}};});
        check(importPreset(preset,Arsenal{},Options{},false,[](const auto&,auto id){return id;}).options.groupModes[0]==GroupMode::cycleBrowse,"Complete presets retain combined mode");
    }
    for(bool vr:{false,true})for(bool hold:{false,true})for(bool loseModifier:{false,true}) {
        const auto main=vr?PadButton::rt:PadButton::up;
        Options controls;controls.groupModes[0]=GroupMode::cycleBrowse;controls.controllerGroups[0]=controls.vrGroups[0]={main,PadButton::lb};
        ControllerInput input;Selection choosing;input.button(PadButton::lb,true,true,controls,true,false,vr);
        const auto pressed=input.button(main,true,true,controls,true,false,vr);
        check(pressed.action==PadAction::press,"Controller and VR chords begin a deferred group gesture");
        choosing.press(1,group,0,true);choosing.tick(hold?2:1.01);
        check(input.button(main,true,false,controls,true,true,vr).action==PadAction::none,"Held group button repeats do not commit a cycle");
        if(loseModifier){const auto lost=input.button(PadButton::lb,false,false,controls,true,true,vr);check(lost.action==PadAction::cancel,"Releasing the chord modifier cancels the deferred gesture");choosing.cancel();}
        const auto released=input.button(main,false,false,controls,true,choosing.held(),vr);
        const auto committed=released.action==PadAction::release?choosing.release(group):std::nullopt;
        check(loseModifier?!committed:committed&&committed->cycling==!hold&&committed->browsed==hold,"Controller and VR main-button release commits exactly the completed tap or hold");
        check(!input.owned(main),"Deferred gestures balance controller button ownership");
    }
    Selection selection;selection.press(1,group);selection.cycle(1,group);auto commit=selection.release(group);
    check(commit->browsed,"Early wheel movement is browsing");
    ++group.revision;check(!selection.accepts(*commit,group),"Edits invalidate released actions");
    o.groupModes[0]=GroupMode::cycle;
    check(groupMode(o,a,0)==GroupMode::browse,"Mixed group with a potion retains quick-use controls");
    for(auto kind:{Kind::food,Kind::poison,Kind::smartPotion}){group.entries[2].kind=kind;check(groupMode(o,a,0)==GroupMode::browse,"Every consumable type protects quick-use");}
    normalizeGroupModes(o,a);check(o.groupModes[0]==GroupMode::browse,"Adding consumables clears incompatible cycle preference");
    group.entries[2]=potion;
    a.groups[1].add(spell);o.groupModes[1]=GroupMode::cycle;check(groupMode(o,a,1)==GroupMode::cycle,"Equipment group permits cycle");
    const auto round=decodeOptions(encodeOptions(o));check(round.groupModes==o.groupModes,"Modes round trip");
    auto old=Json::parse(encodeOptions(o));old.erase("groupModes");old["groupBehavior"]={{7,3,5}};old["groupHold"]={{1,2}};
    check(decodeOptions(old.dump()).groupModes==Options{}.groupModes,"Removed overrides cannot change new default controls");
    check(!Json::parse(encodeOptions(round)).contains("groupBehavior"),"Removed override fields are not saved");
    Options visual;visual.showEquipment=true;visual.equipmentX=.7f;visual.equipmentY=.3f;visual.inactiveGroups=2;visual.hideEmptyGroups=true;
    visual.equipmentDim=true;visual.equipmentFadeDelay=2;visual.equipmentIdleOpacity=.2f;
    visual.equipmentStyle=2;visual.equipmentLayout=1;visual.equipmentTextPosition=0;
    visual.equipmentScale=1.7f;visual.equipmentTextScale=1.3f;visual.equipmentSpacing=2.5f;
    o.autoSort[1]=true;
    auto imported=o;importAppearance(imported,visual);
    check(imported.showEquipment&&imported.equipmentX==.7f&&imported.equipmentY==.3f,"Appearance carries independent equipment position");
    check(imported.equipmentDim&&imported.equipmentStyle==2&&imported.equipmentLayout==1&&imported.equipmentTextPosition==0&&imported.equipmentFadeDelay==2&&imported.equipmentIdleOpacity==.2f,"Appearance imports equipment style and visibility");
    const auto reloaded=decodeOptions(encodeOptions(imported));
    check(reloaded.equipmentScale==1.7f&&reloaded.equipmentTextScale==1.3f&&imported.scale==o.scale,"Equipment sizing survives settings and appearance import independently of group scale");
    check(reloaded.equipmentSpacing==2.5f&&imported.equipmentSpacing==2.5f,"Equipment spacing survives settings and appearance import");
    auto oldSpacing=Json::parse(encodeOptions(Options{}));oldSpacing.erase("equipmentSpacing");
    check(decodeOptions(oldSpacing.dump()).equipmentSpacing==8,"Existing settings retain their equipment spacing");
    auto oldSizing=Json::parse(encodeOptions(Options{}));oldSizing.erase("equipmentScale");oldSizing.erase("equipmentTextScale");
    check(decodeOptions(oldSizing.dump()).equipmentScale==1&&decodeOptions(oldSizing.dump()).equipmentTextScale==1,"Existing settings retain equipment's original size");
    visual.equipmentScale=visual.equipmentTextScale=visual.equipmentSpacing=1;
    check(reloaded.equipmentDim&&reloaded.equipmentStyle==2&&reloaded.equipmentLayout==1&&reloaded.equipmentTextPosition==0&&reloaded.equipmentFadeDelay==2&&reloaded.equipmentIdleOpacity==.2f,"Equipment options survive settings reload");
    visual.equipmentStyle=0;
    check(imported.groupModes==o.groupModes&&imported.autoSort==o.autoSort,"Appearance preserves play controls and per-group sorting");
    Options bad;bad.groupModes[0]=GroupMode(99);bad.inactiveGroups=99;bad.equipmentX=-1;bad.equipmentY=2;
    sanitizeOptions(bad);check(bad.groupModes[0]==GroupMode::browse&&bad.inactiveGroups==2&&bad.equipmentX==0&&bad.equipmentY==1,"Invalid settings bounded");
    bad.equipmentScale=10;bad.equipmentTextScale=-1;sanitizeOptions(bad);
    check(bad.equipmentScale==3&&bad.equipmentTextScale==.5f,"Equipment scale limits bounded");
    bad.equipmentScale=std::numeric_limits<float>::infinity();bad.equipmentTextScale=std::numeric_limits<float>::quiet_NaN();sanitizeOptions(bad);
    check(bad.equipmentScale==1&&bad.equipmentTextScale==1,"Invalid equipment scales restore finite defaults");
    for(const auto value:{-1.f,5.f,std::numeric_limits<float>::quiet_NaN()}) {
        bad.equipmentSpacing=value;sanitizeOptions(bad);
        check(bad.equipmentSpacing==(std::isfinite(value)?std::clamp(value,0.f,128.f):8.f),"Invalid equipment spacing is bounded with a finite default");
    }
    bad.equipmentStyle=99;bad.equipmentLayout=99;bad.equipmentTextPosition=99;bad.equipmentIdleOpacity=0;bad.equipmentFadeDelay=100;
    sanitizeOptions(bad);check(bad.equipmentStyle==4&&bad.equipmentLayout==5&&bad.equipmentTextPosition==3&&bad.equipmentIdleOpacity==.05f&&bad.equipmentFadeDelay==20,"Equipment settings bounded; dim cannot hide completely");

    group.selected=2;rememberConsumable(a,0,2);a.groups[1].add(potion);a.active=1;
    check(selectedConsumableGroup(a)==0,"Last chosen consumable survives another group");
    const auto loaded=decodeArsenal(encodeArsenal(a),[](auto id,auto& mapped){mapped=id;return true;});
    check(loaded.consumableGroup==0&&selectedConsumableGroup(loaded)==0,"Consumable choice saved");
    group.selected=0;check(selectedConsumableGroup(a)==0&&a.rememberedConsumable&&isConsumable(*a.rememberedConsumable),"Selecting equipment preserves the last explicitly selected consumable");

    {
        Arsenal setup;Options controls;setup.presetIdentity="same";auto& mixed=setup.groups[0];
        Entry drink=potion;drink.form=901;drink.name="Health";mixed.add(drink);mixed.add(sword);
        controls.groupModes[0]=GroupMode::cycleBrowse;normalizeGroupModes(controls,setup);
        check(groupMode(controls,setup,0)==GroupMode::browse,"Existing consumable groups keep tap-to-use behavior");
        controls.separateConsumableUse[0]=true;controls.groupModes[0]=GroupMode::cycleBrowse;normalizeGroupModes(controls,setup);
        check(groupMode(controls,setup,0)==GroupMode::cycleBrowse,"Separate use enables combined cycling and browsing for mixed groups");
        Selection selection;const auto tap=selection.request(mixed,0,0);
        for(const auto kind:{Kind::potion,Kind::food,Kind::poison,Kind::smartPotion}) {auto item=drink;item.kind=kind;check(selectionAction(item,*tap,controls)==SelectionAction::select,"Group activation selects each consumable type in separate-use mode");}
        check(selectionAction(sword,*tap,controls)==SelectionAction::equip,"Mixed-group equipment keeps its normal equip action");
        rememberConsumable(setup,0,0);mixed.selected=1;setup.active=1;
        check(setup.rememberedConsumable->same(drink)&&selectedConsumableGroup(setup)==0,"Last selected consumable survives equipment selection and another active group");
        auto other=drink;other.form=902;other.name="Stamina";setup.groups[1].add(other);rememberConsumable(setup,1,0);
        check(setup.rememberedConsumable->same(other),"An explicit selection in another consumable group becomes the use target");
        setup.groups[1].entries.clear();
        check(setup.rememberedConsumable->same(other),"Depletion or a Favorites refresh retains the selected consumable for restocking");
        const auto identity=[](auto id,auto& mapped){mapped=id;return true;};
        const auto stored=encodeArsenal(setup);auto restored=decodeArsenal(stored,identity);
        check(restored.rememberedConsumable->same(other),"Save/load restores the selected consumable independently of generated membership");
        check(assignConsumableBinding(controls,{35,keyModifier::ctrl}),"A dedicated keyboard action accepts a modifier chord");
        check(consumableShortcutMatches(controls,35,keyModifier::ctrl)&&!consumableShortcutMatches(controls,35,0)&&!consumableShortcutMatches(controls,35,keyModifier::ctrl|keyModifier::alt),"The dedicated key requires its configured modifier combination");
        controls.controllerUse={PadButton::y,PadButton::rt};controls.vrUse={PadButton::y,PadButton::rb};
        const auto round=decodeOptions(encodeOptions(controls));
        check(round.consumableKey==controls.consumableKey&&round.controllerUse==controls.controllerUse&&round.vrUse==controls.vrUse&&round.separateConsumableUse==controls.separateConsumableUse,"All device bindings and per-group use choices persist");
        const auto preset=exportPreset(setup,controls,false,[](auto id){return Json{{"file","Test.esp"},{"local",id}};});
        const auto imported=importPreset(preset,setup,Options{},false,[](const auto&,auto id){return id;});
        check(imported.arsenal.rememberedConsumable->same(other)&&imported.options.consumableKey==controls.consumableKey&&imported.options.separateConsumableUse[0],"Complete presets preserve the consumable target and controls");
        Options appearance;importAppearance(appearance,controls);check(!appearance.consumableKey.key&&!appearance.separateConsumableUse[0],"Appearance imports preserve gameplay bindings and consumable behavior");
        auto legacy=Json::parse(encodeOptions(controls));for(const auto field:{"consumableKey","controllerUse","vrUse","separateConsumableUse"})legacy.erase(field);
        const auto old=decodeOptions(legacy.dump());check(!old.consumableKey.key&&old.controllerUse==PadBinding{}&&old.vrUse==PadBinding{}&&!old.separateConsumableUse[0],"Older settings keep the new action unbound and legacy use behavior");
        check(!assignConsumableBinding(controls,{controls.keys[0]})&&!assignConsumableBinding(controls,{controls.editorKey})&&!assignConsumableBinding(controls,{203}),"The use key cannot replace group, editor or browsing keys");
        check(!assignGroupKey(controls,0,35)&&!assignSelectorBinding(controls,KeyboardAction::next,{35}),"Other bindings reserve the dedicated use key");
        controls.controllerUse=controls.controllerGroups[0];controls.vrUse=controls.vrEditor;sanitizeOptions(controls);
        check(controls.controllerUse==PadBinding{}&&controls.vrUse==PadBinding{},"Conflicting controller and VR use chords are cleared during validation");
        Arsenal growing;Options reserved;reserved.keys[3]=0;
        check(assignConsumableBinding(reserved,{5}),"A cleared group key can be assigned to use");
        reserved.controllerUse={PadButton::left};reserved.vrUse=reserved.vrGroups[3];
        sanitizeOptions(reserved);
        check(reserved.controllerUse==PadBinding{PadButton::left}&&reserved.vrUse==reserved.vrGroups[3],"Hidden group defaults do not reserve controller use bindings");
        check(addGroup(growing,reserved)&&reserved.keys[3]!=reserved.consumableKey.key&&reserved.controllerGroups[3]!=reserved.controllerUse&&reserved.vrGroups[3]!=reserved.vrUse,"Adding a group preserves dedicated use bindings on every device");
        Entry smart;smart.kind=Kind::smartPotion;smart.name="Health rule";const auto id=rememberShortcut(setup,smart);addShortcut(setup,2,id);rememberConsumable(setup,2,0);
        smart.policy=1;updateShortcut(setup,id,smart);check(setup.rememberedConsumable->policy==1,"The selected smart potion follows shared rule edits");
        deleteShortcut(setup,id);check(!setup.rememberedConsumable,"Deleting a selected smart rule clears the use target");
        rememberConsumable(setup,0,0);resetGroup(setup,controls,0);check(!setup.rememberedConsumable&&!controls.separateConsumableUse[0],"Removing the source group resets the selected consumable and behavior");
        auto invalid=Json::parse(stored);invalid["rememberedConsumable"]=encodeEntry(sword);bool rejected=false;
        try{decodeArsenal(invalid.dump(),identity);}catch(...){rejected=true;}check(rejected,"Saved use targets must be consumables");
    }
    for(bool vr:{false,true})for(bool allowed:{false,true}) {
        Options controls;ControllerInput input;const PadBinding binding{PadButton::y,PadButton::rb};
        (vr?controls.vrUse:controls.controllerUse)=binding;
        input.button(binding.modifier,true,true,controls,allowed,false,vr);
        const auto down=input.button(binding.button,true,true,controls,allowed,false,vr);
        check((down.action==PadAction::useConsumable)==allowed,"Controller and VR use chords activate only in allowed gameplay");
        check(input.button(binding.button,true,false,controls,allowed,false,vr).action==PadAction::none,"Held use buttons do not repeat consumption");
        const auto up=input.button(binding.button,false,false,controls,allowed,false,vr);
        check(up.action==PadAction::none&&up.consumed==allowed&&!input.owned(binding.button),"Use action releases are balanced without a second activation");
    }
    {
        Options controls;controls.vrUse={PadButton::rt};ControllerInput input;
        input.button(PadButton::lb,true,true,controls,true,false,true);
        check(input.button(PadButton::rt,true,true,controls,true,false,true).action==PadAction::press,"Explicit VR group chords take priority over a plain use trigger");
    }
    Snapshot s;s.visible=true;s.groups[0]={sword,3,true,false};s.groups[1]={spell,2,true,true};
    visual.inactiveGroups=1;check(groupVisibility(s,visual,0,5)==visual.inactiveGroupOpacity,"Inactive card dimmed");
    check(groupVisibility(s,visual,1,5)==1,"Equipped card stays visible");
    Entry combo;combo.kind=Kind::combo;combo.parts={sword,spell};combo.parts[1].hand=Hand::left;
    s.groups[0].entry=combo;s.equipment.hands[0]=Row{sword,true,true};
    check(groupVisibility(s,visual,0,5)==visual.inactiveGroupOpacity,"A matching component does not light an incomplete combination");
    check(entryHands(combo,false)==0,"Incomplete combination has no equipped hand marker");
    s.groups[0].equipped=true;check(groupVisibility(s,visual,0,5)==1&&entryHands(combo,true)==3,"Complete combination has both hand markers");s.groups[0].equipped=false;
    visual.inactiveGroups=2;check(groupVisibility(s,visual,0,5)==0,"Inactive card hidden");
    s.revealUntil=6;check(groupVisibility(s,visual,0,5)==1,"Input temporarily reveals inactive group");
    check(groupVisibility(s,visual,0,7)==0,"Temporary reveal expires");
    s.holding=true;check(groupVisibility(s,visual,0,7)==1,"Held group visible");s.holding=false;
    check(groupVisibility(s,visual,2,7)==0,"Empty group hidden");
    s.groups[0].consumable=true;check(groupVisibility(s,visual,0,7)==1&&entryHands(potion,true)==0,"Ready potion stays visible without an equipped hand marker");
    check(showEquipmentDisplay(s,visual,7)&&showGroupDisplay(s,visual,7),"Enabling equipment never hides idle groups");
    s.browsing=true;check(showEquipmentDisplay(s,visual,7)&&showGroupDisplay(s,visual,7),"Equipment remains visible while browsing");s.browsing=false;
    s.revealUntil=9;check(showEquipmentDisplay(s,visual,7)&&showGroupDisplay(s,visual,7),"Group interaction does not hide equipment");s.revealUntil=0;
    visual.showIdle=false;check(showEquipmentDisplay(s,visual,7)&&!showGroupDisplay(s,visual,7),"Equipment is independent of selection-only groups");visual.showIdle=true;
    s.editorPreview=true;s.placementPreview=true;check(showEquipmentDisplay(s,visual,7)&&showGroupDisplay(s,visual,7),"Placement exposes both independent displays");
    check(groupVisibility(s,visual,2,7)==1,"Position mode reveals empty slots for placement");s.placementPreview=s.editorPreview=false;
    for(const float height:{720.f,1080.f,1440.f,2160.f})for(unsigned shape=0;shape<4;++shape)for(float edge:{0.f,1.f}) {
        visual.shape=shape;visual.layout=2;visual.groupCount=4;visual.equipmentX=visual.equipmentY=edge;
        const auto g=hudGeometry(visual,height*16/9,height);auto without=visual;without.showEquipment=false;
        const auto plain=hudGeometry(without,height*16/9,height);
        check(g.origin.x==plain.origin.x&&g.origin.y==plain.origin.y&&g.width==plain.width&&g.height==plain.height,"Equipment list never moves/resizes groups");
        const auto& e=g.equipment;
        check(e.origin.y>=8&&e.origin.y+e.height*e.scale<=height-7.9f&&e.origin.x>=8&&e.origin.x+e.width*e.scale<=height*16/9-7.9f,"Independent list fits at screen extremes");
        check(e.height*e.scale<height*.2f,"Idle equipment stays compact at each resolution");
    }
    for(unsigned style=0;style<5;++style)for(unsigned text=0;text<4;++text)for(unsigned layout=0;layout<6;++layout)
        for(float t:{.75f,1.5f})for(float edge:{0.f,1.f})for(unsigned count:{3u,4u})for(float spacing:{0.f,1.f,4.f}) {
            Options look;look.equipmentStyle=style;look.equipmentTextPosition=text;look.equipmentLayout=layout;look.textScale=t;look.equipmentX=look.equipmentY=edge;
            look.equipmentScale=2;look.equipmentTextScale=1.5f;look.equipmentSpacing=spacing;
            const auto g=equipmentGeometry(look,1280,720,count);
            check(g.origin.x>=8&&g.origin.y>=8&&g.origin.x+g.width*g.scale<=1272.1f&&g.origin.y+g.height*g.scale<=712.1f,"Every equipment layout fits screen extremes");
            if(count==4&&layout<2) {
                const bool horizontal=layout==0;
                check(horizontal?g.cells[1].origin.x>g.cells[0].origin.x&&g.cells[1].origin.y==g.cells[0].origin.y:g.cells[1].origin.y>g.cells[0].origin.y&&g.cells[1].origin.x==g.cells[0].origin.x,"Equipment cells follow requested arrangement");
                if(style>0&&style<4)check(text==0?g.cells[0].text.y<g.cells[0].icon.y:text==1?g.cells[0].text.y>g.cells[0].icon.y:text==2?g.cells[0].text.x<g.cells[0].icon.x:g.cells[0].text.x>g.cells[0].icon.x,"Text uses the requested side of its icon");
                if(style==4)check(g.iconSize==0,"Text-only layout has no icon");
            }
        }
    for(unsigned style=0;style<5;++style)for(unsigned layout=0;layout<6;++layout) {
        Options spaced;spaced.equipmentStyle=style;spaced.equipmentLayout=layout;
        const auto normal=equipmentGeometry(spaced,3840,2160);
        spaced.equipmentSpacing=0;const auto tight=equipmentGeometry(spaced,3840,2160);
        spaced.equipmentSpacing=128;const auto loose=equipmentGeometry(spaced,3840,2160);
        const auto pitch=[&](const auto& g){return std::hypot(g.cells[1].origin.x-g.cells[0].origin.x,g.cells[1].origin.y-g.cells[0].origin.y);};
        check(pitch(tight)<pitch(normal)&&pitch(normal)<pitch(loose),"Spacing adjusts separation in every equipment style and arrangement");
        check(tight.iconSize==normal.iconSize&&normal.iconSize==loose.iconSize&&tight.cells[0].textWidth==loose.cells[0].textWidth&&tight.scale==loose.scale,"Spacing preserves icon/text size while there is screen room");
        if(layout<2)check(pitch(tight)>=(layout==0?normal.cells[0].width:normal.cells[0].height),"Zero spacing joins the content bounds");
        const auto one=equipmentGeometry(spaced,3840,2160,1);spaced.equipmentSpacing=0;
        const auto oneTight=equipmentGeometry(spaced,3840,2160,1);
        check(one.width==oneTight.width&&one.height==oneTight.height,"Spacing adds no outer padding to a single equipment entry");
    }
    for(unsigned style=0;style<5;++style)for(unsigned layout=0;layout<6;++layout)for(unsigned text=0;text<4;++text) {
        Options a;a.equipmentStyle=style;a.equipmentLayout=layout;a.equipmentTextPosition=text;a.equipmentScale=1;a.textScale=1;a.equipmentSpacing=0;
        auto b=a;b.equipmentSpacing=128;
        const auto tight=equipmentGeometry(a,1280,720),loose=equipmentGeometry(b,1280,720);
        const float requested=a.equipmentScale*.75f;
        if(loose.width*requested<=1264&&loose.height*requested<=704)check(tight.scale==loose.scale,"Equipment size stays fixed while the layout fits the screen");
        const auto previewTight=previewGeometry(a,1280,720,600,440,false,false,true,4,{},5,.5f).equipment;
        const auto previewLoose=previewGeometry(b,1280,720,600,440,false,false,true,4,{},5,.5f).equipment;
        check(std::abs(previewTight.scale/tight.scale-previewLoose.scale/loose.scale)<.001f,"Equipment preview zoom remains fixed when spacing changes");
    }
    Options sized;sized.equipmentStyle=1;
    const auto previewEquipment=[&]{return previewGeometry(sized,1920,1080,900,700,false,false,true,4,{},5,1).equipment;};
    const auto small=previewEquipment();sized.equipmentScale=2;const auto big=previewEquipment();
    check(big.width*big.scale>small.width*small.scale&&big.width*big.scale<900&&big.height*big.scale<700,"Equipment live preview grows with scale while fitting its panel");
    auto two=sword;two.twoHanded=true;s.equipment.hands[0]=Row{two,true,true};
    check(equipmentRows(s.equipment)==3&&entryHands(two,true)==3,"Two-handed gear uses one compact equipment row");
    IdleFade fade;s.revealUntil=0;s.holding=true;visual.showIdle=false;
    check(fade.update(s,visual,10)==1,"Hold reveals selection-only HUD");
    s.holding=false;check(fade.update(s,visual,11)==0,"Selection-only HUD hides on release");
    Snapshot idle;idle.visible=true;idle.equipment.hands[0]=Row{sword,true,true};
    Options independent;setWidgetVisibility(independent,WidgetVisibility::selecting);setEquipmentVisibility(independent,EquipmentVisibility::always);
    EquipmentFade equipmentFade;IdleFade widgetFade;
    check(widgetFade.update(idle,independent,1)==0&&equipmentFade.update(idle,independent,1)==1,"Always-visible equipment survives hidden groups");
    check(equipmentFade.update(idle,independent,100)==1,"Always-visible equipment does not fade with time");
    setEquipmentVisibility(independent,EquipmentVisibility::dim);independent.equipmentFadeDelay=1;independent.equipmentIdleOpacity=.25f;
    ++idle.interaction;check(equipmentFade.update(idle,independent,101)==1&&equipmentFade.update(idle,independent,103)==.25f,"Independent equipment timer dims to selected opacity");
    idle.equipment.hands[0]->entry.form=9;check(equipmentFade.update(idle,independent,104)==1,"External hand change reveals equipment");
    check(equipmentFade.update(idle,independent,106)==.25f,"External hand change fades after idle delay");
    Entry voice;voice.kind=Kind::shout;voice.form=10;idle.equipment.voice=Row{voice,true,true,{},0,5};
    check(equipmentFade.update(idle,independent,107)==1,"Changed selected power reveals equipment");
    idle.equipment.voice->cooldown=4;check(equipmentFade.update(idle,independent,109)==.25f,"Cooldown ticking does not prevent dimming");
    idle.equipment.voice->cooldown=0;check(equipmentFade.update(idle,independent,110)==1,"Finished cooldown reveals equipment");
    idle.inCombat=true;check(equipmentFade.update(idle,independent,200)==1,"Equipment stays bright in combat");
    idle.inCombat=false;check(equipmentFade.update(idle,independent,202)==.25f,"Leaving combat resumes dimming");
    idle.visible=false;check(equipmentFade.update(idle,independent,203)==0,"Full menus hide equipment in every mode");idle.visible=true;
    setEquipmentVisibility(independent,EquipmentVisibility::off);setWidgetVisibility(independent,WidgetVisibility::always);
    check(equipmentFade.update(idle,independent,204)==0&&widgetFade.update(idle,independent,204)==1,"Disabling equipment never hides groups");
    for(const auto widgetMode:{WidgetVisibility::always,WidgetVisibility::fade,WidgetVisibility::hide,WidgetVisibility::selecting})
        for(const auto equipmentMode:{EquipmentVisibility::off,EquipmentVisibility::always,EquipmentVisibility::dim}) {
            Options look;setWidgetVisibility(look,widgetMode);setEquipmentVisibility(look,equipmentMode);
            look.fadeDelay=look.equipmentFadeDelay=1;
            IdleFade regularWidget,lootWidget;EquipmentFade regularEquipment,lootEquipment;
            Snapshot regular;regular.visible=true;regular.equipment.hands[0]=Row{sword,true,true};
            for(const double now:{1.,1.5,2.2,3.,3.5,5.}) {
                const bool lootOpen=now>=1.5&&now<3.5;
                const auto open=[&](auto name){return lootOpen&&name=="LootMenu";};
                auto looting=regular;looting.visible=!gameplayMenuOpen(open,MenuPurpose::hud);
                check(looting.visible,"Opening or closing QuickLoot never forces passive HUD visibility off");
                check(gameplayMenuOpen(open)==lootOpen,"Passive HUD visibility cannot enable gameplay input while looting");
                check(lootWidget.update(looting,look,now)==regularWidget.update(regular,look,now),"QuickLoot preserves every widget visibility mode and its fade timer");
                check(lootEquipment.update(looting,look,now)==regularEquipment.update(regular,look,now),"QuickLoot preserves independent equipment visibility and its fade timer");
            }
            auto looting=regular;looting.visible=!gameplayMenuOpen([](auto name){return name=="LootMenu";},MenuPurpose::hud);
            looting.equipment.hands[0]->entry.form=77;
            check(lootEquipment.update(looting,look,6)==(equipmentMode==EquipmentVisibility::off?0:1),"External equipment changes can update the passive display during looting");
            looting.visible=!gameplayMenuOpen([](auto name){return name=="LootMenu"||name=="TweenMenu";},MenuPurpose::hud);
            check(lootWidget.update(looting,look,7)==0&&lootEquipment.update(looting,look,7)==0,"Tween still hides both displays when QuickLoot is also open");
        }
    for(const auto codes:{PadCodeSet::xinput,PadCodeSet::orbis}) {
        ControllerInput input;Options controls;controls.groupModes[0]=GroupMode::cycle;
        const auto up=padButton(padCodes(codes)[unsigned(PadButton::up)],codes);
        check(input.button(up,true,true,controls,true,false).action==PadAction::press,"Fresh controller cycle press recognized");
        input.cancel();
        check(input.button(up,true,false,controls,true,false).action==PadAction::none,"Held cycle button does not repeat");
        check(!input.button(PadButton::rb,true,true,controls,true,false,false,false).consumed,"Cycle does not capture browse bumpers");
        const auto release=input.button(up,false,false,controls,true,false);
        check(release.consumed&&release.action==PadAction::none,"Cycle release swallowed without a second action");
    }

    MenuAPIState api;check(!api.requestOpen()&&!api.isOpen(),"API safe before engine initialization");
    api.publish(true,false);check(api.requestOpen()&&api.pending()&&!api.isOpen(),"Accepted does not mean visible");
    api.requestClose();check(api.take()==MenuAPIState::Command::close&&!api.pending(),"Close cancels pending open");
    check(api.requestOpen()&&api.take()==MenuAPIState::Command::open,"Request reaches game thread");
    api.publish(true,true);check(api.isOpen()&&api.requestOpen(),"Opening open editor is idempotent");
    api.requestClose();check(api.isOpen(),"Close reports actual state until game thread closes");
    api.take();api.publish(true,false);check(!api.isOpen(),"Closed state published");
    api.setHotkeyEnabled(false);check(!api.hotkeyEnabled()&&api.requestOpen(),"API still opens with disabled shortcut");
    api.publish(false,false);check(!api.pending()&&!api.requestOpen(),"Loading cancels pending requests");
    EditorLaunch launch;launch.request(10);check(launch.poll(11,true,false)==EditorLaunch::Result::wait,"Wait for caller to release input");
    check(launch.poll(12,true,true)==EditorLaunch::Result::open,"Open after handoff");
    launch.request(20);check(launch.poll(23,true,false)==EditorLaunch::Result::cancel,"Blocked handoff expires");
    {
        EditorTextInput textInput;using Change=EditorTextInput::Change;
        int count=0,acquires=0,releases=0;
        const auto update=[&](bool editorOpen,bool menuOpen) {
            const auto change=textInput.update(editorOpen,menuOpen,count);
            if(change==Change::acquire||change==Change::restore){++count;++acquires;}
            else if(change==Change::release){--count;++releases;}
            return change;
        };
        for(unsigned frame=0;frame<10;++frame)
            check(update(true,false)==Change::none&&count==0,"Queued editor opening cannot claim text input before its menu is on the stack");
        check(update(false,false)==Change::none&&acquires==0&&releases==0,"Cancelled opening never acquires or releases text input");
        check(update(false,true)==Change::none&&count==0,"Late menu show after cancellation cannot claim text input");
        check(update(true,true)==Change::acquire&&count==1,"Visible editor claims text input once");
        for(unsigned frame=0;frame<10;++frame)
            check(update(true,true)==Change::none&&count==1&&acquires==1,"Repeated menu notifications and ticks do not grow the text-entry count");
        count=0;
        check(update(true,true)==Change::restore&&count==1,"External counter reset recovers typing without reopening the editor");
        check(update(true,true)==Change::none&&count==1,"Recovery claims exactly one reference");
        count=0;
        check(update(true,true)==Change::restore&&count==1,"A second reset in the same session also recovers");
        ++count;
        check(update(true,true)==Change::none&&count==2,"Another text-input owner can coexist without triggering recovery");
        check(update(false,true)==Change::release&&count==1,"Closing while hide is queued releases only the editor's reference");
        for(const bool menuOpen:{true,false})
            check(update(false,menuOpen)==Change::none&&count==1,"Duplicate close and delayed hide preserve another owner's reference");
        check(update(true,true)==Change::acquire&&count==2,"Deferred acquisition adds its own reference when another owner is already active");
        --count;
        check(update(true,true)==Change::none&&count==1,"Another owner's balanced release leaves the editor claim intact");
        check(update(true,false)==Change::release&&count==0,"Unexpected menu removal releases the editor's reference");
        check(update(false,false)==Change::none&&count==0,"Closing after menu removal cannot release twice");
        for(unsigned cycle=0;cycle<6;++cycle) {
            check(update(true,false)==Change::none&&count==0,"Reopening waits for the menu again");
            check(update(true,true)==Change::acquire&&count==1,"Each new session acquires one reference");
            count=0;
            check(update(false,true)==Change::none&&count==0,"Reset immediately before close cannot underflow the shared counter");
        }
        count=-1;
        check(update(true,true)==Change::none&&count==-1,"An unavailable text-entry counter is left untouched");
        count=0;
        check(update(true,true)==Change::acquire&&count==1,"Text input can be acquired when the counter becomes available");
        count=-1;
        check(update(false,true)==Change::none&&count==-1,"Unavailable counter on shutdown does not receive a release");
        count=std::numeric_limits<std::int8_t>::max();
        check(update(true,true)==Change::none&&count==127,"A saturated engine counter cannot overflow during acquisition");
        --count;
        check(update(true,true)==Change::acquire&&count==127,"Acquisition retries after counter capacity becomes available");
        check(update(false,true)==Change::release&&count==126,"Release at counter capacity still preserves other references");
    }
    ControllerInput controller;Options controls;
    controller.button(PadButton::up,true,true,controls,true,false);
    auto result=controller.button(PadButton::start,true,true,controls,true,false,false,true,false);
    check(result.action!=PadAction::editor,"Runtime gate disables controller editor chord");
    controller.reset();result=controller.button(PadButton::up,true,true,controls,true,false,false,true,false);
    check(result.action==PadAction::press,"Runtime editor gate preserves group shortcuts");
    Entry bottle;bottle.kind=Kind::potion;bottle.name="Healing";Row stock{bottle,true,false,{},4};
    check(equipmentTextName(stock,true)=="Healing"&&equipmentTextName(stock,false)=="Healing ×4","Stock stays on the icon, or beside the name for text-only equipment");
    Entry power;power.kind=Kind::power;power.name="Battle Cry";Row used{power,true,true,{},0,0,true};
    check(equipmentDimmed(used)&&equipmentTextName(used,true)=="Battle Cry"&&equipmentTextName(used,false)=="Battle Cry","Used powers dim their icon and text without a detached status label");
    used.powerUsed=false;used.cooldown=17.2f;
    check(equipmentDimmed(used)&&equipmentTextName(used,false)=="Battle Cry · 18s"&&equipmentTextName(used,true)=="Battle Cry","Recharge timer sits on the icon or beside text-only names");
    used.cooldown=0;check(!equipmentDimmed(used),"Finished recharge restores normal brightness");
    {
        Options hands;check(hands.mouseActions==defaultMouseActions&&!hands.selectorBindings[unsigned(KeyboardAction::rightHand)].key,"Existing configurations keep right-click cancel and no hand actions");
        assignMouseAction(hands,MouseAction::leftHand,1);
        check(hands.mouseActions[0]==-1&&hands.mouseActions[2]==1,"Assigning right-click to a hand removes its cancellation conflict");
        assignMouseAction(hands,MouseAction::rightHand,0);
        check(!assignMouseAction(hands,MouseAction::rightHand,8),"Mouse wheel cannot masquerade as a hand button");
        check(assignSelectorBinding(hands,KeyboardAction::rightHand,{16})&&selectorKeyAction(hands,16,0u).action==SelectorKeyAction::rightHand,"Keyboard hand actions use configurable captured bindings");
        const auto saved=decodeOptions(encodeOptions(hands));
        check(saved.mouseActions==hands.mouseActions&&saved.selectorBindings==hands.selectorBindings,"Hand bindings persist");
        auto old=Json::parse(encodeOptions(hands));old.erase("mouseActions");old["selectorBindings"].erase(old["selectorBindings"].begin()+5,old["selectorBindings"].end());old["controllerActions"].erase(old["controllerActions"].begin()+4,old["controllerActions"].end());
        const auto migrated=decodeOptions(old.dump());check(migrated.mouseActions==defaultMouseActions&&!migrated.selectorBindings[5].key&&migrated.controllerActions[4].button==PadButton::none,"Earlier action arrays migrate without enabling hand actions");
        Options local;importAppearance(local,hands);check(local.mouseActions==defaultMouseActions,"Appearance imports preserve mouse controls");
    }
    {
        Options look;Entry bow;bow.kind=Kind::bow;bow.name="Hunting Bow";Row row{bow,true,true};row.weapon={true,42};
        check(equipmentTextName(row,false,look)=="Hunting Bow ×42 · Poisoned"&&equipmentTextName(row,true,look)=="Hunting Bow","Text-only equipment retains ammo and poison status without duplicating icon badges");
        check(badgeQuantity(bow,1,row.weapon,look)==42,"Bows show ammunition rather than weapon copies");
        look.showAmmo=false;check(badgeQuantity(bow,1,row.weapon,look)==-1&&equipmentTextName(row,false,look)=="Hunting Bow · Poisoned","Ammo display can be disabled without hiding poison");
        look.showAmmo=true;look.showPoison=false;check(equipmentTextName(row,false,look)=="Hunting Bow ×42","Poison display can be disabled independently");
        row.weapon={};check(badgeQuantity(bow,1,row.weapon,look)==-1&&equipmentTextName(row,false,look)=="Hunting Bow","Unresolved weapons do not invent ammo or poison status");
        look.showPoison=true;row.weapon={true,0};check(equipmentTextName(row,false,look)=="Hunting Bow ×0 · Poisoned","Zero compatible ammunition remains explicit");
        check(badgeQuantity(bottle,4,row.weapon,look)==4,"Existing consumable quantities keep priority");
        for(bool crossbow:{false,true})for(bool bolt:{false,true})for(int quantity:{-1,0,27,1500})
            check(compatibleAmmoCount(crossbow,bolt,quantity)==(crossbow==bolt?std::max(0,quantity):0),"Bows count arrows and crossbows count bolts without mixing incompatible stock");
        check(compatibleAmmoCount(false,{},100)==0,"Unequipped ammunition reports zero");
        EquippedDisplay equipped;equipped.hands[0]=row;
        check(!look.showEquipmentAmmo&&equipmentRows(equipped,look)==4,"Ammunition starts hidden in the equipment widget");
        look.showEquipmentAmmo=true;check(equipmentRows(equipped,look)==5,"Ammunition adds a fifth equipment row");
        equipped.hands[0]->entry.twoHanded=true;check(equipmentRows(equipped,look)==4,"A bow and ammunition use four equipment rows");
        Entry ammo;ammo.kind=Kind::ammo;ammo.name="Steel Arrows";equipped.ammo=Row{ammo,true,true,{},42};
        look.showAmmo=false;
        check(badgeQuantity(ammo,42,{},look)==42&&equipmentTextName(equipped.ammo,false,look)=="Steel Arrows ×42","Equipment ammunition counts remain available with bow badges disabled");
        equipped.ammo->quantity=0;check(equipmentTextName(equipped.ammo,false,look)=="Steel Arrows ×0","A depleted ammunition row reflects the current quantity");
        equipped.ammo.reset();check(equipmentTextName(equipped.ammo,false,look)=="—","The ammunition slot has an empty state");
        const auto ammoSaved=decodeOptions(encodeOptions(look));check(ammoSaved.showEquipmentAmmo,"Equipment ammunition persists in settings");
        Options importedAmmo;importAppearance(importedAmmo,ammoSaved);check(importedAmmo.showEquipmentAmmo,"Appearance presets include the ammunition row");
        auto oldAmmo=Json::parse(encodeOptions(look));oldAmmo.erase("showEquipmentAmmo");check(!decodeOptions(oldAmmo.dump()).showEquipmentAmmo,"Existing settings retain the original equipment layout");
        look.showPoison=false;look.showAmmo=false;const auto saved=decodeOptions(encodeOptions(look));
        check(!saved.showPoison&&!saved.showAmmo,"Status settings persist");
        auto legacy=Json::parse(encodeOptions(look));legacy.erase("showPoison");legacy.erase("showAmmo");const auto migrated=decodeOptions(legacy.dump());
        check(migrated.showPoison&&migrated.showAmmo,"Existing settings gain both status indicators");
        Options appearance;importAppearance(appearance,saved);check(!appearance.showPoison&&!appearance.showAmmo,"Appearance imports include status display preferences");
    }
    {
        const auto identity=[](std::uint32_t id,std::uint32_t& mapped){mapped=id;return true;};
        Arsenal favorites;Options settings;auto& linked=favorites.groups[0];
        Entry manual;manual.form=17;manual.owner=0x14;manual.unique=1;manual.name="Manual sword";linked.add(manual);
        Entry other=manual;other.form=18;other.unique=2;other.name="Manual axe";linked.add(other);
        const auto manualData=encode(linked);
        std::vector<Entry> choices;
        for(unsigned i=0;i<40;++i){auto e=manual;e.form=100+i;e.unique=std::uint16_t(10+i);e.name="Favorite "+std::to_string(100+i);choices.push_back(e);}
        Entry spell;spell.form=200;spell.kind=Kind::spell;spell.spell=true;spell.name="Healing";choices.push_back(spell);
        Entry potion;potion.form=201;potion.kind=Kind::potion;potion.name="Potion";choices.push_back(potion);
        choices.push_back(choices[0]);
        check(setFavoriteSource(linked,FavoriteSource::weapons,Hand::left)&&linked.manualEntries.size()==2,"Linking Favorites preserves the manual group separately");
        check(refreshFavorites(linked,choices)&&linked.entries.size()==40&&linked.entries.front().hand==Hand::left,"Category groups include all matches beyond the manual 16-item limit and remove duplicate identities");
        check(!linked.add(manual)&&!linked.reorder(0,1)&&!linked.sortByName()&&!favorites.move(0,0,1),"Generated membership cannot be manually added, reordered or moved");
        linked.selected=28;const auto remembered=linked.entries[28];const auto revision=linked.revision;
        std::reverse(choices.begin(),choices.end());check(!refreshFavorites(linked,choices)&&linked.revision==revision&&linked.entries[linked.selected].same(remembered),"Inventory enumeration order cannot reorder Favorites or change selection");
        Selection browse;browse.press(1,linked);const auto queued=browse.release(linked);
        auto added=manual;added.form=300;added.unique=100;added.name="A first favorite";choices.push_back(added);
        check(refreshFavorites(linked,choices)&&linked.entries[linked.selected].same(remembered)&&!browse.accepts(*queued,linked),"Inserting a favorite retains the selected identity and invalidates a queued index");
        linked.selected=1;const auto excluded=linked.entries[1];check(linked.remove()&&linked.excludedFavorites.size()==1,"Removing a generated row creates an exclusion");
        refreshFavorites(linked,choices);check(std::none_of(linked.entries.begin(),linked.entries.end(),[&](const auto& e){return sameFavorite(e,excluded);}),"Excluded favorites stay absent on refresh");
        setFavoriteSource(linked,FavoriteSource::all,Hand::right);refreshFavorites(linked,choices);
        check(linked.entries.size()==42&&std::none_of(linked.entries.begin(),linked.entries.end(),[&](const auto& e){return sameFavorite(e,excluded);}),"Changing category or default hand retains exclusions");
        const auto encoded=encodeArsenal(favorites);const auto json=Json::parse(encoded);
        check(json["groups"][0]["entries"].size()==2&&json["groups"][0]["favorites"]["excluded"].size()==1,"Save data contains manual entries and source preferences, not generated membership");
        auto loaded=decodeArsenal(encoded,identity);auto& restored=loaded.groups[0];
        check(restored.automatic()&&restored.entries.empty()&&restored.manualEntries.size()==2&&restored.excludedFavorites.size()==1,"Loading restores source and manual backup before reading current Favorites");
        refreshFavorites(restored,choices);check(restored.entries.size()==linked.entries.size(),"Current Favorites rebuild the linked group after load");
        setFavoriteSource(restored,FavoriteSource::manual,Hand::right);
        auto oldManual=Json::parse(manualData),restoredManual=Json::parse(encode(restored));restoredManual.erase("favorites");
        check(restoredManual==oldManual,"Switching back to Manual restores the original contents and remembered item");
        auto oldSave=Json::parse(encoded);oldSave["groups"][0].erase("favorites");
        check(!decodeArsenal(oldSave.dump(),identity).groups[0].automatic(),"Older settings remain manual without losing entries");
        auto invalid=json;invalid["groups"][0]["favorites"]["source"]=999u;bool rejected=false;
        try{decodeArsenal(invalid.dump(),identity);}catch(...){rejected=true;}check(rejected,"Invalid source values are rejected");
        const auto portable=exportPreset(favorites,settings,false,[](auto id){return Json{{"file","Favorites.esp"},{"local",id}};});
        favorites.presetIdentity="same";const auto samePreset=exportPreset(favorites,settings,false,[](auto id){return Json{{"file","Favorites.esp"},{"local",id}};});
        const auto imported=importPreset(samePreset,favorites,settings,false,[](const auto&,auto id){return id;});
        check(imported.arsenal.groups[0].automatic()&&imported.arsenal.groups[0].manualEntries[0].same(manual)&&imported.arsenal.groups[0].excludedFavorites.size()==1,"Complete presets round-trip hidden manual references and exclusions");
        const auto foreign=importPreset(portable,favorites,settings,false,[](const auto&,auto id){return id;});
        const auto& foreignGroup=foreign.arsenal.groups[0];
        check(foreignGroup.automatic()&&foreignGroup.manualEntries[0].unresolved&&!foreignGroup.manualEntries[0].owner&&foreignGroup.excludedFavorites[0].unresolved&&foreignGroup.rememberedFavorite->unresolved,"Another character's preset retains category settings without borrowing exact inventory identities");
        EditHistory history;history.begin(favorites,settings);history.prepare(favorites,settings);
        setFavoriteSource(linked,FavoriteSource::manual,Hand::right);history.finish(true,false,1);
        check(history.undo(favorites,settings)&&favorites.groups[0].automatic(),"Undo restores the source and generated selection together");
        auto& g=favorites.groups[0];setFavoriteSource(g,FavoriteSource::spells,Hand::left);refreshFavorites(g,choices);
        check(g.entries.size()==1&&g.entries[0].spell&&g.entries[0].hand==Hand::left,"Spell categories honor their default hand");
        setFavoriteSource(g,FavoriteSource::potions,Hand::left);refreshFavorites(g,choices);
        check(g.entries.size()==1&&g.entries[0].kind==Kind::potion&&g.entries[0].hand==Hand::right,"Potion categories retain consumable hand rules");
        refreshFavorites(g,{});check(g.entries.empty(),"Unfavorited or depleted items leave generated membership");
        refreshFavorites(g,choices);check(g.entries.size()==1,"Restocked favorites return without manual additions");
        const auto changed=followIdentityChange(g,{manual.form,manual.owner,0x24,manual.unique,31});
        check(changed&&g.manualEntries[0].owner==0x24&&g.manualEntries[0].unique==31,"Hidden manual items follow inventory identity changes");
        resetGroup(favorites,settings,0);check(!favorites.groups[0].automatic()&&favorites.groups[0].manualEntries.empty()&&favorites.groups[0].excludedFavorites.empty(),"Removing a group clears its source, exclusions and hidden backup");
        Entry smart;smart.kind=Kind::smartPotion;smart.name="Restore health";
        const auto shortcut=rememberShortcut(favorites,smart);addShortcut(favorites,0,shortcut);favorites.groups[0].add(manual);
        setFavoriteSource(favorites.groups[0],FavoriteSource::potions,Hand::right);refreshFavorites(favorites.groups[0],choices);
        smart.name="Renamed health";
        check(shortcutUses(favorites,shortcut)==1&&updateShortcut(favorites,shortcut,smart)&&favorites.groups[0].manualEntries[0].name==smart.name,"Shared shortcut edits reach the saved manual contents of a Favorites group");
        check(deleteShortcut(favorites,shortcut)&&favorites.groups[0].manualEntries.size()==1&&favorites.groups[0].manualSelected==0&&favorites.groups[0].entries.size()==1,"Deleting a shared shortcut preserves generated membership and the remaining manual selection");
        setFavoriteSource(favorites.groups[0],FavoriteSource::manual,Hand::right);
        check(favorites.groups[0].entries[0].same(manual),"Restoring Manual cannot resurrect a deleted shared shortcut");
        Group capped;setFavoriteSource(capped,FavoriteSource::all,Hand::right);refreshFavorites(capped,choices);
        for(unsigned i=0;i<Group::exclusionLimit;++i){auto e=potion;e.form=1000+i;capped.excludedFavorites.push_back(e);}
        const auto before=capped.entries.size();check(!capped.remove()&&capped.entries.size()==before,"A full exclusion list leaves membership intact instead of losing an unpersisted exclusion");
        auto oversized=Json::parse(encode(capped));oversized["favorites"]["excluded"].push_back(encodeEntry(potion));rejected=false;
        try{decode(oversized.dump(),identity);}catch(...){rejected=true;}check(rejected,"Loading rejects an exclusion list beyond its supported limit");
        for(unsigned i=1;i<unsigned(FavoriteSource::count);++i)for(const auto kind:{Kind::sword,Kind::spell,Kind::potion,Kind::poison,Kind::food,Kind::ammo,Kind::scroll,Kind::shout,Kind::shield}) {
            Entry e;e.form=1;e.kind=kind;
            const bool expected=i==1||(i==2&&kind==Kind::sword)||(i==3&&kind==Kind::spell)||(i==4&&kind==Kind::potion)||(i==5&&kind==Kind::poison)||(i==6&&kind==Kind::food)||(i==7&&kind==Kind::ammo)||(i==8&&kind==Kind::scroll)||(i==9&&kind==Kind::shout)||(i==10&&kind==Kind::shield);
            check(favoriteMatches(FavoriteSource(i),e)==expected,"Each Favorites category uses item type rather than display name");
        }
    }
    {
        const auto identity=[](std::uint32_t id,std::uint32_t& mapped){mapped=id;return true;};
        const auto weapons=favoriteCategoryBit(FavoriteSource::weapons),offhand=favoriteCategoryBit(FavoriteSource::offhand);
        Arsenal arsenal;Options options;arsenal.presetIdentity="favorites-test";auto& group=arsenal.groups[0];
        Entry manual;manual.form=42;manual.owner=0x14;manual.unique=1;manual.name="Manual sword";group.add(manual);
        std::vector<Entry> favorites;
        for(unsigned i=0;i<24;++i){auto e=manual;e.form=100+i;e.unique=std::uint16_t(10+i);e.name="Weapon "+std::to_string(100+i);favorites.push_back(e);}
        auto copy=favorites.front();copy.unique=99;favorites.push_back(copy);favorites.push_back(favorites.front());
        Entry shield;shield.form=300;shield.kind=Kind::shield;shield.name="A shield";favorites.push_back(shield);
        Entry torch;torch.form=301;torch.kind=Kind::torch;torch.name="Torch";favorites.push_back(torch);
        Entry bow;bow.form=302;bow.kind=Kind::bow;bow.twoHanded=true;bow.name="Bow";favorites.push_back(bow);
        Entry potion;potion.form=303;potion.kind=Kind::potion;potion.name="Potion";favorites.push_back(potion);
        check(setFavoriteCategories(group,weapons|offhand,Hand::right)&&refreshFavorites(group,favorites),"Multiple categories populate a Favorites group");
        check(group.entries.size()==28&&group.manualEntries.size()==1,"Combined categories include every matching identity beyond 16 and deduplicate repeated inventory entries");
        check(group.entries.front().kind==Kind::shield&&group.entries[1].kind==Kind::bow&&group.entries[2].kind==Kind::torch,"Combined categories share one alphabetical list");
        check(group.entries[0].hand==Hand::left&&group.entries[1].hand==Hand::right&&group.entries[2].hand==Hand::left&&group.entries.back().hand==Hand::right,"Mixed groups apply shield, torch, two-handed and default-hand rules");
        group.selected=20;const auto remembered=group.entries[group.selected];
        Selection browse;browse.press(1,group);const auto queued=browse.release(group);
        check(setFavoriteCategories(group,weapons|offhand|favoriteCategoryBit(FavoriteSource::potions),Hand::left)&&refreshFavorites(group,favorites),"Adding another category refreshes membership");
        check(sameFavorite(group.entries[group.selected],remembered)&&!browse.accepts(*queued,group),"Category changes retain selected identity and invalidate an old queued index");
        check(std::all_of(group.entries.begin(),group.entries.end(),[](const auto& e){return e.hand==normalizedHand(e,Hand::left);}),"Changing default hand applies equipment restrictions throughout the combined group");
        group.selected=0;const auto excluded=group.entries.front();group.remove();refreshFavorites(group,favorites);
        const auto mask=favoriteCategoryMask(group);
        const auto saved=encodeArsenal(arsenal);auto loaded=decodeArsenal(saved,identity);auto& restored=loaded.groups[0];refreshFavorites(restored,favorites);
        check(favoriteCategoryMask(restored)==mask&&restored.excludedFavorites.size()==1&&restored.manualEntries[0].same(manual)&&restored.entries.size()==group.entries.size(),"Save/load preserves selected categories, exclusions and hidden manual entries");
        check(sameFavorite(restored.entries[restored.selected],group.entries[group.selected]),"Save/load retains the selected favorite in a combined group");
        const auto preset=exportPreset(arsenal,options,false,[](auto id){return Json{{"file","Favorites.esp"},{"local",id}};});
        const auto imported=importPreset(preset,arsenal,options,false,[](const auto&,auto id){return id;});
        check(favoriteCategoryMask(imported.arsenal.groups[0])==mask&&imported.arsenal.groups[0].manualEntries[0].same(manual),"Complete presets retain combined categories and manual contents");
        Arsenal otherCharacter;otherCharacter.presetIdentity="another-character";
        const auto foreign=importPreset(preset,otherCharacter,options,false,[](const auto&,auto id){return id;});
        check(favoriteCategoryMask(foreign.arsenal.groups[0])==mask&&foreign.arsenal.groups[0].manualEntries[0].unresolved,"Another character imports the category selection with unresolved exact manual copies");
        EditHistory history;history.begin(arsenal,options);history.prepare(arsenal,options);
        setFavoriteCategories(group,0,Hand::left);refreshFavorites(group,favorites);history.finish(true,false,1);
        check(group.automatic()&&group.entries.empty()&&group.manualEntries.size()==1&&group.excludedFavorites.size()==1,"Clearing all categories leaves an empty linked group and keeps its saved contents");
        const auto empty=decode(encode(group),identity);check(empty.automatic()&&!favoriteCategoryMask(empty),"An empty category selection survives save/load");
        check(history.undo(arsenal,options)&&favoriteCategoryMask(arsenal.groups[0])==mask,"Undo restores the combined category selection");
        auto& current=arsenal.groups[0];setFavoriteCategories(current,allFavoriteCategories,Hand::right);refreshFavorites(current,favorites);
        check(current.source==FavoriteSource::all&&current.entries.size()==28&&!favoriteMatches(Group{},manual),"All Favorites selects every category while Manual accepts no generated entries");
        check(std::none_of(current.entries.begin(),current.entries.end(),[&](const auto& e){return sameFavorite(e,excluded);}),"All Favorites retains group exclusions");
        setFavoriteCategories(current,0,Hand::right);setFavoriteSource(current,FavoriteSource::manual,Hand::right);
        check(current.entries.size()==1&&current.entries[0].same(manual),"Manual restores saved entries after an empty category selection");
        for(unsigned i=1;i<=unsigned(FavoriteSource::offhand);++i) {
            Group legacy;legacy.add(manual);setFavoriteSource(legacy,FavoriteSource(i),Hand::left);
            const auto bytes=encode(legacy);check(!Json::parse(bytes)["favorites"].contains("categories"),"Legacy single-category saves retain their original representation");
            auto migrated=decode(bytes,identity);refreshFavorites(migrated,favorites);
            check(migrated.source==FavoriteSource(i)&&migrated.manualEntries[0].same(manual)&&std::all_of(migrated.entries.begin(),migrated.entries.end(),[&](const auto& e){return favoriteMatches(FavoriteSource(i),e);}),"Every legacy source preserves its category and manual contents");
        }
        for(const auto bad:{Json(-1),Json(1.5),Json("3"),Json(512u),Json(UINT64_MAX),Json(nullptr)}) {
            auto invalid=Json::parse(saved);invalid["groups"][0]["favorites"]["categories"]=bad;bool rejected=false;
            try{decodeArsenal(invalid.dump(),identity);}catch(...){rejected=true;}check(rejected,"Invalid category masks are rejected before narrowing");
        }
        auto missing=Json::parse(saved);missing["groups"][0]["favorites"].erase("categories");bool rejected=false;
        try{decodeArsenal(missing.dump(),identity);}catch(...){rejected=true;}check(rejected,"A combined source requires a category selection");
        const auto before=encode(current);check(!setFavoriteCategories(current,allFavoriteCategories+1,Hand::right)&&encode(current)==before,"Invalid category edits leave the group intact");
    }
    {
        Group favorites;setFavoriteSource(favorites,FavoriteSource::weapons,Hand::right);
        std::vector<Entry> weapons;
        for(unsigned i=0;i<40;++i){Entry e;e.form=500+i;e.owner=0x14;e.unique=std::uint16_t(i);e.name="Weapon "+std::to_string(100+i);weapons.push_back(e);}
        refreshFavorites(favorites,weapons);favorites.selected=31;
        Options options;options.quickLootKeyboard=true;
        for(bool controller:{false,true}) {
            Selection select;select.press(0,favorites);
            const auto saved=favorites.entries[31];const auto commit=releaseToHand(select,favorites,0,Hand::left,controller);
            check(commit&&commit->index==31&&commit->hand==Hand::left&&select.accepts(*commit,favorites),"Hand actions select Favorites beyond the manual group limit");
            check(favorites.entries[31].same(saved)&&favorites.favoriteHand==Hand::right,"A per-use hand choice leaves Favorites defaults and membership unchanged");
            const bool allow=commit->controller?options.quickLootController:options.quickLootKeyboard;
            check(gameplayMenuOpen([](auto name){return name==quickLootMenuName;},MenuPurpose::input,allow)==controller,"The hand commit retains its device's QuickLoot policy");
            auto changed=weapons;changed.erase(changed.begin());refreshFavorites(favorites,changed);
            check(!select.accepts(*commit,favorites)&&sameFavorite(favorites.entries[favorites.selected],saved),"A Favorites refresh preserves the chosen identity while cancelling the old hand commit");
            refreshFavorites(favorites,weapons);
        }
    }
    {
        Group cycle;for(unsigned i=1;i<=3;++i){Entry e;e.form=0x500+i;e.owner=0x14;e.unique=std::uint16_t(i);e.name="Blade "+std::to_string(i);cycle.add(e);}
        Selection taps;CycleConfirm confirm;
        auto first=taps.requestCycle(cycle,0,1);
        check(first&&confirm.begin(*first,10,0)&&!confirm.active(),"A zero confirm delay equips on the tap");
        check(!confirm.begin(*first,10,.8f)&&confirm.active()&&confirm.index()==1,"A confirm delay holds the tapped choice open");
        check(!confirm.poll(10.5)&&confirm.active(),"The choice stays open until the delay ends");
        auto second=taps.requestCycle(cycle,0,2);
        check(!confirm.begin(*second,10.6,.8f)&&confirm.index()==2&&!confirm.poll(11.2),"Another tap moves on and restarts the delay");
        const auto equipped=confirm.poll(11.5);
        check(equipped&&equipped->index==2&&equipped->cycling&&taps.accepts(*equipped,cycle,0)&&!confirm.active(),"The last tapped choice equips once when the delay ends");
        check(!confirm.poll(12),"A confirmed choice equips a single time");
        confirm.begin(*second,13,.8f);confirm.cancel();check(!confirm.active()&&!confirm.poll(20),"Cancelling during the delay equips nothing");
        Options timing;timing.holdDelay=.35f;timing.cycleDelay=1.5f;auto saved=decodeOptions(encodeOptions(timing));
        check(saved.holdDelay==.35f&&saved.cycleDelay==1.5f,"Hold and confirm delays persist");
        timing.holdDelay=5;timing.cycleDelay=.05f;sanitizeOptions(timing);check(timing.holdDelay==.8f&&timing.cycleDelay==.2f,"Delays stay inside their ranges");
        auto earlier=Json::parse(encodeOptions(Options{}));earlier.erase("holdDelay");earlier.erase("cycleDelay");
        saved=decodeOptions(earlier.dump());check(saved.holdDelay==0&&saved.cycleDelay==0,"Earlier settings leave the hold delay to the INI and the confirm delay off");
        Eased eased;
        check(eased.update(1,100,.12f)>0&&eased.value()<1,"A fresh appearance starts partly visible and eases in");
        check(eased.update(1,100.06,.12f)>.5f&&eased.update(1,100.2,.12f)==1,"Easing reaches the target over its rise time");
        check(eased.update(0,100.21,.12f)==0,"Without a fall time the value follows a drop at once");
        eased.update(1,100.22,.1f,.08f);eased.update(1,100.4,.1f,.08f);const float closing=eased.update(0,100.44,.1f,.08f);
        check(closing>0&&closing<1&&eased.update(0,100.6,.1f,.08f)==0,"A fall time eases the value out");
        eased.update(1,100.61,.1f);eased.update(1,100.8,.1f);check(eased.update(1,105,.1f)<1,"After a pause in drawing the value eases in again");
        TilePulse pulse;
        check(pulse.update(true,false,0x10,1,1,false,1)<0&&pulse.update(true,false,0x10,1,1,false,2)<0,"An unchanged group stays still");
        check(pulse.update(true,true,0x10,1,1,false,3)==0,"Equipping the group's item starts a pulse");
        const float middle=pulse.update(true,true,0x10,1,1,false,3.2);check(middle>0&&middle<1&&pulse.update(true,true,0x10,1,1,false,3.5)<0,"The pulse plays once and ends");
        check(pulse.update(true,true,0x20,2,1,false,4)==0,"Switching to another equipped item pulses again");
        check(pulse.update(true,false,0x20,2,1,false,5)<0,"Unequipping does not pulse");
        TilePulse doses;doses.update(true,false,0x30,0,5,true,1);
        check(doses.update(true,false,0x30,0,4,true,2)==0&&doses.update(true,false,0x30,0,6,true,3)<0,"Using a dose pulses; restocking does not");
        TilePulse appeared;check(appeared.update(true,true,0x10,1,1,false,1)<0,"Items already equipped when the HUD appears do not pulse");
        CooldownScale cooldown;
        check(cooldown.fraction(0x40,30)==1&&cooldown.fraction(0x40,15)==.5f&&cooldown.fraction(0x40,0)==0,"A cooldown shade shrinks from full to empty");
        check(cooldown.fraction(0x40,10)==1,"Each cooldown starts from its own length");
        Options feel;feel.animateHud=false;feel.uiSounds=false;feel.equippedMarkerScale=9;feel.customEquippedColor=true;feel.equippedColor={.1f,.2f,2.f};
        auto felt=decodeOptions(encodeOptions(feel));
        check(!felt.animateHud&&!felt.uiSounds&&felt.equippedMarkerScale==2.5f&&felt.customEquippedColor&&felt.equippedColor[2]==1.f,"Motion, sound and equipped-marker settings persist inside their ranges");
        Options quiet;quiet.uiSounds=false;Options skin;skin.equippedMarkerScale=2;importAppearance(quiet,skin);
        check(!quiet.uiSounds&&quiet.equippedMarkerScale==2,"Appearance presets carry the marker size and leave sounds in place");
        check(wheelIndex(0,1,8,.3f)==std::size_t(0)&&wheelIndex(1,0,8,.3f)==std::size_t(2)&&wheelIndex(0,-1,8,.3f)==std::size_t(4)&&wheelIndex(-1,0,8,.3f)==std::size_t(6),"Wheel slots run clockwise from the top");
        check(wheelIndex(-.1f,1,8,.3f)==std::size_t(0)&&wheelIndex(.7f,.7f,8,.3f)==std::size_t(1)&&wheelIndex(-.7f,.7f,8,.3f)==std::size_t(7),"Directions select the nearest slot and wrap at the top");
        check(!wheelIndex(.1f,.1f,8,.3f)&&!wheelIndex(1,0,0,.3f)&&wheelIndex(1,0,1,.3f)==std::size_t(0)&&wheelIndex(0,-1,3,.3f)==std::size_t(2),"A resting stick selects nothing; any count maps cleanly");
        WheelPointer pointer;pointer.move(5,5);check(!pointer.index(4),"Small mouse movement leaves the highlight in place");
        pointer.move(60,0);check(pointer.index(4)==std::size_t(1),"Moving the mouse right points at the right slot");
        pointer.move(-400,0);check(pointer.index(4)==std::size_t(3)&&pointer.x()>=-1.f,"The pointer has limited reach so a reversal responds at once");
        pointer.move(90,-400);check(pointer.index(4)==std::size_t(0),"Moving the mouse up points at the top slot");
        pointer.reset();check(!pointer.index(4)&&pointer.x()==0,"Each selection starts from the hub");
        Selection wheel;wheel.press(20,cycle,0);wheel.point(2,cycle);
        check(wheel.browsing()&&wheel.index()==2,"Pointing opens the selector on the chosen entry");
        wheel.point(9,cycle);check(wheel.index()==2,"Pointing past the entries keeps the highlight");
        const auto pointed=wheel.release(cycle,0);check(pointed&&pointed->index==2&&pointed->browsed,"Releasing equips the pointed entry");
        wheel.press(21,cycle,0);cycle.add(Entry{.form=0x600,.owner=0x14,.unique=9,.name="New blade"});wheel.point(1,cycle);check(!wheel.held(),"A changed group cancels pointing");
        Options round;round.selector=wheelSelector;round.selectorSpacing=8;
        for(const std::size_t count:{std::size_t(1),std::size_t(2),std::size_t(5),std::size_t(16)}) {
            const auto layout=selectorGeometry(round,count);bool clear=layout.ring.size()==count;
            for(std::size_t i=0;i<count;++i)for(std::size_t j=0;j<i;++j)clear&=std::hypot(layout.ring[i].x-layout.ring[j].x,layout.ring[i].y-layout.ring[j].y)>=50+round.selectorSpacing-.01f;
            for(const auto slot:layout.ring)clear&=slot.x-29>=0&&slot.x+29<=layout.width&&slot.y+29<layout.ruleY;
            check(clear,"Wheel slots stay apart and inside the selector for every group size");
        }
        check(std::abs(selectorGeometry(round,6).ring[0].x-selectorGeometry(round,6).hub.x)<.01f&&selectorGeometry(round,6).ring[0].y<selectorGeometry(round,6).hub.y,"The first wheel slot sits at the top");
        round.selector=9;sanitizeOptions(round);check(round.selector==wheelSelector,"The wheel is the last selector style");
        {
            Arsenal loadout;Entry blade;blade.form=0x700;blade.owner=0x14;blade.unique=1;blade.name="Blade";Entry bow;bow.form=0x701;bow.owner=0x14;bow.unique=2;bow.kind=Kind::bow;bow.twoHanded=true;bow.name="Bow";
            Entry staff;staff.form=0x702;staff.owner=0x14;staff.unique=3;staff.kind=Kind::staff;staff.name="Staff";
            loadout.groups[0].add(blade);loadout.groups[0].add(staff);loadout.groups[1].add(bow);
            ChoiceHistory history;check(!history.target(loadout),"There is nothing to return to before two selections");
            history.remember(0,blade);history.remember(0,blade);check(!history.target(loadout),"Re-selecting the same entry keeps one choice");
            history.remember(1,bow);check(history.target(loadout)==std::pair<std::size_t,std::size_t>{0,0},"The earlier selection is the return target");
            history.remember(0,blade);check(history.target(loadout)==std::pair<std::size_t,std::size_t>{1,0},"Returning swaps the two choices so the action toggles");
            history.remember(0,staff);loadout.groups[0].reorder(0,1);check(history.target(loadout)==std::pair<std::size_t,std::size_t>{0,1},"The target follows its entry after reordering");
            history.remember(1,bow);loadout.groups[0].selected=1;loadout.groups[0].remove();check(history.target(loadout)==std::pair<std::size_t,std::size_t>{0,0},"Removing another entry keeps the target");
            loadout.groups[0].selected=0;loadout.groups[0].remove();check(!history.target(loadout),"A removed entry is no longer a target");
            history.clear();check(!history.current&&!history.previous,"Loading a game clears the history");
            Options keys;check(assignSwapBinding(keys,{47,0})&&swapShortcutMatches(keys,47,0)&&!swapShortcutMatches(keys,47,keyModifier::shift),"The return action takes a key and exact modifiers");
            check(!assignSwapBinding(keys,{keys.keys[0],0})&&!assignConsumableBinding(keys,{47,0})&&!assignGroupKey(keys,0,47)&&!assignSelectorBinding(keys,KeyboardAction::cancel,{47,0}),"The return key stays distinct from group, consumable and selector keys");
            keys.doubleTapSwap=true;auto kept=decodeOptions(encodeOptions(keys));check(kept.swapKey==KeyBinding{47,0}&&kept.doubleTapSwap,"The return binding and double tap persist");
            Options other;importAppearance(kept,other);check(kept.swapKey==KeyBinding{47,0}&&kept.doubleTapSwap,"Appearance presets leave the return binding in place");
            MenuAPIState actions;check(!actions.requestAction({MenuAPIState::Action::Type::swapPrevious,0})&&actions.groups()==0,"Gameplay requests wait for a loaded game");
            actions.publish(true,false);actions.publishGroups(3);
            check(actions.groups()==3&&actions.requestAction({MenuAPIState::Action::Type::tapGroup,2})&&!actions.requestAction({MenuAPIState::Action::Type::tapGroup,3}),"Group taps are limited to the groups shown");
            check(actions.requestAction({MenuAPIState::Action::Type::swapPrevious,0})&&actions.takeActions().size()==2&&actions.takeActions().empty(),"Requests reach the game thread once, in order");
            actions.publish(true,true);check(!actions.requestAction({MenuAPIState::Action::Type::swapPrevious,0}),"Gameplay requests are declined while the editor is open");
            actions.publish(false,false);check(actions.groups()==0,"Unloading clears the published group count");
        }
        Options imported;imported.holdDelay=.5f;imported.cycleDelay=2;Options look;look.themeId="edge";importAppearance(imported,look);
        check(imported.themeId=="edge"&&imported.holdDelay==.5f&&imported.cycleDelay==2,"Appearance presets leave timing settings in place");
    }
    std::cout<<"PASS: "<<checks<<" browse/cycle, compact equipped HUD, settings and menu API checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

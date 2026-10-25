#include "Controller.h"
#include "GameplayMenus.h"
#include "EquippedState.h"
#include "HudMotion.h"
#include "Layout.h"
#include "MenuAPI.h"
#include "EditorLaunch.h"
#include "EditorTextInput.h"
#include "Presets.h"
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
    check(decodeOptions(oldSpacing.dump()).equipmentSpacing==1,"Existing settings retain their equipment spacing");
    auto oldSizing=Json::parse(encodeOptions(Options{}));oldSizing.erase("equipmentScale");oldSizing.erase("equipmentTextScale");
    check(decodeOptions(oldSizing.dump()).equipmentScale==1&&decodeOptions(oldSizing.dump()).equipmentTextScale==1,"Existing settings retain equipment's original size");
    visual.equipmentScale=visual.equipmentTextScale=visual.equipmentSpacing=1;
    check(reloaded.equipmentDim&&reloaded.equipmentStyle==2&&reloaded.equipmentLayout==1&&reloaded.equipmentTextPosition==0&&reloaded.equipmentFadeDelay==2&&reloaded.equipmentIdleOpacity==.2f,"Equipment options survive settings reload");
    visual.equipmentStyle=0;
    check(imported.groupModes==o.groupModes&&imported.autoSort==o.autoSort,"Appearance preserves play controls and per-group sorting");
    Options bad;bad.groupModes[0]=GroupMode(99);bad.inactiveGroups=99;bad.equipmentX=-1;bad.equipmentY=2;
    sanitizeOptions(bad);check(bad.groupModes[0]==GroupMode::browse&&bad.inactiveGroups==2&&bad.equipmentX==0&&bad.equipmentY==1,"Invalid settings bounded");
    bad.equipmentScale=10;bad.equipmentTextScale=-1;sanitizeOptions(bad);
    check(bad.equipmentScale==2&&bad.equipmentTextScale==.75f,"Equipment scale limits bounded");
    bad.equipmentScale=std::numeric_limits<float>::infinity();bad.equipmentTextScale=std::numeric_limits<float>::quiet_NaN();sanitizeOptions(bad);
    check(bad.equipmentScale==1&&bad.equipmentTextScale==1,"Invalid equipment scales restore finite defaults");
    for(const auto value:{-1.f,5.f,std::numeric_limits<float>::quiet_NaN()}) {
        bad.equipmentSpacing=value;sanitizeOptions(bad);
        check(bad.equipmentSpacing==(std::isfinite(value)?std::clamp(value,0.f,4.f):1.f),"Invalid equipment spacing is bounded with a finite default");
    }
    bad.equipmentStyle=99;bad.equipmentLayout=99;bad.equipmentTextPosition=99;bad.equipmentIdleOpacity=0;bad.equipmentFadeDelay=100;
    sanitizeOptions(bad);check(bad.equipmentStyle==4&&bad.equipmentLayout==1&&bad.equipmentTextPosition==3&&bad.equipmentIdleOpacity==.05f&&bad.equipmentFadeDelay==20,"Equipment settings bounded; dim cannot hide completely");

    group.selected=2;rememberConsumable(a,0,2);a.groups[1].add(potion);a.active=1;
    check(selectedConsumableGroup(a)==0,"Last chosen consumable survives another group");
    const auto loaded=decodeArsenal(encodeArsenal(a),[](auto id,auto& mapped){mapped=id;return true;});
    check(loaded.consumableGroup==0&&selectedConsumableGroup(loaded)==0,"Consumable choice saved");
    group.selected=0;check(selectedConsumableGroup(a)==1,"Invalidated consumable falls back");

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
    for(unsigned style=0;style<5;++style)for(unsigned text=0;text<4;++text)for(unsigned layout=0;layout<2;++layout)
        for(float t:{.75f,1.5f})for(float edge:{0.f,1.f})for(unsigned count:{3u,4u})for(float spacing:{0.f,1.f,4.f}) {
            Options look;look.equipmentStyle=style;look.equipmentTextPosition=text;look.equipmentLayout=layout;look.textScale=t;look.equipmentX=look.equipmentY=edge;
            look.equipmentScale=2;look.equipmentTextScale=1.5f;look.equipmentSpacing=spacing;
            const auto g=equipmentGeometry(look,1280,720,count);
            check(g.origin.x>=8&&g.origin.y>=8&&g.origin.x+g.width*g.scale<=1272.1f&&g.origin.y+g.height*g.scale<=712.1f,"Every equipment layout fits screen extremes");
            if(count==4) {
                const bool horizontal=style&&layout==1;
                check(horizontal?g.cells[1].x>g.cells[0].x&&g.cells[1].y==g.cells[0].y:g.cells[1].y>g.cells[0].y&&g.cells[1].x==g.cells[0].x,"Equipment cells follow requested arrangement");
                if(style>0&&style<4)check(text==0?g.text.y<g.icon.y:text==1?g.text.y>g.icon.y:text==2?g.text.x<g.icon.x:g.text.x>g.icon.x,"Text uses the requested side of its icon");
                if(style==4)check(g.iconSize==0,"Text-only layout has no icon");
            }
        }
    for(unsigned style=0;style<5;++style)for(unsigned layout=0;layout<2;++layout) {
        Options spaced;spaced.equipmentStyle=style;spaced.equipmentLayout=layout;
        const auto normal=equipmentGeometry(spaced,3840,2160);
        spaced.equipmentSpacing=0;const auto tight=equipmentGeometry(spaced,3840,2160);
        spaced.equipmentSpacing=4;const auto loose=equipmentGeometry(spaced,3840,2160);
        const auto pitch=[&](const auto& g){return style&&layout?g.cells[1].x-g.cells[0].x:g.cells[1].y-g.cells[0].y;};
        check(pitch(tight)<pitch(normal)&&pitch(normal)<pitch(loose),"Spacing adjusts separation in every equipment style and arrangement");
        check(tight.iconSize==normal.iconSize&&normal.iconSize==loose.iconSize&&tight.textWidth==loose.textWidth&&tight.scale==loose.scale,"Spacing preserves icon/text size while there is screen room");
        check(style?pitch(tight)>=(layout?normal.cellWidth:normal.cellHeight):pitch(tight)>=40,"Minimum spacing leaves readable rows without overlapping frames");
        const auto one=equipmentGeometry(spaced,3840,2160,1);spaced.equipmentSpacing=0;
        const auto oneTight=equipmentGeometry(spaced,3840,2160,1);
        check(one.width==oneTight.width&&one.height==oneTight.height,"Spacing adds no outer padding to a single equipment entry");
    }
    for(unsigned style=0;style<5;++style)for(unsigned layout=0;layout<2;++layout)for(unsigned text=0;text<4;++text) {
        Options a;a.equipmentStyle=style;a.equipmentLayout=layout;a.equipmentTextPosition=text;a.equipmentScale=2;a.textScale=1.5f;a.equipmentSpacing=0;
        auto b=a;b.equipmentSpacing=4;
        const auto tight=equipmentGeometry(a,1280,720),loose=equipmentGeometry(b,1280,720);
        check(tight.scale==loose.scale,"Equipment screen fitting preserves size across the spacing range");
        const auto previewTight=previewGeometry(a,1280,720,600,440,false,false,true).equipment;
        const auto previewLoose=previewGeometry(b,1280,720,600,440,false,false,true).equipment;
        check(std::abs(previewTight.scale-previewLoose.scale)<.001f,"Equipment preview spacing cannot resize icons or text");
    }
    Options sized;sized.equipmentStyle=1;
    const auto previewEquipment=[&]{return previewGeometry(sized,1920,1080,900,700,false,false,true).equipment;};
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
        look.showPoison=false;look.showAmmo=false;const auto saved=decodeOptions(encodeOptions(look));
        check(!saved.showPoison&&!saved.showAmmo,"Status settings persist");
        auto legacy=Json::parse(encodeOptions(look));legacy.erase("showPoison");legacy.erase("showAmmo");const auto migrated=decodeOptions(legacy.dump());
        check(migrated.showPoison&&migrated.showAmmo,"Existing settings gain both status indicators");
        Options appearance;importAppearance(appearance,saved);check(!appearance.showPoison&&!appearance.showAmmo,"Appearance imports include status display preferences");
    }
    std::cout<<"PASS: "<<checks<<" browse/cycle, compact equipped HUD, settings and menu API checks\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

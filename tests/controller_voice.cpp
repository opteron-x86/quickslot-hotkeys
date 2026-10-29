#include "Controller.h"
#include "GameplayMenus.h"
#include "HudMenuState.h"
#include "InputFilter.h"
#include "VoiceAction.h"
#include "Presets.h"
#include "KeyGlyphs.h"
#include <iostream>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool ok,const char* why,std::source_location at=std::source_location::current()) {
    if(!ok)throw std::runtime_error(std::string(why)+" at "+std::to_string(at.line()));++checks;
}
int main()try {
    {
        HudMenuState hud;
        check(!hud.request(false,false),"The HUD stays off the menu stack before gameplay");
        check(hud.request(true,false)==true,"Gameplay queues the HUD show message");
        check(!hud.request(true,false),"Repeated input frames share the pending show message");
        check(!hud.request(false,false),"Opening a custom menu waits for an already queued HUD show");
        hud.processed();
        check(hud.request(false,true)==false,"A late HUD show is followed by a hide while a custom menu owns the screen");
        check(!hud.request(false,true),"The pending hide is queued once");
        check(!hud.request(true,true),"Closing the custom menu waits for an already queued HUD hide");
        hud.processed();
        check(hud.request(true,false)==true,"A late HUD hide is followed by a show after returning to gameplay");
        hud.processed();
        check(!hud.request(true,true),"An open gameplay HUD needs no message");
        check(hud.request(false,true)==false,"Another interface removes the HUD from the menu stack");
        hud.processed();
        check(!hud.request(false,false),"A hidden HUD stays absent while another interface is open");
        hud.processed();
        check(hud.request(true,false)==true,"An externally removed HUD can reopen during gameplay");
    }
    {
        HudMenuState hud;
        check(!hud.request(false,true,true),"Opening the console keeps an existing HUD on the stack");
        check(!hud.request(true,false,true),"The console also defers HUD shows until gameplay resumes");
        check(!hud.request(true,true),"Closing the console keeps an existing HUD without another stack change");
        check(hud.request(false,true)==false,"A custom menu can still remove the HUD after the console closes");
        check(hud.accept(false),"HUD transitions proceed while the console is closed");
        hud.processed();
        check(!hud.request(false,false),"The HUD remains absent in the custom menu");
        check(hud.request(true,false)==true,"Closing the custom menu restores the gameplay HUD");
        hud.processed();
    }
    for(const bool show:{false,true}) {
        HudMenuState hud;
        const bool onStack=!show;
        check(hud.request(show,onStack)==show,"A HUD transition can be queued before the console opens");
        check(!hud.request(false,onStack,true),"The console leaves the pending transition for delivery to resolve");
        check(!hud.accept(true),"Delivery rejects a queued HUD transition while the console owns its cursor");
        check(!hud.request(false,onStack,true),"Rejecting a transition does not requeue it while the console is open");
        check(hud.request(show,onStack)==show,"The rejected transition releases its pending state for retry after closing the console");
        check(hud.accept(false),"The retried transition can update the HUD stack");
        hud.processed();
        check(!hud.request(show,show),"Successful delivery reaches the requested HUD state");
    }
    {
        HudMenuState hud;
        check(hud.request(false,true)==false,"Opening the editor can queue a HUD hide");
        check(!hud.accept(true),"Opening the console before that hide arrives cancels it");
        check(!hud.request(true,true),"Returning to gameplay preserves the HUD when the cancelled hide is no longer needed");
        check(hud.request(false,true)==false,"A later custom-menu hide is independent of the cancelled message");
        check(hud.accept(false),"A console that closes before delivery leaves the queued hide valid");
        hud.processed();
        check(hud.request(true,false)==true,"Gameplay restores the HUD after a late hide");
        hud.processed();
    }
    {
        Options controls;ControllerInput input;
        controls.controllerGroups[0]={PadButton::up,PadButton::x};
        input.button(PadButton::right,true,true,controls,true,false);
        check(input.button(PadButton::x,true,true,controls,true,true).action==PadAction::none,"Pressing an explicit X modifier while browsing does not unequip");
        input.reset();input.button(PadButton::x,true,true,controls,true,false);
        check(input.button(PadButton::up,true,true,controls,true,false).action==PadAction::press,"X plus a group key starts the intended group");
        check(input.button(PadButton::x,false,false,controls,true,true).action==PadAction::cancel,"Releasing X first cancels its chord");
        check(input.button(PadButton::up,false,false,controls,true,false).action==PadAction::none,"A late group release cannot equip after modifier cancellation");
        input.reset();controls.controllerActions[unsigned(ControllerAction::unequip)]={PadButton::y};
        input.button(PadButton::x,true,true,controls,true,false);input.button(PadButton::up,true,true,controls,true,false);
        check(input.button(PadButton::y,true,true,controls,true,true).action==PadAction::unequip,"Remapped unequip works while a group modifier remains held");
        check(input.button(PadButton::y,false,false,controls,true,false).consumed&&input.button(PadButton::up,false,false,controls,true,false).action==PadAction::none,"Remapped unequip balances its release without also equipping");
        input.reset();controls.controllerActions[unsigned(ControllerAction::cancel)]={PadButton::y,PadButton::lb};
        input.button(PadButton::right,true,true,controls,true,false);
        check(input.button(PadButton::lb,true,true,controls,true,true).action==PadAction::none,"A browsing-action modifier does not trigger the old bumper action");
        check(input.button(PadButton::y,true,true,controls,true,true).action==PadAction::cancel,"A configured cancellation chord takes precedence over a plain Y action");
        input.reset();controls.controllerActions[unsigned(ControllerAction::unequip)]={};
        input.button(PadButton::right,true,true,controls,true,false);
        check(!input.button(PadButton::y,true,true,controls,true,true).consumed,"An unbound browsing action passes through");
        input.reset();input.button(PadButton::right,true,true,controls,true,false);
        check(input.button(PadButton::b,true,true,controls,true,true,true).action==PadAction::cancel,"Desktop remapping does not change VR cancel defaults");
    }
    Options o;ControllerInput pad;Selection selection;Group group;
    {
        Options controls;Selection::Commit c{0,1,0};Entry item;
        for(unsigned k=0;k<=unsigned(Kind::smartAmmo);++k)for(bool gamepad:{false,true}) {
            item.kind=Kind(k);c.controller=gamepad;c.browsed=false;c.cycling=false;
            check(selectionAction(item,c,controls)==(isConsumable(item)||isVoice(item)?SelectionAction::use:SelectionAction::equip),"Normal tap behavior is consistent across devices");
            c.browsed=true;check(selectionAction(item,c,controls)==(isConsumable(item)?SelectionAction::select:SelectionAction::equip),"Browse never spends items or casts powers");
            c.browsed=false;c.cycling=true;check(selectionAction(item,c,controls)==(isConsumable(item)?SelectionAction::select:SelectionAction::equip),"Cycle equips without activation");
        }
        controls.groupModes[0]=GroupMode::cycle;controls.controllerModifiers&=~padButtonMask(PadButton::rt);
        const auto saved=decodeOptions(encodeOptions(controls));
        check(saved.groupModes==controls.groupModes&&saved.controllerModifiers==controls.controllerModifiers,"Modes and modifier exceptions persist");
        Options appearance;const auto local=appearance;importAppearance(appearance,saved);
        check(appearance.groupModes==local.groupModes&&appearance.controllerModifiers==local.controllerModifiers,"Appearance import preserves controls");
        auto broken=controls;broken.groupModes[0]=GroupMode(999);broken.controllerModifiers=~0u;sanitizeOptions(broken);
        check(broken.groupModes[0]==GroupMode::browse&&broken.controllerModifiers==allPadButtons,"Invalid settings default to browse");
        Arsenal a;reorderGroup(controls,0,1);check(controls.groupModes[0]==GroupMode::cycle,"Mode follows stable group ID");
        removeGroup(a,controls,0);check(controls.groupModes[0]==GroupMode::browse,"Removed group resets mode");
    }
    for(const auto codes:{PadCodeSet::xinput,PadCodeSet::orbis})for(unsigned modifier=5;modifier<unsigned(PadButton::count);++modifier) {
        Options controls;ControllerInput input;const auto mod=PadButton(modifier);
        input.button(mod,true,true,controls,true,false);
        const auto trigger=padButton(padCodes(codes)[unsigned(PadButton::up)],codes);
        check(!input.button(trigger,true,true,controls,true,false).consumed&&!input.active(),"A foreign controller chord does not start a bare D-pad group");
        input.button(mod,false,false,controls,true,false);
        check(!input.button(trigger,true,false,controls,true,false).consumed,"Releasing a foreign modifier cannot adopt an already-held D-pad");
        check(!input.button(trigger,false,false,controls,true,false).consumed,"Unmatched chord releases pass through");
        check(input.button(trigger,true,true,controls,true,false).action==PadAction::press,"Bare binding works after foreign chord is released");input.reset();
        PadState physical{};physical[unsigned(mod)]=physical[unsigned(trigger)]=true;input.observe(physical);
        check(!input.button(trigger,true,true,controls,true,false).consumed,"Physical snapshot protects chords when D-pad event comes first");
        input.reset();controls.controllerGroups[0]={trigger,mod};input.observe(physical);
        check(input.button(trigger,true,true,controls,true,false).action==PadAction::press,"Explicit chord works in either event order");
        physical[unsigned(trigger)]=false;input.observe(physical);
        check(input.button(trigger,false,false,controls,true,true).action==PadAction::release,"Pre-dispatch sampling does not discard a legitimate group release");
        input.reset();controls.controllerGroups[0]={trigger};controls.controllerModifiers&=~padButtonMask(mod);
        input.button(mod,true,true,controls,true,false);
        check(input.button(trigger,true,true,controls,true,false).action==PadAction::press,"Exempted held button permits bare binding");
    }
    {
        Options controls;ControllerInput input;controls.controllerGroups[0]={PadButton::up,PadButton::lb};
        input.button(PadButton::lb,true,true,controls,true,false);input.button(PadButton::b,true,true,controls,true,false);
        check(!input.button(PadButton::up,true,true,controls,true,false).consumed,"An additional modifier prevents a partial chord match");
        input.reset();controls.controllerEditor={PadButton::up};input.button(PadButton::b,true,true,controls,true,false);
        check(!input.button(PadButton::up,true,true,controls,true,false).consumed,"Unmodified editor binding also respects foreign modifiers");
    }
    Entry sword;sword.form=1;group.add(sword);Entry potion;potion.form=2;potion.kind=Kind::potion;group.add(potion);group.selected=0;
    for(unsigned i=1;i<unsigned(PadButton::count);++i)check(padButton(padNativeCodes[i])==PadButton(i),"Native button masks map bijectively, including triggers");
    check(padButton(0)==PadButton::none&&padButton(0xffff)==PadButton::none,"Unknown controller input does not alias a group");

    check(padButton(0x10,PadCodeSet::orbis)==PadButton::up&&padButton(0x10)==PadButton::start,"PS4 D-pad Up is not Xbox Start");
    check(padButton(0x4000,PadCodeSet::orbis)==PadButton::a&&padButton(0x4000)==PadButton::x,"PS4 Cross is not Xbox X");
    check(padButton(0x1000,PadCodeSet::orbis)==PadButton::y&&padButton(0x1000)==PadButton::a,"PS4 Triangle is not Xbox A");
    check(padButton(1,PadCodeSet::orbis)==PadButton::none&&padButton(0x100,PadCodeSet::orbis)==PadButton::none&&padButton(0xffff,PadCodeSet::orbis)==PadButton::none,"Unused PS4 Share and trigger bit flags cannot alias shortcut buttons");
    check(padButton(0x100000,PadCodeSet::orbis)==PadButton::back&&padLabel(PadButton::back,1)=="Touchpad","Native PS4 Back uses the touchpad, not Share");
    for(const auto codes:{PadCodeSet::xinput,PadCodeSet::orbis}) {
        for(unsigned i=1;i<unsigned(PadButton::count);++i) {
            const auto button=PadButton(i);const auto native=padCodes(codes)[i];
            check(padButton(native,codes)==button,"Every native button event maps to its logical binding");
            const auto physical=padPhysicalState(codes,button==PadButton::lt||button==PadButton::rt?0:native,
                button==PadButton::lt?31:0,button==PadButton::rt?31:0);
            for(unsigned other=0;other<physical.size();++other)
                check(physical[other]==(other==i),"Raw polling identifies exactly one held button without false trigger presses");
        }
        ControllerInput device;Options controls;
        auto press=device.button(padButton(padCodes(codes)[unsigned(PadButton::up)],codes),true,true,controls,true,false);
        check(press.action==PadAction::press&&press.group==0,"Native D-pad Up starts the first group");
        check(!device.synchronize(padPhysicalState(codes,padCodes(codes)[unsigned(PadButton::up)],0,0))&&device.active(),"Fresh physical press survives synchronization without a nonzero held duration");
        auto release=device.button(padButton(padCodes(codes)[unsigned(PadButton::up)],codes),false,false,controls,true,true);
        check(release.action==PadAction::release&&release.group==0,"Native D-pad release commits the held group once");
        check(!device.synchronize(padPhysicalState(codes,0,0,0))&&!device.active(),"Physical release after commit is balanced");
        controls.controllerGroups[0]={PadButton::lt,PadButton::none};
        check(device.button(padButton(9,codes),true,true,controls,true,false).action==PadAction::press,"Trigger group starts from the game-defined axis event");
        check(!device.synchronize(padPhysicalState(codes,0,31,0))&&device.active(),"Analog trigger holds the group above its threshold");
        check(device.synchronize(padPhysicalState(codes,0,30,0))&&!device.active(),"Lost trigger release cancels at the threshold");
        check(device.button(padButton(9,codes),false,false,controls,true,false).action==PadAction::none,"Late trigger release cannot activate after cancellation");
        controls.controllerGroups[0]={PadButton::right,PadButton::lb};
        device.button(padButton(padCodes(codes)[unsigned(PadButton::lb)],codes),true,true,controls,true,false);
        check(device.button(padButton(padCodes(codes)[unsigned(PadButton::right)],codes),true,true,controls,true,false).group==0&&device.active(),"Native shoulder modifier takes precedence over the plain D-pad binding");
        check(!device.synchronize(padPhysicalState(codes,padCodes(codes)[unsigned(PadButton::lb)]|padCodes(codes)[unsigned(PadButton::right)],0,0)),"Physical chord state retains both buttons");
        check(device.synchronize(padPhysicalState(codes,padCodes(codes)[unsigned(PadButton::right)],0,0)),"Lost modifier release cancels the chord without activation");
    }
    auto r=pad.button(PadButton::up,true,true,o,true,false);
    check(r.consumed&&r.action==PadAction::press&&r.group==0,"D-pad press starts group one");selection.press(1,group,r.group);
    check(pad.button(PadButton::up,true,false,o,true,true).action==PadAction::none,"Held repeats cannot start another action");
    r=pad.button(PadButton::up,false,false,o,true,true);check(r.consumed&&r.action==PadAction::release,"Group release is captured");
    auto commit=selection.release(group);check(commit&&!commit->browsed,"A tap commits the remembered entry once");
    check(!selection.release(group)&&!pad.button(PadButton::up,false,false,o,true,false).consumed,"Duplicate releases cannot commit again");
    pad.button(PadButton::up,true,true,o,true,false);selection.press(2,group);
    r=pad.button(PadButton::rb,true,true,o,true,true);check(r.consumed&&r.action==PadAction::next,"Bumper cycles only during selection");selection.cycle(1,group);
    check(pad.button(PadButton::rb,false,false,o,true,true).consumed,"Cycling owns its corresponding release");
    pad.button(PadButton::up,false,false,o,true,true);commit=selection.release(group);
    check(commit&&commit->browsed&&commit->index==1&&selectsConsumable(potion,*commit,o),"Controller browsing selects a potion without consuming it");
    check(!pad.button(PadButton::rb,true,true,o,true,false).consumed,"Ordinary combat bumper press passes through outside selection");pad.button(PadButton::rb,false,false,o,true,false);
    pad.button(PadButton::up,true,true,o,true,false);selection.press(3,group);
    r=pad.button(PadButton::b,true,true,o,true,true);check(r.action==PadAction::cancel&&r.consumed,"B cancels");selection.cancel();
    check(pad.button(PadButton::up,false,false,o,true,false).action==PadAction::none&&!selection.release(group),"Releasing after cancel never equips");pad.button(PadButton::b,false,false,o,true,false);
    pad.button(PadButton::up,true,true,o,true,false);
    check(pad.button(PadButton::x,true,true,o,true,true).action==PadAction::unequip,"X clears the highlighted entry");
    check(pad.button(PadButton::up,false,false,o,true,false).action==PadAction::none,"Unequip does not also commit on release");pad.button(PadButton::x,false,false,o,true,false);
    o.groupCount=5;o.controllerGroups[4]={PadButton::up,PadButton::lb};
    check(!pad.button(PadButton::lb,true,true,o,true,false).consumed,"A modifier alone retains its normal input");
    r=pad.button(PadButton::up,true,true,o,true,false);check(r.group==4&&r.action==PadAction::press,"Chord takes precedence over base button");
    r=pad.button(PadButton::lb,false,false,o,true,true);check(!r.consumed&&r.action==PadAction::cancel,"Releasing modifier first cancels and balances its engine release");
    check(pad.button(PadButton::up,false,false,o,true,false).action==PadAction::none,"No late activation after modifier release");
    o.controllerGroups[4]={PadButton::b,PadButton::up};
    pad.button(PadButton::up,true,true,o,true,false);
    r=pad.button(PadButton::b,true,true,o,true,true);check(r.action==PadAction::press&&r.group==4,"An explicit chord overrides contextual cancel while another group is held");
    r=pad.button(PadButton::up,false,false,o,true,true);check(r.consumed&&r.action==PadAction::cancel,"Owned group used as modifier has balanced cancellation on early release");
    check(pad.button(PadButton::b,false,false,o,true,false).action==PadAction::none,"Chord's late trigger release cannot cast after cancelling an owned modifier");
    pad.button(PadButton::up,true,true,o,true,false);
    check(pad.button(PadButton::up,false,false,o,false,true).action==PadAction::none,"A menu opening invalidates the held gesture");
    check(gameplayMenuOpen([](auto name){return name=="LootMenu";}),"QuickLoot owns navigation without pause or menu-context flags");
    check(!menuFlagsBlock(false,true,false,false,MenuPurpose::editor),"Cursor-only text-entry overlay does not dismiss the editor");
    for(const auto purpose:{MenuPurpose::input,MenuPurpose::hud,MenuPurpose::capture})
        check(menuFlagsBlock(false,true,false,false,purpose),"Cursor-only overlays still block gameplay controls and the passive HUD");
    for(const auto purpose:{MenuPurpose::input,MenuPurpose::hud,MenuPurpose::editor,MenuPurpose::capture}) {
        check(!menuFlagsBlock(false,false,false,false,purpose),"Passive overlay does not take over an interface");
        check(menuFlagsBlock(true,false,false,false,purpose),"Menu-context owner blocks with no cursor");
        check(menuFlagsBlock(false,false,true,false,purpose),"Paused interface blocks with no cursor");
        check(menuFlagsBlock(false,false,false,true,purpose),"An unpaused modal menu owns input with keyboard or gamepad");
        check(menuFlagsBlock(true,true,false,false,purpose),"Cursor exception cannot override menu-context ownership");
        check(menuFlagsBlock(false,true,true,false,purpose),"Cursor exception cannot override a paused interface");
    }
    check(menuFlagsBlock(false,true,false,true,MenuPurpose::editor)&&menuFlagsBlock(false,false,false,true,MenuPurpose::editor),"Modal interfaces always dismiss the editor");
    for(const auto purpose:{MenuPurpose::input,MenuPurpose::hud,MenuPurpose::editor,MenuPurpose::capture})
        check(gameplayMenuOpen([](auto name){return name=="CustomMenu";},purpose,true),"UIExtensions CustomMenu blocks Quickslot even when its flags are cleared and QuickLoot input is enabled");
    for(const auto inventory:{"InventoryMenu","MagicMenu"}) {
        check(!gameplayMenuOpen([&](auto name){return name==inventory;},MenuPurpose::capture),"Inventory and Magic menus permit capturing their selected item");
        check(ignoreMenu(inventory,MenuPurpose::capture),"Capture exempts its inventory or magic menu from stack flag checks");
        for(const auto purpose:{MenuPurpose::input,MenuPurpose::hud,MenuPurpose::editor})
            check(!ignoreMenu(inventory,purpose),"The inventory capture exception is local to capture");
        for(const auto foreground:gameplayBlockingMenus)if(foreground!="InventoryMenu"&&foreground!="MagicMenu")
            check(gameplayMenuOpen([&](auto name){return name==inventory||name==foreground;},MenuPurpose::capture,true),"A foreground menu blocks inventory capture even with QuickLoot enabled");
    }
    for(const auto menu:gameplayBlockingMenus) {
        const auto blocked=gameplayMenuOpen([&](auto name){return name==menu;});
        check(blocked,"Interactive game menus block Quickslot even without pause or context flags");
        check(gameplayMenuOpen([&](auto name){return name==menu;},MenuPurpose::editor),"Editor still yields to named game interfaces including QuickLoot without flags");
        check(gameplayMenuOpen([&](auto name){return name==menu;},MenuPurpose::hud)==(menu!="LootMenu"),"Only QuickLoot leaves the passive HUD visible");
        check(gameplayMenuOpen([&](auto name){return name==menu||name=="LootMenu";},MenuPurpose::hud)==(menu!="LootMenu"),"QuickLoot cannot override another menu hiding the HUD");
        ControllerInput navigation;Options menuOptions;menuOptions.groupCount=4;
        for(const auto button:{PadButton::up,PadButton::right,PadButton::down,PadButton::left,PadButton::a,PadButton::b}) {
            const auto press=navigation.button(button,true,true,menuOptions,!blocked,false);
            const auto release=navigation.button(button,false,false,menuOptions,!blocked,false);
            check(!press.consumed&&!release.consumed&&press.action==PadAction::none&&release.action==PadAction::none,"Menu navigation reaches Skyrim without starting a group or consuming release");
        }
        check(navigation.button(PadButton::up,true,true,menuOptions,true,false).action==PadAction::press,"Bindings resume after closing the menu");
        check(navigation.button(PadButton::up,false,false,menuOptions,!blocked,true).action==PadAction::none,"Opening a menu during a held group prevents delayed activation");
    }
    check(!gameplayMenuOpen([](auto name){return name=="HUD Menu"||name=="Quickslot"||name=="QuickslotSelectionPause";}),"HUD and Quickslot's own overlays do not block gameplay");
    check(!gameplayMenuOpen([](auto name){return name=="OstimSceneMenu"||name=="OstimAlignMenu";}),"Idle OStim overlays leave gameplay available");
    for(const std::string_view foreground:{"LootMenu","CustomMenu","TestModalMenu","OstimSceneMenu"})
    for(const auto codes:{PadCodeSet::xinput,PadCodeSet::orbis})for(const auto mode:{GroupMode::browse,GroupMode::cycle}) {
        Options menuOptions;menuOptions.groupModes[0]=mode;
        ControllerInput navigation;Selection choosing;Group items;
        Entry sword;sword.form=1;items.add(sword);Entry potion;potion.kind=Kind::potion;potion.form=2;items.add(potion);
        bool menuOpen=true;
        const auto allowed=[&]{return !(menuOpen&&foreground=="OstimSceneMenu")&&!gameplayMenuOpen([&](auto name){return (menuOpen||foreground=="OstimSceneMenu")&&name==foreground;})&&
            !(menuOpen&&foreground=="TestModalMenu"&&menuFlagsBlock(false,false,false,true));};
        for(unsigned i=1;i<unsigned(PadButton::count);++i) {
            const auto button=padButton(padCodes(codes)[i],codes);
            const auto down=navigation.button(button,true,true,menuOptions,allowed(),false);
            const auto repeat=navigation.button(button,true,false,menuOptions,allowed(),false);
            const auto up=navigation.button(button,false,false,menuOptions,allowed(),false);
            check(!down.consumed&&!repeat.consumed&&!up.consumed&&down.action==PadAction::none&&repeat.action==PadAction::none&&up.action==PadAction::none,"Menus and player scenes receive controller presses, repeats and releases in both group modes");
        }
        const auto trigger=padButton(padCodes(codes)[unsigned(PadButton::up)],codes);
        navigation.button(trigger,true,true,menuOptions,allowed(),false);menuOpen=false;
        check(navigation.button(trigger,true,false,menuOptions,allowed(),false).action==PadAction::none,"Closing a menu cannot adopt a held navigation button");
        check(!navigation.button(trigger,false,false,menuOptions,allowed(),false).consumed,"A navigation release still passes through after the menu closes");
        check(navigation.button(trigger,true,true,menuOptions,allowed(),false).action==PadAction::press,"A fresh press resumes Quickslot after the menu closes");
        choosing.press(1,items);choosing.cycle(1,items);menuOpen=true;
        if(!allowed()){choosing.cancel();navigation.cancel();}
        const auto release=navigation.button(trigger,false,false,menuOptions,allowed(),false);
        check(release.consumed&&release.action==PadAction::none&&!navigation.owned(trigger)&&!choosing.release(items),"Opening a menu cancels the selection and balances its owned release");
        bool stickOwned=true;
        check(!captureGameplayStick(stickOwned,false,false,!allowed())&&!stickOwned,"The foreground menu receives a previously captured stick immediately");
        menuOpen=false;choosing.press(2,items);auto released=choosing.release(items);menuOpen=true;
        if(!allowed())choosing.cancel();
        check(!choosing.accepts(*released,items),"Opening a menu invalidates a released but queued use");
        auto cycled=choosing.requestCycle(items,0,0);choosing.cancel();
        check(!choosing.accepts(*cycled,items),"Opening a menu invalidates a queued tap-cycle equip");
    }
    for(bool keyboard:{false,true})for(bool controller:{false,true}) {
        Options policy;policy.quickLootKeyboard=keyboard;policy.quickLootController=controller;
        const auto restoredPolicy=decodeOptions(encodeOptions(policy));
        check(restoredPolicy.quickLootKeyboard==keyboard&&restoredPolicy.quickLootController==controller,"QuickLoot opt-ins persist independently");
        auto legacy=Json::parse(encodeOptions(policy));legacy.erase("quickLootKeyboard");legacy.erase("quickLootController");
        const auto migrated=decodeOptions(legacy.dump());
        check(!migrated.quickLootKeyboard&&!migrated.quickLootController,"Existing settings keep QuickLoot interception disabled");
        Options local;importAppearance(local,policy);
        check(!local.quickLootKeyboard&&!local.quickLootController,"Appearance imports preserve local QuickLoot policy");
        for(bool sourceController:{false,true}) {
            const bool enabled=sourceController?policy.quickLootController:policy.quickLootKeyboard;
            const auto blocked=[&](std::string_view extra={}) {return gameplayMenuOpen([&](auto name){return name==quickLootMenuName||name==extra;},MenuPurpose::input,enabled);};
            check(blocked()!=enabled,"Keyboard and controller use their own QuickLoot opt-in");
            for(const auto menu:gameplayBlockingMenus)if(menu!=quickLootMenuName)check(blocked(menu),"Opt-in cannot override another menu");
            check(gameplayMenuOpen([](auto name){return name==quickLootMenuName;},MenuPurpose::editor,enabled),"QuickLoot still blocks opening the editor");
            Selection choosing;Group items;Entry item;item.form=17;items.add(item);
            choosing.press(1,items);if(blocked())choosing.cancel();
            auto released=choosing.release(items);
            check(bool(released)==enabled,"Opening QuickLoot keeps only opted-in held selections");
            auto queued=choosing.requestCycle(items,0,0);queued->controller=sourceController;
            // Hovering with the other device must not change this queued action's origin.
            const bool queueAllowed=queued->controller?policy.quickLootController:policy.quickLootKeyboard;
            if(!queueAllowed)choosing.cancel();
            check(choosing.accepts(*queued,items)==enabled,"Released and cycle commits retain their original device policy");
            bool stickOwned=false;
            check(captureGameplayStick(stickOwned,false,enabled,blocked())==enabled,"QuickLoot receives blocked navigation and never shares captured stick input");
            if(enabled)check(captureGameplayStick(stickOwned,true,false,false)&&!stickOwned,"An opted-in selection balances stick capture on neutral");
        }
        ControllerInput navigation;
        const bool allowed=!gameplayMenuOpen([](auto name){return name==quickLootMenuName;},MenuPurpose::input,controller);
        const auto press=navigation.button(PadButton::up,true,true,policy,allowed,false);
        const auto release=navigation.button(PadButton::up,false,false,policy,allowed,controller);
        check(press.consumed==controller&&release.consumed==controller&&
            press.action==(controller?PadAction::press:PadAction::none)&&release.action==(controller?PadAction::release:PadAction::none),"Opted-in controller group presses and releases are exclusively owned");
    }
    {
        bool owned=false;
        check(captureGameplayStick(owned,false,true,false)&&owned,"Browsing captures a deflected stick");
        check(!captureGameplayStick(owned,false,false,true)&&!owned,"An open menu immediately receives a previously captured stick");
        check(!captureGameplayStick(owned,false,false,false),"Closing a menu cannot resume old stick capture");
        check(captureGameplayStick(owned,false,true,false),"New browsing can capture the stick again");
        check(captureGameplayStick(owned,false,false,false)&&owned,"Normal selector release still prevents accidental movement");
        check(captureGameplayStick(owned,true,false,false)&&!owned&&!captureGameplayStick(owned,false,false,false),"Centering restores normal gameplay stick input");
    }
    pad.button(PadButton::up,true,true,o,true,false);
    std::array<bool,unsigned(PadButton::count)> physical{};
    check(pad.synchronize(physical)&&!pad.active(),"A lost release cancels rather than activates");
    check(pad.button(PadButton::up,true,true,o,true,false).action==PadAction::press,"Fresh input works after focus recovery");pad.reset();
    check(!pad.editorButton(PadButton::a,false,false),"Editor preserves release of a press made before it opened");
    check(pad.editorButton(PadButton::a,true,true),"Editor captures its own press");
    check(pad.button(PadButton::a,false,false,o,true,false).consumed,"Editor-owned release remains captured after closing");
    pad.button(PadButton::up,true,true,o,true,false);
    check(pad.button(PadButton::start,true,true,o,true,false).action==PadAction::editor,"D-pad Up plus Start opens editor");pad.reset();
    check(pad.stick(.2f,0,0,.45f)==0,"Deadzone rejects drift");
    check(pad.stick(.8f,0,0,.45f)==1&&pad.stick(.8f,0,.2,.45f)==0&&pad.stick(.8f,0,.36,.45f)==1,"Stick repeat uses wall time with initial delay");
    check(pad.stick(-.8f,0,.4,.45f)==-1,"Changing stick direction responds immediately");
    check(pad.stick(0,0,.5,.45f)==0&&pad.stick(0,.8f,.6,.45f)==-1,"Center resets and upward direction browses backward");
    pad.reset();o.controllerEnabled=false;check(!pad.button(PadButton::up,true,true,o,true,false).consumed,"Controller gameplay interception can be disabled");o.controllerEnabled=true;
    {
        Options hands;ControllerInput input;check(useBumpersForHands(hands),"Bumper hand preset accepts default controls");
        check(hands.controllerActions[unsigned(ControllerAction::previous)].button==PadButton::none&&hands.controllerActions[unsigned(ControllerAction::next)].button==PadButton::none,"Bumper hand preset removes conflicting navigation bindings");
        Group items;Entry sword;sword.form=1;items.add(sword);Selection browse;
        for(const auto button:{PadButton::lb,PadButton::rb}) {
            input.reset();input.button(PadButton::up,true,true,hands,true,false);browse.press(0,items);
            const auto result=input.button(button,true,true,hands,true,true);
            const auto hand=button==PadButton::lb?Hand::left:Hand::right;
            check(result.consumed&&result.action==(hand==Hand::left?PadAction::leftHand:PadAction::rightHand),"Bumpers request their assigned hand rather than browsing");
            const auto commit=releaseToHand(browse,items,0,hand,true);input.cancel();
            check(commit&&input.button(PadButton::up,false,false,hands,true,false).action==PadAction::none&&!browse.release(items),"Completing a hand choice suppresses the held group release");
            check(input.button(button,false,false,hands,true,false).consumed,"Hand-button releases remain balanced after selection closes");
        }
        Options occupied;occupied.controllerGroups[0]={PadButton::up,PadButton::lb};const auto before=encodeOptions(occupied);
        check(!useBumpersForHands(occupied)&&encodeOptions(occupied)==before,"Preset refuses to overwrite group or modifier conflicts");
        input.reset();check(input.button(PadButton::rb,true,true,hands,true,false).action==PadAction::none,"Hand buttons remain normal gameplay input outside selection");
        input.reset();input.button(PadButton::up,true,true,hands,true,false);
        check(input.button(PadButton::rb,true,true,hands,false,true).action==PadAction::none,"Menus prevent new hand actions");
        input.reset();check(input.button(PadButton::rb,true,true,hands,true,true,true).action==PadAction::none,"Desktop hand bindings do not change VR controls");
    }
    const auto encoded=encodeOptions(o);const auto restored=decodeOptions(encoded);
    check(encodeOptions(restored)==encoded&&restored.controllerGroups[4]==o.controllerGroups[4],"Bindings and behavior round-trip in settings");
    o.controllerGroups[0]={PadButton(999),PadButton::a};o.controllerGroups[1]=o.controllerEditor;o.shoutCharge=99;o.controllerDeadzone=-1;sanitizeOptions(o);
    check(o.controllerGroups[0].button==PadButton::none&&o.controllerGroups[1].button==PadButton::none&&o.shoutCharge==2.5f&&o.controllerDeadzone==.2f,"Invalid or duplicate controller bindings cannot steal inputs");
    Options target;target.voiceHotkey=false;target.controllerEnabled=false;target.controllerGroups[0]={PadButton::y,PadButton::lb};
    const auto saved=target;importAppearance(target,restored);
    check(target.controllerGroups==saved.controllerGroups&&target.voiceHotkey==saved.voiceHotkey&&target.controllerEnabled==saved.controllerEnabled,"Appearance import preserves play controls");
    Arsenal arsenal;const auto preset=exportPreset(arsenal,restored,false,[](std::uint32_t){return Json{};});
    const auto imported=importPreset(preset,arsenal,target,false,[](const std::string&,std::uint32_t){return 0u;});
    check(imported.options.controllerGroups==restored.controllerGroups&&imported.options.voiceHotkey,"Complete preset restores controller and voice controls");
    for(const auto kind:{Kind::power,Kind::lesserPower,Kind::shout}) {
        Entry e;e.kind=kind;Selection::Commit c{0,0,0};Options controls;
        check(activatesVoice(e,c,controls),"Power and shout taps activate");c.browsed=true;
        check(!activatesVoice(e,c,controls),"Browsing voice entries selects by default");c.browsed=false;c.cycling=true;
        check(!activatesVoice(e,c,controls),"Cycling never casts");c.cycling=false;controls.voiceHotkey=false;
        check(!activatesVoice(e,c,controls),"Select-only preference disables hotkey casting");
    }
    VoiceAction voice;check(voice.begin(1,1.2f)&&!voice.begin(1.1f,1.2f),"Repeated requests cannot overlap an active voice gesture");
    check(voice.tick(1.5)==VoiceAction::Step::held&&voice.tick(2.3)==VoiceAction::Step::release&&voice.tick(2.4)==VoiceAction::Step::idle,"A voice request emits one release without retry");
    check(voice.cancel()&&!voice.active()&&voice.tick(3)==VoiceAction::Step::idle,"Rejected release can still cancel the native cast");
    voice.begin(4,1.2f);voice.cancel();check(voice.tick(6)==VoiceAction::Step::idle,"Cancellation prevents delayed release into another power");
    check(keyGlyphName(padGlyph(PadButton::rb,0))=="360_RB"&&keyGlyphName(padGlyph(PadButton::a,1))=="PS3_A","Controller glyphs use installed icon-pack filenames");
    check(padLabel({PadButton::a,PadButton::lb},1)=="L1 + Cross","PlayStation chords have matching text fallbacks");
    std::cout<<"PASS: "<<checks<<" controller and voice checks.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

#include "Controller.h"
#include "GameplayMenus.h"
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
    for(const auto purpose:{MenuPurpose::input,MenuPurpose::hud})
        check(menuFlagsBlock(false,true,false,false,purpose),"Cursor-only overlays still block gameplay controls and the passive HUD");
    for(const auto purpose:{MenuPurpose::input,MenuPurpose::hud,MenuPurpose::editor}) {
        check(!menuFlagsBlock(false,false,false,false,purpose),"Passive overlay does not take over an interface");
        check(menuFlagsBlock(true,false,false,false,purpose),"Menu-context owner blocks with no cursor");
        check(menuFlagsBlock(false,false,true,false,purpose),"Paused interface blocks with no cursor");
        check(menuFlagsBlock(true,true,false,false,purpose),"Cursor exception cannot override menu-context ownership");
        check(menuFlagsBlock(false,true,true,false,purpose),"Cursor exception cannot override a paused interface");
    }
    check(menuFlagsBlock(false,true,false,true,MenuPurpose::editor)&&menuFlagsBlock(false,false,false,true,MenuPurpose::editor),"Modal interfaces always dismiss the editor");
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
    for(const auto codes:{PadCodeSet::xinput,PadCodeSet::orbis})for(const auto mode:{GroupMode::browse,GroupMode::cycle}) {
        Options lootOptions;lootOptions.groupModes[0]=mode;
        ControllerInput navigation;Selection choosing;Group items;
        Entry sword;sword.form=1;items.add(sword);Entry potion;potion.kind=Kind::potion;potion.form=2;items.add(potion);
        bool lootOpen=true;
        const auto allowed=[&]{return !gameplayMenuOpen([&](auto name){return lootOpen&&name=="LootMenu";});};
        for(unsigned i=1;i<unsigned(PadButton::count);++i) {
            const auto button=padButton(padCodes(codes)[i],codes);
            const auto down=navigation.button(button,true,true,lootOptions,allowed(),false);
            const auto repeat=navigation.button(button,true,false,lootOptions,allowed(),false);
            const auto up=navigation.button(button,false,false,lootOptions,allowed(),false);
            check(!down.consumed&&!repeat.consumed&&!up.consumed&&down.action==PadAction::none&&repeat.action==PadAction::none&&up.action==PadAction::none,"QuickLoot receives controller presses, repeats and releases in both group modes");
        }
        const auto trigger=padButton(padCodes(codes)[unsigned(PadButton::up)],codes);
        navigation.button(trigger,true,true,lootOptions,allowed(),false);lootOpen=false;
        check(navigation.button(trigger,true,false,lootOptions,allowed(),false).action==PadAction::none,"Closing QuickLoot cannot adopt a held navigation button");
        check(!navigation.button(trigger,false,false,lootOptions,allowed(),false).consumed,"A navigation release still passes through after QuickLoot closes");
        check(navigation.button(trigger,true,true,lootOptions,allowed(),false).action==PadAction::press,"A fresh press resumes Quickslot after QuickLoot closes");
        choosing.press(1,items);choosing.cycle(1,items);lootOpen=true;choosing.cancel();navigation.cancel();
        check(navigation.button(trigger,false,false,lootOptions,allowed(),false).action==PadAction::none&&!choosing.release(items),"QuickLoot opening cancels a held selection without equipment or consumption on release");
        lootOpen=false;choosing.press(2,items);auto released=choosing.release(items);lootOpen=true;choosing.cancel();
        check(!choosing.accepts(*released,items),"QuickLoot opening invalidates a released but queued use");
        auto cycled=choosing.requestCycle(items,0,0);choosing.cancel();
        check(!choosing.accepts(*cycled,items),"QuickLoot opening invalidates a queued tap-cycle equip");
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

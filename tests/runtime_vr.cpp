#include "RuntimePolicy.h"
#include "Controller.h"
#include "Presets.h"
#include "VRFocus.h"
#include "KeyGlyphs.h"
#include "CallSite.h"
#include <iostream>
#include <source_location>
#ifdef _WIN32
#include "PlayerInput.h"
#endif
using namespace quickslot;
unsigned checks=0;
void check(bool pass,const char* message,std::source_location at=std::source_location::current()) {
    if(!pass)throw std::runtime_error(std::string(message)+" at "+std::to_string(at.line()));++checks;
}
int main()try {
#ifdef _WIN32
    struct PlayerLayout {REL::Version version;std::size_t flags;};
    for(const auto& layout:std::array<PlayerLayout,14>{{
        {{1,5,97,0},0xBD8},{{1,6,317,0},0xBD8},{{1,6,318,0},0xBD8},{{1,6,353,0},0xBD8},{{1,6,628,0},0xBD8},
        {{1,6,629,0},0xBE0},{{1,6,640,0},0xBE0},{{1,6,659,0},0xBE0},{{1,6,1130,0},0xBE0},{{1,6,1170,0},0xBE0},{{1,6,1179,0},0xBE0},
        {{1,7,99,0},0xBE8},{{1,7,104,0},0xBE8},{{1,4,15,0},0x12D0}
    }}) {
        check(REL::Module::mock(layout.version),"Runtime layout fixture initialized");
        alignas(RE::PlayerCharacter) std::array<std::uint8_t,0x1400> memory{};
        const auto* player=reinterpret_cast<const RE::PlayerCharacter*>(memory.data());
        check(reinterpret_cast<const std::uint8_t*>(&player->GetPlayerFlags())-memory.data()==layout.flags,"Player flags accessor selects this runtime's offset");
        for(const auto offset:{0xBD8,0xBE0,0xBE8,0x12D0})memory[offset+2]=0x08;
        memory[layout.flags+2]=0;
        check(!playerAIControlled(player),"Other runtime offsets cannot suppress player input");
        memory.fill(0);memory[layout.flags+2]=0xf7;
        check(!playerAIControlled(player),"Adjacent player flags do not claim scene input");
        memory[layout.flags+2]|=0x08;
        const auto before=memory;
        check(playerAIControlled(player),"SetAIDriven's package flag yields player input");
        check(memory==before,"Reading input ownership leaves engine state intact");
        memory[layout.flags+2]&=0xf7;
        check(!playerAIControlled(player),"Clearing AI control restores player input");
        REL::Module::reset();
    }
    check(!playerAIControlled(nullptr),"Missing player is safe to inspect during load transitions");
#endif
    for(const auto v:std::array<RuntimeVersion,13>{{{1,5,97},{1,6,317},{1,6,318},{1,6,353},{1,6,629},{1,6,640},{1,6,659},{1,6,1130},{1,6,1170},{1,6,1179},{1,7,99},{1,7,104},{1,4,15}}})
        check(v.supported(),"Release runtimes pass the loader policy");
    for(const auto v:std::array<RuntimeVersion,6>{{{1,5,80},{1,6,316},{1,4,14},{1,9,32},{1,8,0},{2,0,0}}})check(!v.supported(),"Unsupported engine families are rejected");
    for(const auto v:std::array<RuntimeVersion,4>{{{1,5,97},{1,4,15},{1,6,353},{1,6,628}}})check(v.extraDataSize()==0x18,"SE, VR and early AE inventory allocations fit their engine layouts");
    for(const auto v:std::array<RuntimeVersion,3>{{{1,6,629},{1,6,1170},{1,7,104}}})check(v.extraDataSize()==0x20,"Post-629 inventory allocation includes the vtable");
    check(RuntimeVersion{1,4,15}.pickupSlot()==0xce&&RuntimeVersion{1,5,97}.pickupSlot()==0xcc&&RuntimeVersion{1,7,104}.pickupSlot()==0xcc,"Pickup hook uses the correct virtual slot");
    check(RuntimeVersion{1,4,15}.inputCallOffset()==0x81&&RuntimeVersion{1,5,97}.inputCallOffset()==0x7b&&RuntimeVersion{1,7,104}.inputCallOffset()==0x7b,"VR input uses its own call offset");
    for(const bool leftHanded:{false,true})for(unsigned device=3;device<=8;++device) {
        const bool main=((device%2)!=0)!=leftHanded;
        check(vrMainHand(device,leftHanded)==main,"All three VR device families honor left-handed mode");
        check(vrButton(device,33,leftHanded)==(main?PadButton::rt:PadButton::lt),"VR trigger belongs to the correct hand");
        check(vrButton(device,2,leftHanded)==vrButton(device,34,leftHanded),"Oculus grip alias maps to the same button");
        check(vrButton(device,35,leftHanded)==vrButton(device,36,leftHanded),"Touchpad aliases map to the same button");
        check(vrButton(device,7,leftHanded)==(main?PadButton::a:PadButton::x),"A/X follows the logical hand");
        check(vrButton(device,1,leftHanded)==(main?PadButton::b:PadButton::y),"B/Y follows the logical hand");
        check(vrButton(device,32,leftHanded)==(main?PadButton::rs:PadButton::ls),"Stick click follows the logical hand");
        check(vrButton(device,999,leftHanded)==PadButton::none,"Unknown VR buttons do not activate a group");
    }
    for(unsigned device:{0u,1u,2u,9u,255u})check(!vrDevice(device)&&vrButton(device,7)==PadButton::none,"Desktop devices cannot alias VR buttons");
    const auto grip2=std::uint64_t(1)<<2,grip34=std::uint64_t(1)<<34,trigger=std::uint64_t(1)<<33;
    check(vrPhysicalState(grip2|grip34,0)[unsigned(PadButton::rb)]&&vrPhysicalState(grip34,0)[unsigned(PadButton::rb)],"Releasing one grip alias leaves the physical grip held");
    for(const bool leftHanded:{false,true})for(unsigned group=0;group<4;++group) {
        Options o;o.groupCount=4;ControllerInput pad;Selection select;Group entries;
        Entry e;e.form=1;entries.add(e);e.form=2;e.kind=Kind::potion;entries.add(e);entries.selected=0;
        const auto b=o.vrGroups[group];
        check(pad.button(b.button,true,true,o,true,false,true).action==PadAction::none,"VR group cannot fire before its modifier");pad.reset();
        check(!pad.button(b.modifier,true,true,o,true,false,true).consumed,"Modifier alone retains its original game behavior");
        const auto start=pad.button(b.button,true,true,o,true,false,true);
        check(start.consumed&&start.action==PadAction::press&&start.group==group,"VR chord activates exactly its configured group");select.press(1,entries,group);
        check(pad.stick(.8f,0,1,.45f)==1&&pad.stick(.8f,0,1.2,.45f)==0&&pad.stick(.8f,0,1.4,.45f)==1,"Held VR axis repeats at controlled intervals");
        select.cycle(1,entries);
        check(pad.button(b.button,false,false,o,true,true,true).action==PadAction::release,"VR release commits the selected group");
        const auto commit=select.release(entries,group);
        check(commit&&commit->browsed&&commit->index==1&&selectsConsumable(entries.entries[1],*commit,o),"VR browsing selects a potion without using it");
        check(pad.button(b.button,false,false,o,true,false,true).action==PadAction::none,"A duplicate release never activates twice");
        pad.reset();pad.button(b.modifier,true,true,o,true,false,true);pad.button(b.button,true,true,o,true,false,true);
        check(pad.button(b.modifier,false,false,o,true,true,true).action==PadAction::cancel&&!pad.active(),"Releasing the VR modifier cancels rather than commits");
        check(pad.button(b.button,false,false,o,true,false,true).action==PadAction::none,"Late group release after cancellation cannot equip/use");
    }
    Options o;ControllerInput pad;
    pad.button(PadButton::lb,true,true,o,true,false,true);pad.button(PadButton::rt,true,true,o,true,false,true);
    check(!pad.synchronize(vrPhysicalState(trigger,grip34))&&pad.active(),"Physical polling retains a fresh VR chord");
    check(pad.synchronize(vrPhysicalState(trigger,0))&&!pad.active(),"Missing grip release cancels the selection");
    pad.reset();pad.button(PadButton::lb,true,true,o,true,false,true);pad.button(PadButton::rt,true,true,o,true,false,true);
    check(pad.synchronize({})&&!pad.active(),"Controller disconnect cancels without auto-committing");
    pad.reset();pad.button(o.vrEditor.modifier,true,true,o,true,false,true);
    check(pad.button(o.vrEditor.button,true,true,o,true,false,true).action==PadAction::editor,"VR editor shortcut is separate from desktop Start");
    o.controllerGroups[0]={PadButton::y,PadButton::lt};o.vrGroups[0]={PadButton::start,PadButton::rb};o.vrStick=0;
    const auto saved=decodeOptions(encodeOptions(o));
    check(saved.controllerGroups==o.controllerGroups&&saved.vrGroups==o.vrGroups&&saved.vrEditor==o.vrEditor&&saved.vrStick==0,"Desktop and VR bindings round-trip independently");
    Options imported;importAppearance(imported,o);
    check(imported.vrGroups==Options{}.vrGroups&&imported.vrEditor==Options{}.vrEditor&&imported.vrStick==1,"Appearance import preserves local VR controls");
    o.vrGroups[1]=o.vrGroups[0];o.vrGroups[2]={PadButton::up};o.vrStick=999;sanitizeOptions(o);
    check(o.vrGroups[1].button==PadButton::none&&o.vrGroups[2].button==PadButton::none&&o.vrStick==2,"Invalid/duplicate VR bindings are removed safely");
    check(keyLabel(padGlyph(PadButton::rt,2))=="Main trigger"&&keyGlyphName(padGlyph(PadButton::rt,2)).empty(),"VR captions do not borrow incorrect Xbox/PlayStation artwork");
    VRFocus focus;focus.update(false);check(!focus.request(false),"No editor request while hidden");
    focus.desired=true;check(focus.request(false)==true,"Helper focus requests the editor");
    for(unsigned i=0;i<10;++i){focus.update(false);check(focus.desired&&!focus.request(false),"Pending open survives delayed game tasks without duplicate requests");}
    focus.update(true,true);check(focus.desired&&!focus.request(true),"Game acknowledgement settles the open transition");
    focus.desired=false;check(focus.request(true)==false,"Helper focus loss requests editor close");
    for(unsigned i=0;i<10;++i){focus.update(true);check(!focus.desired&&!focus.request(true),"Pending close does not reopen while the game task is delayed");}
    focus.update(false,false);check(!focus.desired&&!focus.request(false),"Close acknowledgement clears pending transition");
    focus.desired=true;focus.request(false);focus.update(false,false);check(!focus.desired&&!focus.request(false),"Rejected editor open relinquishes helper focus");
    focus.update(true);check(focus.desired&&!focus.request(true),"Keyboard/gamepad editor open also requests helper focus");
    focus.update(false);check(!focus.desired&&!focus.request(false),"Normal editor close relinquishes helper focus");
    for(const bool hudFirst:{false,true}) {
        VRPanelFocus panel;bool wanted=false;
        check(!panel.update(wanted,false)&&!wanted,"Idle panel does not request focus");
        wanted=true;check(panel.update(wanted,false)==true&&wanted,"Local open sends one helper request");
        check(!panel.update(wanted,!hudFirst)&&wanted,"Delayed SDK focus cannot cancel a local open");
        check(!panel.update(wanted,true)&&wanted,"Both callback orders acknowledge the open");
        wanted=false;check(panel.update(wanted,true)==false&&!wanted,"Local close releases focus once");
        check(!panel.update(wanted,hudFirst)&&!wanted,"Stale helper grant cannot reopen a closed editor");
        check(!panel.update(wanted,false)&&!wanted,"Both callback orders acknowledge the close");
        check(!panel.update(wanted,true)&&wanted,"Helper selection can open the editor");
        check(!panel.update(wanted,false)&&!wanted,"Switching helper overlays closes without reclaiming focus");
        wanted=true;panel.update(wanted,false);panel.update(wanted,false);
        check(!panel.update(wanted,false)&&!wanted,"A revoked focus request settles closed instead of retrying");
        wanted=true;panel.update(wanted,false);
        wanted=false;check(panel.update(wanted,true)==false,"Rejected game open immediately releases helper focus");
        panel.update(wanted,true);panel.update(wanted,false);
        check(!wanted&&!panel.update(wanted,false),"Rejected open remains closed after stale focus drains");
    }
    std::cout<<"PASS: "<<checks<<" runtime, VR input, persistence and focus checks. No headset or engine simulation claimed.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

#include "CastModel.h"
#include "Controller.h"
#include "GroupSetup.h"
#include "Settings.h"
#include "Storage.h"
#include <iostream>
#include <source_location>

using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* reason,std::source_location location=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(reason)+" at line "+std::to_string(location.line()));
    ++checks;
}
Entry spell(std::uint32_t form,const char* name="Firebolt") {
    Entry e;e.form=form;e.kind=Kind::spell;e.spell=true;e.name=name;return e;
}
Entry scroll(std::uint32_t form) {
    Entry e;e.form=form;e.kind=Kind::scroll;e.name="Scroll of Fireball";return e;
}
Entry sword() {
    Entry e;e.form=0x1234;e.owner=0x14;e.unique=1;e.kind=Kind::sword;e.name="Sword";return e;
}
Selection::Commit tap(std::size_t group,std::size_t index=0){return Selection::Commit{index,0,0,group};}
Selection::Commit browsed(std::size_t group,std::size_t index=0){auto c=tap(group,index);c.browsed=true;return c;}
int main()try {
    // Timing of one cast.
    {
        CastSequence cast;cast.begin(1.f,false);
        check(cast.active()&&!cast.channeling()&&cast.progress()==0,"A fire-and-forget cast starts charging");
        auto step=cast.tick(.5f);check(!step.release&&!step.fire&&std::abs(cast.progress()-.5f)<.001f,"Halfway through the charge nothing is released yet");
        step=cast.tick(.39f);check(step.release&&!step.fire,"The release clip starts just before the spell leaves the hand");
        step=cast.tick(.05f);check(!step.release&&!step.fire,"The release clip starts once");
        step=cast.tick(.1f);check(step.fire,"The spell fires at the end of its charge time");
        cast.fired();check(!cast.active(),"A fire-and-forget cast is over once it fires");
        cast.begin(0,false);step=cast.tick(.3f);
        check(step.release&&step.fire,"A spell without a charge time still plays the shortest charge, then releases and fires together");
        cast.begin(-5,false);check(cast.progress()==0&&cast.active(),"A negative charge time counts as none");
        cast.begin(std::nanf(""),false);step=cast.tick(std::nanf(""));check(cast.active()&&!step.fire,"An invalid charge time or frame time does not fire at once");
    }
    {
        CastSequence cast;cast.begin(0,true,true);
        auto step=cast.tick(castTiming::concentrationWindup);check(step.fire&&!step.release,"A concentration spell starts after the short hand raise");
        cast.fired();check(cast.channeling()&&cast.active()&&cast.progress()==1,"A concentration spell runs after it starts");
        step=cast.tick(.3f);check(!step.restart&&!step.stop,"A running concentration spell waits for its next loop");
        step=cast.tick(.25f);check(step.restart,"Its casting clip starts again every half second");
        cast.letGo();step=cast.tick(.016f);check(step.stop&&!cast.active(),"Letting go ends a running concentration spell");
        cast.begin(0,true,true);cast.letGo();step=cast.tick(.016f);
        check(step.stop&&!step.fire&&!cast.active(),"Letting go during the hand raise casts nothing");
        cast.begin(0,true,false);step=cast.tick(.016f);check(step.stop,"A concentration spell whose key is already up casts nothing");
    }
    {
        CastLinger linger;check(linger.wait()==0,"Nothing plays before the first cast");
        linger.linger(.8f,true);check(linger.lingering()&&linger.wait()>.8f,"A new cast waits for the release clip and the settle time");
        check(linger.tick(.5f)==CastLinger::Tick::none,"The release clip still plays");
        check(linger.tick(.5f)==CastLinger::Tick::endShout,"After the release clip the shout state ends");
        check(!linger.lingering()&&linger.wait()>0,"The graph settles before the next cast");
        linger.tick(castTiming::settle);check(linger.wait()==0,"After settling a new cast may start");
        linger.linger(.4f,false);check(linger.tick(1)==CastLinger::Tick::clear&&linger.wait()==0,"A stopped cast needs no settle time");
        linger.linger(.4f,true);linger.restart();check(!linger.lingering()&&linger.tick(1)==CastLinger::Tick::none,"A new cast takes over a lingering animation");
    }
    {
        check(castAnimationFor(false,false,false)==CastAnimation::aimed&&castAnimationFor(true,false,false)==CastAnimation::self,"One-handed spells aim or target the caster");
        check(castAnimationFor(false,true,false)==CastAnimation::aimedConcentration&&castAnimationFor(true,true,false)==CastAnimation::selfConcentration,"Concentration spells have their own clips");
        check(castAnimationFor(false,false,true)==CastAnimation::ritual&&castAnimationFor(true,false,true)==CastAnimation::ritual,"Two-handed spells use the ritual clips");
        check(castAnimationFor(false,true,true)==CastAnimation::dualConcentration&&castAnimationFor(true,true,true)==CastAnimation::dualSelfConcentration,"Two-handed concentration spells use both hands");
        check(int(CastAnimation::ritual)==8&&int(CastSide::left)==2,"Condition values match the animation pack");
        check(concentrationAnimation(CastAnimation::selfConcentration)&&!concentrationAnimation(CastAnimation::ritual),"Concentration clips are recognized");
        check(releaseSeconds(CastAnimation::aimed)>releaseSeconds(CastAnimation::aimedConcentration),"A concentration cast ends with a short blend");
    }

    // What a group does with its spells.
    castingInstalled()=false;
    {
        Options o;o.castStyles[1]=CastStyle::quick;o.castStyles[2]=CastStyle::oblivion;o.groupModes[1]=GroupMode::cycle;
        check(castStyle(o,1)==CastStyle::equip&&selectionAction(spell(1),tap(1),o)==SelectionAction::equip,"Without the casting animations a casting group equips its spells");
        check(groupMode(o,Arsenal{},1)==GroupMode::cycle,"Without the casting animations a group keeps its tap behavior");
        Arsenal a;a.groups[2].add(spell(2));a.readySpellGroup=2;check(!readySpell(a,o),"Without the casting animations there is no ready spell");
    }
    castingInstalled()=true;
    {
        Options o;o.castStyles[1]=CastStyle::quick;o.castStyles[2]=CastStyle::oblivion;o.groupModes[1]=GroupMode::cycle;o.groupModes[2]=GroupMode::cycleBrowse;
        check(groupMode(o,Arsenal{},1)==GroupMode::browse&&groupMode(o,Arsenal{},2)==GroupMode::browse,"A casting group taps on its selected item and holds to browse");
        check(selectionAction(spell(1),tap(1),o)==SelectionAction::cast,"A tap in a quick-cast group casts the spell");
        check(selectionAction(scroll(3),tap(1),o)==SelectionAction::cast,"A tap in a quick-cast group reads a scroll");
        check(selectionAction(spell(1),browsed(1),o)==SelectionAction::select,"Browsing a quick-cast group picks a spell without casting it");
        auto hand=tap(1);hand.hand=Hand::left;
        check(selectionAction(spell(1),hand,o)==SelectionAction::equip,"A hand button still equips the spell");
        check(selectionAction(sword(),tap(1),o)==SelectionAction::equip,"Weapons in a casting group still equip");
        Entry potion;potion.kind=Kind::potion;potion.form=7;
        check(selectionAction(potion,tap(1),o)==SelectionAction::use,"Potions in a casting group are still used on a tap");
        Entry power;power.kind=Kind::power;power.spell=true;power.form=8;
        check(selectionAction(power,tap(1),o)==SelectionAction::use,"Powers in a casting group are still activated on a tap");
        check(selectionAction(spell(2),tap(2),o)==SelectionAction::ready&&selectionAction(spell(2),browsed(2),o)==SelectionAction::ready,"An Oblivion-style group makes the spell ready on a tap or a release");
        check(selectionAction(spell(4),tap(0),o)==SelectionAction::equip,"Other groups keep equipping spells");
        check(castStyle(o,Arsenal::count)==CastStyle::equip,"A group past the last one has no casting style");
        o.castStyles[3]=CastStyle(9);check(castStyle(o,3)==CastStyle::equip,"An unknown casting style equips");

        Arsenal a;a.groups[2].add(sword());a.groups[2].add(spell(2,"Healing"));a.groups[2].selected=1;
        check(!readySpell(a,o),"No ready spell before an Oblivion-style group picked one");
        a.readySpellGroup=2;const auto* ready=readySpell(a,o);
        check(ready&&ready->name=="Healing","The ready spell is the picked spell of the Oblivion-style group used last");
        a.groups[2].selected=0;check(!readySpell(a,o),"A weapon selected in that group is no ready spell");
        a.groups[2].selected=1;o.castStyles[2]=CastStyle::quick;check(!readySpell(a,o),"A group that stopped being Oblivion style has no ready spell");
        o.castStyles[2]=CastStyle::oblivion;o.groupCount=3;
        resetGroup(a,o,2);check(o.castStyles[2]==CastStyle::equip&&a.readySpellGroup==Arsenal::count,"Removing a group resets its casting style and ready spell");
    }
    // The Cast ready spell binding.
    {
        Options o;
        check(assignCastBinding(o,{46,0})&&castShortcutMatches(o,46,0)&&!castShortcutMatches(o,46,keyModifier::shift),"Cast ready spell takes a key with exact modifiers");
        check(!assignConsumableBinding(o,{46,0})&&!assignSwapBinding(o,{46,0}),"Other dedicated actions cannot take the cast key");
        check(assignConsumableBinding(o,{47,0})&&!assignCastBinding(o,{47,0}),"Cast ready spell cannot take another action's key");
        check(!assignCastBinding(o,{o.keys[0],0})&&!assignCastBinding(o,{o.editorKey,0})&&!assignCastBinding(o,{203,0}),"Cast ready spell cannot take a group, editor or browsing key");
        check(!assignGroupKey(o,0,46)&&!assignSelectorBinding(o,KeyboardAction::cancel,{46,0}),"Group and browsing keys cannot take the cast key");
        check(assignCastBinding(o,{})&&!o.castKey.key&&!o.castKey.modifiers,"Clearing the cast key clears its modifiers");
    }
    // Settings, the character's group setup, presets and saves keep casting.
    {
        Options o;o.castStyles={CastStyle::quick,CastStyle::equip,CastStyle::oblivion};assignCastBinding(o,{46,keyModifier::ctrl});
        o.controllerCast={PadButton::y,PadButton::lb};o.vrCast={PadButton::b,PadButton::rb};
        const auto decoded=decodeOptions(encodeOptions(o));
        check(decoded.castStyles==o.castStyles&&decoded.castKey==o.castKey&&decoded.controllerCast==o.controllerCast&&decoded.vrCast==o.vrCast,"Casting settings survive a reload");
        auto older=Json::parse(encodeOptions(o));for(const auto* key:{"castStyles","castKey","controllerCast","vrCast"})older.erase(key);
        const auto upgraded=decodeOptions(older.dump());
        check(upgraded.castStyles==Options{}.castStyles&&!upgraded.castKey.key&&upgraded.controllerCast==PadBinding{},"Settings from earlier versions start with no casting groups or binding");
        auto broken=o;broken.castStyles[4]=CastStyle(42);sanitizeOptions(broken);check(broken.castStyles[4]==CastStyle::equip,"An unknown casting style is reset");
        broken=o;broken.controllerCast=broken.controllerGroups[0];sanitizeOptions(broken);check(broken.controllerCast==PadBinding{},"A cast button that duplicates a group binding is cleared");
        broken=o;broken.controllerCast=broken.controllerEditor;sanitizeOptions(broken);check(broken.controllerCast==PadBinding{},"A cast button that duplicates the editor binding is cleared");

        const auto setup=encodeGroupSetup(o);const auto loaded=withGroupSetup(Options{},setup);
        check(loaded&&loaded->castStyles==o.castStyles,"Casting styles are saved with the character's groups");
        Options imported;copyGroupSetup(imported,o);check(imported.castStyles==o.castStyles,"Complete presets carry casting styles");
        Options controls;copyControls(controls,o);check(controls.castKey==o.castKey&&controls.controllerCast==o.controllerCast,"Control presets carry the cast binding");

        Arsenal a;a.groups[2].add(spell(2));a.readySpellGroup=2;
        const auto identity=[](auto oldID,auto& newID){newID=oldID;return true;};
        check(decodeArsenal(encodeArsenal(a),identity).readySpellGroup==2,"The ready spell's group is saved");
        check(decodeArsenal(encodeArsenal(Arsenal{}),identity).readySpellGroup==Arsenal::count,"Saves without a ready spell load without one");
    }
    // Controller: the cast button reports its press and release.
    {
        Options o;o.controllerCast={PadButton::y};ControllerInput pad;
        auto result=pad.button(PadButton::y,true,true,o,true,false);
        check(result.consumed&&result.action==PadAction::cast&&pad.castHeld(),"The cast button casts the ready spell and is held");
        result=pad.button(PadButton::y,false,false,o,true,false);
        check(result.consumed&&result.action==PadAction::castRelease&&!pad.castHeld(),"Letting go of the cast button is reported");
        o.controllerCast={PadButton::y,PadButton::lb};pad.button(PadButton::lb,true,true,o,true,false);
        result=pad.button(PadButton::y,true,true,o,true,false);check(result.action==PadAction::cast,"The cast binding can be a chord");
        pad.reset();check(!pad.castHeld(),"A controller reset lets go of the cast button");
    }
    std::cout<<checks<<" casting checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

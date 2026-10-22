#include "EditorModel.h"
#include "Settings.h"
#include "Layout.h"
#include "LiveEditor.h"
#include "ConsumableUse.h"
#include "SelectionTime.h"
#include "MenuCapture.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <random>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()) {
    if(!value)throw std::runtime_error(std::string(why)+" at line "+std::to_string(at.line()));
    ++checks;
}
Entry weapon(unsigned id,Hand hand=Hand::right){Entry e;e.form=0x1234;e.owner=0x14;e.unique=std::uint16_t(id);e.name="Sword";e.hand=hand;return e;}
int main(int argc,char** argv)try {
    if(argc!=2)return 2;
    const auto identity=[](auto before,auto& after){after=before;return true;};
    std::ifstream file(std::filesystem::path(argv[1])/"tests/fixtures/arsenal.json");const std::string bytes((std::istreambuf_iterator<char>(file)),{});
    auto a=decodeArsenal(bytes,identity);check(a.size()==23,"Current save preserves all 23 entries");
    auto before=Json::parse(bytes);auto after=Json::parse(encodeArsenal(a));
    for(auto& group:after["groups"])for(auto& entry:group["entries"])entry.erase("shortcut");
    for(unsigned g=0;g<8;++g){check(before["groups"][g]["entries"]==after["groups"][g]["entries"],"Every original entry field survives storage");check(before["groups"][g]["selected"]==after["groups"][g]["selected"],"Every remembered selection survives storage");}
    check(encodeArsenal(decodeArsenal(encodeArsenal(a),identity))==encodeArsenal(a),"Current save round-trips exactly");
    a.active=4;Options o;o.groupCount=5;const auto keys=o.keys;EditHistory history;history.begin(a,o);history.prepare(a,o);
    check(removeGroup(a,o,4)&&o.groupCount==4&&a.active!=4,"Active empty fifth group can be removed");history.finish(true,false,0);
    auto removedKeys=keys;removedKeys[4]=0;
    check(o.keys==removedKeys,"Removing a group frees only its own hotkey");check(history.undo(a,o)&&a.active==4&&o.groupCount==5&&o.keys==keys,"Undo restores active group, layout and hotkey");
    const auto populated=a.groups[2];history.prepare(a,o);check(removeGroup(a,o,2)&&a.groups[2].entries.empty()&&!groupVisible(o,2),"Any populated group can be removed");history.finish(true,false,1);
    removedKeys=keys;removedKeys[2]=0;
    check(o.keys==removedKeys&&o.groupOrder[2]==3,"Later groups compact their layout without changing their IDs or hotkeys");
    check(history.undo(a,o)&&encode(a.groups[2])==encode(populated),"Undo restores every removed consumable");
    check(reorderGroup(o,3,-1)&&o.groupOrder[2]==3&&o.keys==keys,"Reordering groups changes only display order");
    for(unsigned count=8;count>0;--count) {
        Options test;test.groupCount=count;
        for(unsigned layout=0;layout<6;++layout)for(unsigned shape=0;shape<4;++shape)for(bool names:{true,false}) {
            test.layout=layout;test.shape=shape;test.showLabels=names;const auto g=groupGeometry(test);
            check(g.tiles.size()==count&&std::isfinite(g.width)&&std::isfinite(g.height)&&g.width>0&&g.height>0,"All layouts support 1–8 finite tiles");
            for(const auto& p:g.tiles)check(std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0,"All group positions are finite and normalized");
        }
    }
    Options removed;Arsenal tiny;check(removeGroup(tiny,removed,1)&&removeGroup(tiny,removed,2)&&!removeGroup(tiny,removed,0),"Retain at least one group");
    check(addGroup(tiny,removed)&&removed.groupCount==2&&groupVisible(removed,tiny.active),"Add reuses a hidden group ID");
    removed.keys[0]=removed.keys[1]=3;sanitizeOptions(removed);check(removed.keys[0]==3&&removed.keys[1]==0,"A hidden group's hotkey can be reassigned without rejecting the user's binding");
    o.skipEmpty=true;o.selectionSpeed=.25f;o.menuCapture=false;
    check(encodeOptions(decodeOptions(encodeOptions(o)))==encodeOptions(o),"New behavior and group order persist");

    Entry potion;potion.kind=Kind::potion;potion.form=0x123;potion.name="Potion";Group group;group.add(potion);group.add(weapon(2));Selection select;Options controls;
    group.selected=0;select.press(1,group);auto tap=select.release(group);check(tap&&!tap->browsed&&!selectsConsumable(potion,*tap,controls),"Tap requests a single use");
    select.press(2,group);select.tick(2.2);auto held=select.release(group);check(held&&held->browsed&&selectsConsumable(potion,*held,controls),"A hold selects without spending a dose");
    select.press(3,group);select.cycle(1,group);select.cycle(-1,group);auto scrolled=select.release(group);check(scrolled&&selectsConsumable(potion,*scrolled,controls),"Even a fast scroll makes release selection-only");
    check(!selectsConsumable(weapon(1),*scrolled,controls),"Weapons still equip on release");
    select.press(4,group);select.cycleAvailable(1,group,{true,false});check(select.index()==0,"Skip empty wraps to available entry");
    select.cycleAvailable(-1,group,{false,false});check(select.index()==0&&select.browsing(),"All empty retains a valid shortcut without deleting it");
    select.cycleAvailable(1,group,{false,true});check(select.index()==1,"Restocked entry becomes scrollable");
    auto stale=select.release(group);group.add(weapon(3));check(!select.accepts(*stale,group),"An inventory/edit revision invalidates queued input");

    std::vector<PotionCandidate> bottles;
    for(unsigned i=0;i<3;++i){PotionCandidate c;c.entry=potion;c.entry.form=0x100+i;c.entry.name="Strength "+std::to_string(i);c.count=5;c.restore[0]=float(25u<<i);c.regeneration[0]=float(20+20*i);bottles.push_back(c);}
    auto rule=smartEntry(0,0,0);check(choosePotion(rule,bottles,60)->entry.form==0x100,"Lowest strength chooses weakest restoration");
    rule.policy=1;check(choosePotion(rule,bottles,60)->entry.form==0x102,"Highest strength chooses strongest restoration");
    rule.policy=2;for(const auto [deficit,expected]:std::vector<std::pair<float,unsigned>>{{0,0x100},{25,0x100},{26,0x101},{50,0x101},{51,0x102},{200,0x102}})
        check(choosePotion(rule,bottles,deficit)->entry.form==expected,"Best fit minimizes excess; strongest if deficit exceeds all bottles");
    rule.exclusions={{0x102,0}};check(choosePotion(rule,bottles,90)->entry.form==0x101,"Exclusion overrides best fit");
    bottles[1].count=0;check(choosePotion(rule,bottles,90)->entry.form==0x100,"Depletion chooses another bottle");bottles[1].count=4;
    check(choosePotion(rule,bottles,90)->entry.form==0x101,"Restocking returns the stronger bottle");
    rule.resource=1;check(!choosePotion(rule,bottles,90),"Never substitutes health for magicka");
    rule=smartEntry(0,1,1);bottles[0].restore[0]=999;check(choosePotion(rule,bottles,0)->entry.form==0x102,"Regeneration ranks separate effects, not restoration magnitude");
    bottles[1].entry.recipe=7;rule.exclusions={{bottles[1].entry.form,6}};rule.policy=0;bottles[0].count=0;
    check(choosePotion(rule,bottles,0)->entry.form==0x101,"A changed crafted recipe does not inherit an unrelated exclusion");
    bottles[1].regeneration[0]=NAN;check(choosePotion(rule,bottles,0)->entry.form==0x102,"Nonfinite effect strengths are ignored");
    bottles[1].regeneration[0]=60;
    check(choosePotion(rule,bottles,0)->entry.form==0x101,"Tied strengths use stable form ordering");
    for(unsigned seed=0;seed<100;++seed){std::mt19937 rng(seed);std::shuffle(bottles.begin(),bottles.end(),rng);check(choosePotion(rule,bottles,0)->entry.form==0x101,"Inventory iteration order never changes the chosen bottle");}
    Group smart;smart.add(rule);const auto mapped=decode(encode(smart),[](auto old,auto& id){id=old+0x1000000;return true;});
    check(mapped.entries[0].form==0&&!mapped.entries[0].unresolved&&mapped.entries[0].exclusions[0].form==0x1000101,"Smart rules have no fake form, while exclusions remap");
    check(decode(encode(smart),identity).entries[0].same(rule),"Smart rule and exclusions survive storage");

    auto right=weapon(4),left=weapon(5,Hand::left);Entry pair;pair.kind=Kind::combo;pair.name="Blades";pair.parts={right,left};
    check(validPair(right,left),"Distinct copies support dual wielding");left.unique=right.unique;check(!validPair(right,left),"One physical copy cannot occupy both hands");
    left=right;left.kind=Kind::spell;left.spell=true;left.hand=Hand::left;right=left;right.hand=Hand::right;check(validPair(right,left),"One known spell may fill both hands");
    right.twoHanded=true;check(!validPair(right,left),"A two-handed item cannot be part of a pair");
    right=weapon(1);right.kind=Kind::shield;check(!validPair(right,left),"Shields cannot be the right component");
    Group pairs;pairs.add(pair);check(decode(encode(pairs),identity).entries[0].same(pair),"Combination retains exact child identities");
    check(followIdentityChange(pairs,{0x1234,0x14,0xff000123,4,0})==1&&pairs.entries[0].parts[0].owner==0xff000123,"Drop/pickup identity events reach combination children");
    check(decode(encode(pairs),identity).entries[0].parts[0].unique==0,"A recovered zero unique ID remains valid");
    const auto missing=decode(encode(pairs),[](auto id,auto& out){out=id;return id!=0x1234;});
    check(missing.entries[0].parts[0].unresolved&&missing.entries[0].parts[1].unresolved,"Missing combo components remain explicit placeholders");
    check(decode(encode(missing),identity).entries[0].parts[0].unresolved,"A missing child never rebinds on another load");
    const auto invalid=[&](Json value){try{decode(value.dump(),identity);return false;}catch(...){return true;}};
    auto bad=Json::parse(encode(pairs));bad["entries"][0]["parts"][0]=encodeEntry(pair);check(invalid(bad),"Reject recursive combinations");
    bad=Json::parse(encode(smart));bad["entries"][0]["resource"]=3u;check(invalid(bad),"Reject invalid smart resource");
    bad=Json::parse(encode(smart));bad["entries"][0]["form"]=0x123u;check(invalid(bad),"Rules cannot masquerade as inventory items");
    bad=Json::parse(encode(pairs));bad["entries"][0]["parts"][1]=encodeEntry(potion);check(invalid(bad),"A combination never contains an auto-used consumable");
    Arsenal worst;for(auto& g:worst.groups)for(unsigned i=0;i<Group::limit;++i){auto e=smartEntry(i%3,0,i%3);e.name=std::string(512,'x');for(unsigned k=0;k<128;++k)e.exclusions.push_back({0xff001234+k+i*128,0xffffffffffffffffull});g.entries.push_back(e);}
    check(decodeArsenal(encodeArsenal(worst),identity).size()==128,"Maximum supported rule/exclusion payload reloads within bounds");

    ConsumableUse use;using R=ConsumableUse::Result;
    check(use.begin(0,4,false,true),"Animated use starts once");use.animation(.1,"AnimObjLoad","AnimObjectDrinkPotion");
    check(use.observe(.8,3,false)==R::decreased&&use.animating()&&use.active(),"Inventory decrement leaves animation guard active");
    check(!use.begin(1,3,false,true),"Tap during remaining animation sends no second native request");
    check(use.observe(2,3,false)==R::waiting,"Count cannot report a second use");use.animation(2.7,"DrinkPotionStop");
    check(!use.begin(2.71,3,false,true)&&use.begin(3,3,false,true),"Cleanup settling interval ends before next explicit use");
    use.animation(3.1,"AnimObjLoad","AnimObjectDrinkPotion");use.animation(3.2,"EnableBumper");
    check(use.observe(3.5,3,false)==R::unconfirmed&&!use.active(),"Rejected or interrupted animation releases guard without a ten-second blind lock");
    check(use.begin(4,3,false,true)&&use.observe(7,3,false)==R::unconfirmed,"UAP refusal without events expires promptly without retry");
    use.reset();use.animation(10,"AnimObjLoad","AnimObjectDrinkPotion");check(!use.begin(11,3,false,true),"Potions started outside Quickslot also block overlapping use");
    check(use.observe(30,3,false,.1f)==R::waiting&&use.animating(),"Slow motion does not expire a running animation early");
    use.observe(100,3,false,1,true);check(use.animating(),"A paused menu does not age an animation timeout");
    use.observe(107,3,false,1);check(!use.active(),"Lost animation stop has a bounded fallback");
    use.reset();check(use.begin(110,3,false,true)&&use.observe(110.8,2,false)==R::decreased&&use.animating(),"Lost start event still guards post-consumption animation");
    use.animation(112.7,"MTState");check(!use.active(),"Real cleanup clears the fallback guard");
    use.reset();check(use.begin(120,4,true)&&use.observe(121,4,true)==R::waiting,"Poison dialog stays independent of animation state");
    check(use.observe(140,4,false)==R::cancelled,"Cancel poison cannot spend a dose or retry");

    InventorySample live{3,{{0x123,1,true,0x14,1},{0x456,1,true,0x14,2}}};
    check(matchMenuCopy(live,{0x123}).valid&&matchMenuCopy(live,{0x123}).token==0x123,"Menu capture keeps the exact selected copy");
    check(matchMenuCopy(live,{}).valid&&matchMenuCopy(live,{}).plain,"A plain inventory row can materialize one existing copy");
    check(!matchMenuCopy(live,{0x789}).valid,"A stale menu token is rejected before dereferencing");
    check(!matchMenuCopy(live,{0x123,0x456}).valid,"A merged customized row cannot pick an arbitrary copy");
    live.count=2;check(!matchMenuCopy(live,{}).valid,"A stale plain row cannot bind a customized copy");
    live.stacks[0].count=2;check(!matchMenuCopy(live,{0x123}).valid,"A customized multi-copy stack requires explicit separation");
    live.count=0;check(!matchMenuCopy(live,{0x456}).valid,"An emptied inventory cannot be captured");

    SelectionTime speed;
    auto change=speed.update(true,.25f,1,1);check(change&&change->first==.25f,"Standalone slowdown starts once");
    check(!speed.update(true,.25f,.25f,.25f),"Held frames do not compound slowdown");
    change=speed.update(false,.25f,.25f,.25f);check(change&&change->first==1&&change->second==1,"Release restores prior speed");
    change=speed.update(true,.5f,.4f,.3f);check(change&&change->first==.2f&&change->second==.15f,"Pre-existing current and target speeds are preserved independently");
    change=speed.update(false,.5f,.2f,.15f);check(change&&change->first==.4f&&change->second==.3f,"Cancellation never forces vanilla speed");
    speed.update(true,.5f,1,1);check(!speed.update(true,.5f,.1f,.1f)&&!speed.update(false,.5f,.1f,.1f),"A foreign speed change is never overwritten or repeatedly reapplied");
    check(!speed.update(true,1,1,1),"Disabled slowdown leaves timing untouched");
    check(selectionClock(0)==SelectionClock::pause&&selectionClock(.5f)==SelectionClock::slow&&selectionClock(1)==SelectionClock::full,"Selection timing distinguishes pause, slow and full speed");
    check(!speed.update(true,0,1,1),"Pause never writes a zero or near-zero global timescale");
    speed.update(true,.5f,.8f,.6f);change=speed.update(true,0,.4f,.3f);
    check(change&&change->first==.8f&&change->second==.6f,"Switching from fallback slowdown to pause restores prior multipliers");
    check(selectionPauseAllowsInput(0,false)&&selectionPauseAllowsInput(1,true),"Own pause still permits carousel input");
    check(!selectionPauseAllowsInput(1,false)&&!selectionPauseAllowsInput(2,true),"A foreign pause blocks selection even with our pause open");
    Options paused;paused.selectionSpeed=0;
    const auto pauseRoundtrip=decodeOptions(encodeOptions(paused));
    check(pauseRoundtrip.selectionSpeed==0,"Pause survives settings reload");
    for(const auto display:std::array<Point,5>{{{800,600},{1280,720},{1920,1080},{3440,1440},{3840,2160}}}) {
        {
            const auto editor=editorGeometry(paused,display.x,display.y);
            check(editor.width<=display.x-23&&editor.height<=display.y-23&&editor.scale>0,"Editor remains inside every supported viewport at each text-size limit");
        }
        for(unsigned dock=0;dock<5;++dock)for(unsigned shape=0;shape<4;++shape)for(float x:{0.f,1.f})for(float y:{0.f,1.f}) {
            Options corner;corner.groupCount=4;corner.layout=2;corner.shape=shape;corner.selectorDock=dock;corner.positionX=x;corner.positionY=y;
            const auto g=hudGeometry(corner,display.x,display.y);
            check(x==0?std::abs(g.origin.x-8)<.01f:std::abs(g.origin.x+g.width*g.scale-display.x+8)<.1f,"Horizontal extrema are independent of selector docking");
            check(y==0?std::abs(g.origin.y-8)<.01f:std::abs(g.origin.y+g.height*g.scale-display.y+8)<.1f,"Vertical extrema are independent of selector reserved space");
            const float sx=g.origin.x+g.selectorOrigin.x*g.scale,sy=g.origin.y+(g.selectorOrigin.y-32)*g.scale;
            check(sx>=7.9f&&sy>=7.9f&&sx+g.selectorWidth*g.scale<=display.x-7.9f&&sy+(g.selectorHeight+32)*g.scale<=display.y-7.9f,"Selector stays visible at all four widget corners");
        }
    }
    std::cout<<"PASS: "<<checks<<" advanced checks (current save storage, group management, 1–8 layouts, input intent, ranking, exclusions, paired identity, animation readiness, timing ownership).\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

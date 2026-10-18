#include "Presets.h"
#include "SmartConsumables.h"
#include "Layout.h"
#include "LiveEditor.h"
#include <iostream>
using namespace quickslot;
unsigned checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F> void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
int main()try {
    const auto identity=[](std::uint32_t old,std::uint32_t& mapped){mapped=old;return true;};
    Entry ammo;ammo.form=0x123;ammo.name="Steel Arrow";ammo.kind=Kind::ammo;
    check(isStack(ammo)&&isInventory(ammo)&&!isConsumable(ammo)&&!tracksInstance(ammo),"Ammo is nonconsuming stack equipment");
    check(!flexibleHand(ammo)&&!pairable(ammo)&&normalizedHand(ammo,Hand::left)==Hand::right,"Ammo cannot occupy a hand or combination");
    Options options;Selection::Commit commit{0,1,0,0,true};check(!selectsConsumable(ammo,commit,options),"Browsing ammo requests equip rather than consume/select-only");
    Arsenal arsenal;arsenal.presetIdentity="character-a";arsenal.groups[0].add(ammo);
    Entry sword;sword.form=0x456;sword.owner=0x14;sword.unique=39;sword.name="Named enchanted sword";arsenal.groups[0].add(sword);
    auto potion=smartEntry(0,0,2);potion.exclusions={{0xff000002,123456}};arsenal.groups[1].add(potion);
    Entry created;created.kind=Kind::potion;created.form=0xff000001;created.recipe=999;created.name="Crafted potion";arsenal.groups[1].add(created);
    Entry spell;spell.form=0x100;spell.kind=Kind::spell;spell.spell=true;spell.name="Healing";
    Entry pair;pair.kind=Kind::combo;pair.parts={sword,spell};pair.parts[1].hand=Hand::left;pair.name="Sword and healing";arsenal.groups[2].add(pair);
    const auto saved=encodeArsenal(arsenal);auto roundtrip=decodeArsenal(saved,identity);
    check(roundtrip.groups[0].entries[0].same(ammo)&&roundtrip.presetIdentity==arsenal.presetIdentity,"Ammo and preset lineage survive a game save");
    auto bad=Json::parse(saved);bad["groups"][0]["entries"][0]["hand"]=1u;rejects([&]{decodeArsenal(bad.dump(),identity);},"Ammo hand references are rejected");
    bad=Json::parse(saved);bad["groups"][2]["entries"][0]["parts"][0]=encodeEntry(ammo);rejects([&]{decodeArsenal(bad.dump(),identity);},"Ammo cannot enter combinations through saved JSON");
    options.themeId="oathvein";options.showKeys=false;options.placementGrid=options.placementSnap=true;
    const auto restoredOptions=decodeOptions(encodeOptions(options));check(!restoredOptions.showKeys&&restoredOptions.placementGrid&&restoredOptions.placementSnap,"Visibility and grid preferences persist");
    const auto describe=[](std::uint32_t id)->Json {if((id>>24)==255)return {{"dynamic",id}};return {{"file","Example.esp"},{"local",id}};};
    const auto lookup=[](const std::string&,std::uint32_t id){return id+0x12000000;};
    const auto bytes=exportPreset(arsenal,options,false,describe);
    const auto imported=importPreset(bytes,arsenal,Options{},false,lookup);
    check(editorShortcutMatches(imported.options,43,keyModifier::shift),"Complete presets preserve the editor chord");
    check(imported.arsenal.groups[0].entries[0].form==0x12000123&&imported.arsenal.groups[0].entries[1].owner==0x12000014,"Preset resolves plugin/local IDs across load-order changes");
    check(imported.arsenal.groups[0].entries[1].unique==39&&!imported.arsenal.groups[0].entries[1].unresolved,"Same-save exact copies retain their identities");
    check(imported.arsenal.groups[1].entries[1].form==0xff000001&&imported.arsenal.groups[1].entries[1].recipe==999,"Crafted stacks retain their recipe guard");
    auto other=arsenal;other.presetIdentity="character-b";
    const auto foreign=importPreset(bytes,other,Options{},false,lookup);
    check(foreign.arsenal.groups[0].entries[1].unresolved&&foreign.arsenal.groups[2].entries[0].parts[0].unresolved,"Foreign physical-copy IDs cannot bind unrelated weapons");
    check(foreign.arsenal.groups[1].entries[1].unresolved&&foreign.arsenal.groups[1].entries[1].recipe==999,"Foreign crafted runtime IDs remain unavailable");
    check(foreign.arsenal.groups[0].entries[0].form==0x12000123&&foreign.arsenal.groups[2].entries[0].parts[1].form==0x12000100,"Ammo and spells remain portable across characters");
    check(foreign.arsenal.presetIdentity=="character-b"&&foreign.unresolved==3,"Import preserves target save lineage and reports missing copies");
    check(foreign.arsenal.groups[1].entries[0].exclusions[0].recipe==123456,"Crafted potion exclusions survive a foreign import");
    const auto exclusionReload=decodeArsenal(encodeArsenal(foreign.arsenal),[](std::uint32_t old,std::uint32_t& mapped){mapped=old;return old!=UINT32_MAX;});
    check(exclusionReload.groups[1].entries[0].exclusions[0].recipe==123456,"Recipe exclusion persists through a game reload with no live old form");
    Entry recrafted;recrafted.form=0xff123456;recrafted.recipe=123456;
    check(exclusionMatches(exclusionReload.groups[1].entries[0].exclusions[0],recrafted),"Editor and ranking agree about an imported recipe exclusion");
    const auto missing=importPreset(bytes,arsenal,options,false,[](const auto&,auto){return 0;});check(missing.arsenal.groups[0].entries[0].unresolved,"Missing plugin entries remain placeholders");
    Options target;target.names[0]="Keep name";target.keys[0]=68;target.groupCount=1;target.selectionSpeed=0;target.consumableOnRelease=true;
    target.editorKey=67;target.editorModifiers=keyModifier::ctrl|keyModifier::shift;
    const auto visual=importPreset(bytes,other,target,true,lookup);
    check(editorShortcutMatches(visual.options,67,keyModifier::ctrl|keyModifier::shift),"Appearance imports preserve the local editor key and modifiers");
    check(visual.options.themeId=="oathvein"&&!visual.options.showKeys&&visual.options.keys[0]==68&&visual.options.names[0]=="Keep name"&&visual.options.groupCount==1&&visual.options.selectionSpeed==0&&visual.options.consumableOnRelease,"Appearance import leaves controls and group identities intact");
    check(encodeArsenal(visual.arsenal)==encodeArsenal(other),"Appearance import preserves exact saved entries");
    const auto style=exportPreset(arsenal,options,true,describe);check(!Json::parse(style).contains("arsenal"),"Appearance exports omit equipment references");
    check(importPreset(style,other,target,false,lookup).appearanceOnly,"Appearance files never overwrite groups even with full import selected");
    EditHistory history;auto active=arsenal;auto activeOptions=options;history.begin(active,activeOptions);history.prepare(active,activeOptions);active=foreign.arsenal;activeOptions=foreign.options;history.finish(true,false,0);
    check(history.undo(active,activeOptions)&&encodeArsenal(active)==saved,"Complete import can be reversed as one edit");
    rejects([&]{importPreset("{",arsenal,options,false,lookup);},"Truncated JSON rejected");
    bad=Json::parse(bytes);bad["version"]=2;rejects([&]{importPreset(bad.dump(),arsenal,options,false,lookup);},"Future presets rejected");
    bad=Json::parse(bytes);bad["references"]["291"]["file"]="../Example.esp";rejects([&]{importPreset(bad.dump(),arsenal,options,false,lookup);},"Plugin paths rejected");
    rejects([&]{importPreset(std::string(2097153,'x'),arsenal,options,false,lookup);},"Oversized input rejected before parsing");
    for(const auto name:{"../x","x/y","x\\y","CON","con","NUL","AUX","LPT1","COM1","trailing ",".hidden",""})check(!validPresetName(name),"Unsafe filename rejected");
    check(validPresetName("Oathvein - Combat 2"),"Ordinary preset names accepted");
    const auto folder=std::filesystem::temp_directory_path()/"quickslot-preset-test-06";std::filesystem::create_directories(folder);
    {std::ofstream file(presetPath(folder,"Roundtrip"),std::ios::binary);file<<bytes;}
    check(readPreset(folder,"Roundtrip")==bytes&&!listPresets(folder).empty(),"Preset file reads and lists correctly");
    rejects([&]{readPreset(folder,"Missing");},"Missing file fails without modifying state");
    std::filesystem::remove(presetPath(folder,"Roundtrip"));std::filesystem::remove(folder);
    std::vector<PotionCandidate> bottles;
    for(unsigned i=0;i<3;++i){PotionCandidate b;b.entry.form=i+1;b.entry.kind=Kind::potion;b.entry.name="Potion";b.count=2;b.restore[0]=float(25u<<i);b.regeneration[0]=float(25u<<i);bottles.push_back(b);}
    auto rule=smartEntry(0,0,2);auto feedback=potionFeedback(rule,choosePotion(rule,bottles,40),40);
    check(feedback.reason=="Smallest sufficient"&&feedback.estimate.find("10 excess")!=std::string::npos,"Best-fit feedback identifies excess");
    feedback=potionFeedback(rule,choosePotion(rule,bottles,120),120);check(feedback.reason=="Strongest available"&&feedback.estimate.find("20 shortfall")!=std::string::npos,"Insufficient supply reports shortfall");
    check(potionFeedback(rule,nullptr,0).reason=="Health full"&&potionFeedback(rule,nullptr,50).reason=="No matching potion","Full and out-of-stock feedback are distinct");
    rule.policy=0;check(potionFeedback(rule,choosePotion(rule,bottles,50),50).reason=="Lowest strength","Lowest policy explained");rule.policy=1;
    check(potionFeedback(rule,choosePotion(rule,bottles,50),50).reason=="Highest strength","Highest policy explained");rule.effect=1;
    check(potionFeedback(rule,&bottles[2],20).estimate.find("+100%")!=std::string::npos,"Regeneration feedback uses percent units");
    rule=smartEntry(0,0,2);bottles[0].entry.recipe=123456;rule.exclusions=foreign.arsenal.groups[1].entries[0].exclusions;
    check(choosePotion(rule,bottles,10)!=&bottles[0],"Crafted exclusion follows recipe across runtime IDs");
    for(const auto screen:std::array<Point,5>{{{800,600},{1280,720},{1920,1080},{3440,1440},{3840,2160}}}) {
        const auto editor=editorGeometry(options,screen.x,screen.y);check(editor.width<=screen.x-24&&editor.height<=screen.y-24,"Fixed editor fits all tested resolutions");
        for(float axis:{screen.x,screen.y})for(float position:{0.f,.01f,.23f,.5f,.77f,.99f,1.f}) {
            const float extent=axis*.2f,step=placementGridStep(screen.y),snapped=snapPosition(position,extent,axis,step);
            check(snapped>=0&&snapped<=1,"Snapping stays in bounds");
            if(position==.5f)check(std::abs(snapped-.5f)<.00001f,"Grid center exactly matches display center");
            if(position==0||position==1)check(snapped==position,"Snapping retains both screen edges");
            else if(snapped>0&&snapped<1){const float cells=(8+snapped*(axis-16-extent)+extent*.5f-axis*.5f)/step;check(std::abs(cells-std::round(cells))<.001f,"Snapped centers lie on the visible grid");}
        }
    }
    Options cards;cards.shape=0;cards.showLabels=false;const auto namedCards=groupGeometry(cards);cards.showCardNames=false;
    check(groupGeometry(cards).tileWidth<namedCards.tileWidth,"Hidden card names reclaim horizontal space");
    cards.selector=4;const auto savedCards=decodeOptions(encodeOptions(cards));
    check(!savedCards.showCardNames&&savedCards.selector==4,"Card visibility and list layout survive settings roundtrip");
    Options appearance;importAppearance(appearance,cards);
    check(!appearance.showCardNames&&appearance.selector==4,"Appearance imports include new layout and card visibility");
    for(unsigned selector=0;selector<5;++selector)for(unsigned layout=0;layout<6;++layout)for(unsigned count=1;count<=8;++count)for(bool browsing:{false,true}) {
        Options o;o.layout=layout;o.shape=layout%4;o.groupCount=count;o.selector=selector;o.showCardNames=false;
        const auto g=fitPreviewGeometry(hudGeometry(o,780,600),780,600,browsing);
        check(g.origin.x>=0&&g.origin.y>=0&&g.origin.x+g.width*g.scale<=780.1f&&g.origin.y+g.height*g.scale<=600.1f,"Fitted preview keeps all groups inside its panel");
        if(browsing){const float x=g.origin.x+g.selectorOrigin.x*g.scale,y=g.origin.y+(g.selectorOrigin.y-32)*g.scale;
            check(x>=0&&y>=0&&x+g.selectorWidth*g.scale<=780.1f&&y+(g.selectorHeight+32)*g.scale<=600.1f,"Fitted preview includes full selector");}
    }
    Options previewOptions;const auto onlySelector=fitPreviewGeometry(hudGeometry(previewOptions,3840,2160),780,600,true,false);
    const float sx=onlySelector.origin.x+onlySelector.selectorOrigin.x*onlySelector.scale,sy=onlySelector.origin.y+(onlySelector.selectorOrigin.y-32)*onlySelector.scale;
    check(sx>=0&&sy>=0&&sx+onlySelector.selectorWidth*onlySelector.scale<=780.1f&&sy+(onlySelector.selectorHeight+32)*onlySelector.scale<=600.1f,"Selector-only preview fills its panel without off-screen clipping");
    std::cout<<"PASS: "<<checks<<" preset, ammo, feedback, fixed editor, grid and preview checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

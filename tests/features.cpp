#include "Presets.h"
#include "SmartConsumables.h"
#include "Layout.h"
#include "LiveEditor.h"
#include <iostream>
using namespace quickslot;
unsigned checks=0;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F> void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
#include "spacing_checks.h"
int main()try {
    spacing_test::run();
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
    Options target;target.names[0]="Keep name";target.keys[0]=68;target.groupCount=1;target.selectionSpeed=0;target.groupModes[0]=GroupMode::cycle;
    target.editorKey=67;target.editorModifiers=keyModifier::ctrl|keyModifier::shift;
    const auto visual=importPreset(bytes,other,target,true,lookup);
    check(editorShortcutMatches(visual.options,67,keyModifier::ctrl|keyModifier::shift),"Appearance imports preserve the local editor key and modifiers");
    check(visual.options.themeId=="oathvein"&&!visual.options.showKeys&&visual.options.keys[0]==68&&visual.options.names[0]=="Keep name"&&visual.options.groupCount==1&&visual.options.selectionSpeed==0&&visual.options.groupModes[0]==GroupMode::cycle,"Appearance import leaves controls and group identities intact");
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
        for(unsigned density=0;density<4;++density)for(unsigned guide=0;guide<4;++guide) {
            Options grid;grid.placementSpacing=density;grid.placementFrame=guide;grid.placementGridSize=25;
            const auto frame=placementFrame(grid,screen.x,screen.y);const float step=placementGridStep(grid,screen.y);
            check(frame.min.x>=0&&frame.min.y>=0&&frame.max.x<=screen.x&&frame.max.y<=screen.y,"Aspect guide fits the viewport");
            check(std::abs((frame.min.x+frame.max.x)*.5f-screen.x*.5f)<.01f,"Aspect guides remain centered");
            for(bool horizontal:{false,true})for(float position:{0.f,.01f,.23f,.5f,.77f,.99f,1.f}) {
                const float axis=horizontal?screen.x:screen.y,extent=axis*.2f;
                const float frameLow=horizontal?frame.min.x:frame.min.y,frameHigh=horizontal?frame.max.x:frame.max.y;

                const float low=-7,high=extent+13;
                const auto result=snapPlacementAxis(position,extent,axis,low,high,frameLow,frameHigh,step);
                check(result.value>=0&&result.value<=1,"Bounding-box snapping stays within position range");
                if(position==0||position==1)check(result.value==position,"Snapping preserves both screen extents");
                if(result.aligned) {
                    const float start=8+result.value*(axis-16-extent);
                    check(std::min({std::abs(start+low-result.guide),std::abs(start+(low+high)*.5f-result.guide),std::abs(start+high-result.guide)})<.001f,"A rendered bound edge or center lands exactly on its guide");
                }
            }
        }
    }
    const auto aligned=snapPlacementAxis(.3f,200,1920,0,200,0,1920,16,std::array{520.f,720.f});
    check(aligned.aligned&&std::abs(aligned.guide-520)<.01f,"Nearby element edges take priority over the grid");
    Options textOptions;
    for(float scale:{.55f,1.f,1.5f})for(float size:{.75f,1.f,1.5f}) {
        textOptions.scale=scale;textOptions.textScale=size;
        check(scale*hudTextFactor(textOptions)>=size,"Shrinking icons retains readable label sizing");
        for(unsigned layout=0;layout<5;++layout) {
            textOptions.selector=layout;textOptions.showHints=true;const auto g=selectorGeometry(textOptions);const float factor=hudTextFactor(textOptions);
            check(g.statusY>=g.nameY+(layout==2?20:23)*factor,"Selected name and status do not overlap");
            check(g.pagerY>=g.statusY+13*factor,"Pager follows the status row");
            check(g.hintY>=g.pagerY+6+12*factor&&g.height>=g.hintY+13*factor,"Hints fit below pager at every text scale");
            if(layout==4)check(g.listHeight>=16*factor+14&&g.listPitch>=g.listHeight,"List rows reserve enough height for scaled names");
        }
    }
    textOptions.textScale=1.35f;textOptions.placementSpacing=3;textOptions.placementFrame=2;textOptions.placementGridSize=27;
    const auto textReload=decodeOptions(encodeOptions(textOptions));
    check(textReload.textScale==1.35f&&textReload.placementSpacing==3&&textReload.placementFrame==2&&textReload.placementGridSize==27,"Text and grid settings persist");
    Options textImported;importAppearance(textImported,textOptions);
    check(textImported.textScale==1.35f&&textImported.placementFrame==2&&textImported.placementGridSize==27,"Appearance presets include text size and grid options");
    Options cards;cards.shape=0;cards.showLabels=false;const auto namedCards=groupGeometry(cards);cards.showCardNames=false;
    check(groupGeometry(cards).tileWidth<namedCards.tileWidth,"Hidden card names reclaim horizontal space");
    cards.selector=4;const auto savedCards=decodeOptions(encodeOptions(cards));
    check(!savedCards.showCardNames&&savedCards.selector==4,"Card visibility and list layout survive settings roundtrip");
    Options appearance;importAppearance(appearance,cards);
    check(!appearance.showCardNames&&appearance.selector==4,"Appearance imports include new layout and card visibility");
    for(unsigned selector=0;selector<5;++selector)for(unsigned layout=0;layout<6;++layout)for(unsigned count=1;count<=8;++count)for(bool browsing:{false,true}) {
        Options o;o.layout=layout;o.shape=layout%4;o.groupCount=count;o.selector=selector;o.showCardNames=false;
        const auto g=previewGeometry(o,1920,1080,780,600,browsing);
        check(g.origin.x>=0&&g.origin.y>=0&&g.origin.x+g.width*g.scale<=780.1f&&g.origin.y+g.height*g.scale<=600.1f,"Fitted preview keeps all groups inside its panel");
        if(browsing){const float x=g.origin.x+g.selectorOrigin.x*g.scale,y=g.origin.y+(g.selectorOrigin.y-32)*g.scale;
            check(x>=0&&y>=0&&x+g.selectorWidth*g.scale<=780.1f&&y+(g.selectorHeight+32)*g.scale<=600.1f,"Fitted preview includes full selector");}
    }
    Options previewOptions;const auto onlySelector=previewGeometry(previewOptions,3840,2160,780,600,true,false);
    const float sx=onlySelector.origin.x+onlySelector.selectorOrigin.x*onlySelector.scale,sy=onlySelector.origin.y+(onlySelector.selectorOrigin.y-32)*onlySelector.scale;
    check(sx>=0&&sy>=0&&sx+onlySelector.selectorWidth*onlySelector.scale<=780.1f&&sy+(onlySelector.selectorHeight+32)*onlySelector.scale<=600.1f,"Selector-only preview fills its panel without off-screen clipping");
    const auto sameSize=[](float a,float b){return std::abs(a-b)<.001f;};
    for(unsigned layout=0;layout<6;++layout)for(unsigned shape=0;shape<4;++shape)for(unsigned count:{1u,4u,8u})
        for(bool labels:{false,true})for(float scale:{.55f,1.5f})for(const Point screen:{Point{1280,720},Point{3840,2160}}) {
            Options tight;tight.layout=layout;tight.shape=shape;tight.groupCount=count;tight.showLabels=labels;tight.scale=scale;tight.spacing=0;tight.selectorSpacing=0;
            auto loose=tight;loose.spacing=40;loose.selectorSpacing=32;
            const auto checkSpacing=[&](const HudGeometry& a,const HudGeometry& b) {
                check(sameSize(a.groups.tileWidth,b.groups.tileWidth)&&sameSize(a.groups.tileHeight,b.groups.tileHeight),"Spacing preserves logical group frame size");
                check(b.scale<=a.scale+.001f,"Fitting a wider layout never enlarges its icons");
                if(count>1) {
                    const auto distance=[](const HudGeometry& g){const auto a=g.groups.tiles[0],b=g.groups.tiles[1];return std::hypot(a.x-b.x,a.y-b.y);};
                    check(distance(b)>distance(a),"Spacing must increase actual separation between group icons");
                }else check(sameSize(a.width,b.width)&&sameSize(a.height,b.height),"Single group ignores spacing");
            };
            checkSpacing(hudGeometry(tight,screen.x,screen.y),hudGeometry(loose,screen.x,screen.y));
            for(bool browsing:{false,true})for(unsigned dock:{0u,2u,4u}) {
                tight.selectorDock=loose.selectorDock=dock;
                const auto a=previewGeometry(tight,screen.x,screen.y,600,440,browsing),b=previewGeometry(loose,screen.x,screen.y,600,440,browsing);
                checkSpacing(a,b);
                check(b.origin.x>=0&&b.origin.y>=0&&b.origin.x+b.width*b.scale<=600.1f&&b.origin.y+b.height*b.scale<=440.1f,"Widest group preview stays in the panel");
            }
        }
    for(unsigned selector=0;selector<5;++selector) {
        Options tight;tight.selector=selector;tight.selectorSpacing=0;auto loose=tight;loose.selectorSpacing=32;
        const auto a=previewGeometry(tight,1920,1080,600,440,true,false,false,4,{},5,1),b=previewGeometry(loose,1920,1080,600,440,true,false,false,4,{},5,1);
        check(sameSize(a.scale,b.scale),"Selector spacing cannot resize its preview icons");
    }
    Options resized;resized.shape=3;resized.layout=2;resized.groupCount=4;resized.showLabels=false;resized.scale=.75f;
    const auto smaller=previewGeometry(resized,1920,1080,600,440,false,true,false,4,{},5,1);resized.scale=1.5f;
    const auto larger=previewGeometry(resized,1920,1080,600,440,false,true,false,4,{},5,1);
    check(larger.scale>smaller.scale*1.9f,"Widget scale changes icon size in the preview");
    std::cout<<"PASS: "<<checks<<" preset, ammo, feedback, fixed editor, grid and preview checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

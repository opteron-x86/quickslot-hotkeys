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
        textOptions.scale=scale;textOptions.textScale=textOptions.selectorTextScale=size;
        {
            auto earlier=Json::parse(encodeOptions(Options{}));earlier["layoutVersion"]=2;earlier["scale"]=scale;earlier["textScale"]=size;earlier["opacity"]=.6f;earlier["equipmentTextScale"]=1.2f;
            for(const char* added:{"selectorScale","selectorTextScale","selectorOpacity","equipmentOpacity"})earlier.erase(added);
            const auto migrated=decodeOptions(earlier.dump());const float kept=std::max(scale,1.f)*size;
            check(std::abs(migrated.scale*hudTextFactor(migrated)-kept)<.001f&&std::abs(migrated.scale*selectorTextFactor(migrated)-kept)<.001f,"Settings from earlier versions keep their widget and selector label sizes");
            check(std::abs(migrated.equipmentTextScale-1.2f*size)<.001f&&migrated.selectorOpacity==.6f&&migrated.equipmentOpacity==.6f&&migrated.selectorScale==migrated.scale,"Earlier settings carry their scale, text size and opacity to each part of the HUD");
            check(encodeOptions(decodeOptions(encodeOptions(migrated)))==encodeOptions(migrated),"Converted settings load unchanged afterwards");
        }
        for(unsigned layout=0;layout<5;++layout) {
            textOptions.selector=layout;textOptions.showHints=true;const auto g=selectorGeometry(textOptions);const float factor=selectorTextFactor(textOptions);
            if(layout==4) {
                check(g.listHeaderHeight>0&&g.listVisible>0,"Compact list reserves a header bar and visible row count");
                check(g.listHeight>=15*factor+14&&g.listPitch>=g.listHeight,"List rows reserve enough height for scaled names");
                check(g.hintY>=g.ruleY&&g.height>=g.hintY+13*factor,"Compact list fits hints cleanly below the panel");
            } else {
                check(g.statusY>=g.nameY+(layout==2?20:23)*factor,"Selected name and status do not overlap");
                check(g.pagerY>=g.statusY+13*factor,"Pager follows the status row");
                check(g.hintY>=g.pagerY+6+12*factor&&g.height>=g.hintY+13*factor,"Hints fit below pager at every text scale");
            }
        }
    }
    for(unsigned dock=0;dock<5;++dock)for(float own:{.5f,2.f}) {
        Options sized;sized.selectorDock=dock;sized.scale=1.5f;sized.selectorScale=1.5f;sized.anchor=unanchored;sized.positionX=.5f;sized.positionY=.8f;const auto linked=hudGeometry(sized,1920,1080);
        sized.selectorScale=own;const auto free=hudGeometry(sized,1920,1080);
        check(linked.selectorScale==linked.scale&&std::abs(free.selectorScale-own)<.001f&&free.scale==linked.scale,"The selector has its own scale and the widget keeps its size");
        check(free.origin.x==linked.origin.x&&free.origin.y==linked.origin.y,"Resizing the selector leaves the widget where it is");
        const auto corner=selectorCorner(free),extent=selectorExtent(free);
        check(std::abs(extent.x-free.selectorWidth*own)<.01f&&corner.x>=7.9f&&corner.y>=7.9f&&corner.x+extent.x<=1912.1f&&corner.y+extent.y<=1072.1f,"A resized selector stays on screen at every dock");
        if(dock==0)check(std::abs(corner.y+extent.y+sized.selectorGap*free.scale-free.origin.y)<.01f&&std::abs(corner.x+extent.x*.5f-free.origin.x-free.width*free.scale*.5f)<.01f,"A resized selector stays centered above the widget at its set distance");
        const auto fitted=previewGeometry(sized,1920,1080,780,600,true);const auto shown=selectorCorner(fitted),size=selectorExtent(fitted);
        check(shown.x>=-.1f&&shown.y>=-.1f&&shown.x+size.x<=780.1f&&shown.y+size.y<=600.1f&&std::abs(fitted.selectorScale/fitted.scale-own/1.5f)<.001f,"The editor preview fits a resized selector");
    }
    {
        Options huge;huge.selectorScale=3;huge.scale=3;const auto g=hudGeometry(huge,1280,720);
        check(g.width*g.scale<=1264.1f&&g.height*g.scale<=704.1f&&selectorExtent(g).x<=1264.1f&&selectorExtent(g).y<=704.1f,"The largest scales still fit a 720p screen");
    }
    for(const auto screen:{Point{1280,720},Point{1920,1080},Point{2560,1080},Point{3840,2160}})for(unsigned area=0;area<4;++area)for(float fx:{0.f,.2f,.5f,.8f,1.f})for(float fy:{0.f,.35f,.5f,.9f,1.f}) {
        Options placed;placed.anchorFrame=area;const Point size{340,120};
        const auto corner=placedCorner(unanchored,0,0,fx,fy,size,placed,screen.x,screen.y);
        const auto anchored=anchorFor(corner,size,placed,screen.x,screen.y);
        const auto again=placedCorner(anchored.anchor,anchored.x,anchored.y,fx,fy,size,placed,screen.x,screen.y);
        check(anchored.anchor<unanchored&&std::abs(again.x-corner.x)<.01f&&std::abs(again.y-corner.y)<.01f,"Anchoring an element leaves it exactly where it was");
        for(unsigned chosen=0;chosen<9;++chosen) {
            const auto fixed=anchorFor(corner,size,placed,screen.x,screen.y,chosen);const auto same=placedCorner(fixed.anchor,fixed.x,fixed.y,0,0,size,placed,screen.x,screen.y);
            check(fixed.anchor==chosen&&std::abs(same.x-corner.x)<.01f&&std::abs(same.y-corner.y)<.01f,"Any of the nine anchors can hold the same position");
        }
        if(area==0&&(fx==0||fx==.5f||fx==1)&&(fy==0||fy==.5f||fy==1))check(anchored.anchor==unsigned(fx*2)+3*unsigned(fy*2),"Corners, edges and the center pick their own anchor");
    }
    {
        Options fresh;const auto g=hudGeometry(fresh,1920,1080);const auto shown=elementPlacement(fresh,g,0,1920,1080);
        check(fresh.anchor==8&&shown.anchor==8&&std::abs(shown.x+8)<.01f&&std::abs(shown.y+8)<.01f,"A new installation shows its bottom-right position as an anchor and offset");
        auto anchored=fresh;placeElement(anchored,0,shown);const auto after=hudGeometry(anchored,1920,1080);
        check(std::abs(after.origin.x-g.origin.x)<.01f&&std::abs(after.origin.y-g.origin.y)<.01f,"Saving that anchor keeps the widget in place");
        anchored.selectorDock=4;placeElement(anchored,1,{4,25,-40});placeElement(anchored,2,{6,30,-190});anchored.anchorFrame=1;
        const auto saved=decodeOptions(encodeOptions(anchored));
        check(saved.anchor==8&&saved.selectorAnchor==4&&saved.selectorOffsetX==25&&saved.selectorOffsetY==-40&&saved.equipmentAnchor==6&&saved.equipmentOffsetY==-190&&saved.anchorFrame==1,"Anchors, offsets and the screen area persist");
        auto earlier=Json::parse(encodeOptions(anchored));for(const char* added:{"anchor","offsetX","offsetY","selectorAnchor","selectorOffsetX","selectorOffsetY","equipmentAnchor","equipmentOffsetX","equipmentOffsetY","anchorFrame"})earlier.erase(added);
        earlier["positionX"]=.3f;earlier["positionY"]=.6f;const auto loaded=decodeOptions(earlier.dump());
        check(loaded.anchor==unanchored&&loaded.selectorAnchor==unanchored&&loaded.equipmentAnchor==unanchored&&loaded.positionX==.3f&&loaded.positionY==.6f,"Settings saved before anchors keep their proportional positions");
        Options wild;wild.anchor=77;wild.offsetX=1e9f;wild.offsetY=NAN;wild.anchorFrame=12;sanitizeOptions(wild);
        check(wild.anchor==unanchored&&wild.offsetX==8000&&wild.offsetY==Options{}.offsetY&&wild.anchorFrame==3,"Invalid placement values are bounded");
        Options look;importAppearance(look,anchored);check(look.anchor==8&&look.selectorAnchor==4&&look.anchorFrame==1,"Appearance presets carry anchored placement");
    }
    {
        Options grid;grid.layout=6;grid.shape=2;grid.groupCount=6;grid.gridColumns=3;grid.spacing=10;grid.showLabels=false;
        const auto three=groupGeometry(grid);
        check(three.tiles.size()==6&&std::abs(three.width-(3*78+2*10))<.01f&&std::abs(three.height-(2*78+10))<.01f,"A grid fills rows of the chosen width with true 1:1 squares");
        check(three.tiles[3].x==three.tiles[0].x&&three.tiles[3].y>three.tiles[0].y&&three.tiles[1].y==three.tiles[0].y,"Grid tiles run left to right, then down");
        grid.gridColumns=1;const auto column=groupGeometry(grid);grid.gridColumns=8;const auto row=groupGeometry(grid);
        check(std::abs(column.width-78)<.01f&&std::abs(row.height-78)<.01f,"One column stacks the groups and eight columns keep six in a row");
        grid.groupCount=2;grid.gridColumns=2;check(std::abs(groupGeometry(grid).width-(2*78+10))<.01f,"Two groups in a grid keep the set spacing");
        Options sized;sized.shape=1;sized.showLabels=false;const auto normal=groupGeometry(sized);sized.tileSize=2;const auto doubled=groupGeometry(sized);
        check(std::abs(doubled.tileWidth-normal.tileWidth*2)<.01f&&std::abs(doubled.tileHeight-normal.tileHeight*2)<.01f,"Tile size scales circle, square and diamond tiles");
        sized.showLabels=true;sized.tileSize=1;const auto named=groupGeometry(sized);sized.tileSize=2;const auto namedLarge=groupGeometry(sized);
        check(std::abs((namedLarge.tileHeight-named.tileHeight)-71)<.01f,"The name under a resized tile keeps its own height");
        Options cards;cardsPreset(cards);const auto standard=groupGeometry(cards);cards.cardWidth=180;const auto wider=groupGeometry(cards);cards.tileSize=2;
        check(std::abs(wider.tileWidth-standard.tileWidth-(180-97))<.01f&&groupGeometry(cards).tileHeight==standard.tileHeight,"Card width widens the name area and tile size leaves cards alone");
        Options strip;cardsPreset(strip);strip.selector=0;const auto five=selectorGeometry(strip,12);strip.selectorVisible=3;const auto nine=selectorGeometry(strip,12),few=selectorGeometry(strip,3);strip.selectorVisible=1;
        check(nine.width>five.width&&std::abs(few.width-selectorGeometry(strip,3).width)<.01f,"More visible items widen the strip only when the group has them");
        Options wheel;wheel.selector=wheelSelector;const auto tight=selectorGeometry(wheel,4);wheel.wheelSize=181;const auto open=selectorGeometry(wheel,4);
        check(tight.reach>=62&&std::abs(open.reach-150)<.01f&&open.width>tight.width,"Wheel size sets how far a wheel's tiles sit from its center");
        Options ring;ring.selector=wheelSelector;ring.wheelStyle=1;constexpr float pi=3.141592654f;
        for(const std::size_t count:{std::size_t(1),std::size_t(2),std::size_t(7),std::size_t(16),std::size_t(30)}) {
            const auto wedges=selectorGeometry(ring,count);
            const float chord=count>1?2*wedges.reach*std::sin(pi/float(count))-wedges.wedgeGap:wedges.outer;
            check(wedges.segments&&wedges.ring.size()==count&&wedges.outer>=ring.wheelSize&&std::abs(wedges.outer-wedges.inner-wedges.outer*ring.wheelThickness)<.01f,"A segmented wheel has a wedge for every entry in a ring that takes its share of the wheel");
            check(chord>=(count>Group::limit?25.9f:39.9f)&&std::abs(wedges.ring[0].x-wedges.hub.x)<.01f&&wedges.ring[0].y<wedges.hub.y,"Wedges are wide enough for an icon and start at the top");
            check(wedges.hub.y-wedges.outer-wedges.lift>=-27&&wedges.hub.x+wedges.outer+wedges.lift<=wedges.width&&wedges.pagerY>wedges.hub.y+wedges.outer&&wedges.height>wedges.pagerY,"The ring and the group line beneath it stay inside the selector");
        }
        check(selectorGeometry(ring,7).outer==ring.wheelSize&&selectorGeometry(ring,30).outer>ring.wheelSize,"A wheel keeps its size until its entries need more room");
        {
            // The ring grows with the wheel instead of leaving a larger empty center.
            Options big=ring;big.wheelSize=600;const auto small=selectorGeometry(ring,8),large=selectorGeometry(big,8);
            check(std::abs((large.outer-large.inner)/(small.outer-small.inner)-600/ring.wheelSize)<.01f&&std::abs(large.inner/large.outer-small.inner/small.outer)<.01f,"The ring keeps its proportion as the wheel grows");
            check(small.nameInHub&&!small.nameBelow&&large.nameInHub,"The pointed item's name sits in a roomy center");
            Options filled=ring;filled.wheelThickness=1;const auto pie=selectorGeometry(filled,8);
            check(pie.inner==0&&pie.hubRadius==0&&!pie.nameInHub&&pie.nameBelow&&pie.reach>pie.outer*.5f,"A ring filled to the center moves the name below and its icons outward");
            Options hidden=ring;hidden.wheelName=2;const auto quiet=selectorGeometry(hidden,8);Options below=ring;below.wheelName=1;const auto under=selectorGeometry(below,8);
            check(!quiet.nameInHub&&!quiet.nameBelow&&quiet.height<under.height&&under.nameBelow&&!under.nameInHub,"The name can sit below the wheel or be hidden");
            Options footless=ring;footless.selectorFooter=false;check(selectorGeometry(footless,8).height<small.height,"Hiding the group line shortens the selector");
            Options turned=ring;turned.wheelRotation=90;const auto side=selectorGeometry(turned,4);
            check(side.ring[0].x>side.hub.x+side.reach*.99f&&std::abs(side.ring[0].y-side.hub.y)<.01f,"Rotation turns the first slot away from the top");
            check(wheelIndex(1,0,4,.1f,wheelTurn(turned))==0&&wheelIndex(0,1,4,.1f,wheelTurn(turned))==3&&wheelIndex(0,1,4,.1f)==0,"Pointing follows a turned wheel");
            Options lifted=ring;const auto raised=selectorGeometry(lifted,8);lifted.wheelLift=false;check(raised.lift>0&&selectorGeometry(lifted,8).lift==0,"The pointed segment can stay in line");
            Options odd=ring;odd.wheelSize=NAN;odd.wheelThickness=7;odd.wheelGap=-4;odd.wheelRotation=INFINITY;odd.wheelOpacity=-1;odd.wheelName=9;odd.wheelHighlight=11;sanitizeOptions(odd);
            check(std::isfinite(odd.wheelSize)&&odd.wheelThickness==1&&odd.wheelGap==0&&std::isfinite(odd.wheelRotation)&&odd.wheelOpacity==0&&odd.wheelName==2&&odd.wheelHighlight==3,"Invalid wheel settings are bounded");
            Options marked;marked.chargeIcons=9;sanitizeOptions(marked);check(marked.chargeIcons==2&&Options{}.chargeIcons==1&&decodeOptions(encodeOptions(marked)).chargeIcons==2,"The charge on icons is a choice that persists inside its range");
            Options plain;plain.selector=1;const auto pips=selectorGeometry(plain);plain.showCharge=false;const auto bare=selectorGeometry(plain);
            check(pips.pagerY>bare.pagerY&&pips.height>bare.height,"Shown charge gets its own line under the status");
            Options described;described.selector=1;const auto with=selectorGeometry(described);described.showDescriptions=false;const auto without=selectorGeometry(described);
            check(with.pagerY>=with.descriptionY+2*14-.01f&&with.descriptionY>with.statusY&&with.height>without.height&&decodeOptions(encodeOptions(described)).showDescriptions==false,"Descriptions get two lines above the group line and can be turned off");
            Options taps;check(taps.tapRecall,"The first tap on another group equips its item by default");taps.tapRecall=false;
            auto tapJson=Json::parse(encodeOptions(taps));check(!decodeOptions(tapJson.dump()).tapRecall,"Stepping on every tap persists");tapJson.erase("tapRecall");check(decodeOptions(tapJson.dump()).tapRecall,"Earlier settings keep the first-tap recall");
        }
        {
            // Settings from before the wheel had a size draw the same ring.
            auto earlier=Json::parse(encodeOptions(Options{}));earlier.erase("wheelSize");earlier.erase("wheelThickness");earlier["wheelRadius"]=150;
            const auto migrated=decodeOptions(earlier.dump());
            check(migrated.wheelSize==178&&std::abs(migrated.wheelSize*migrated.wheelThickness-56)<.01f,"An earlier wheel radius becomes a wheel size with the same ring");
            auto linked=Json::parse(encodeOptions(Options{}));linked["scale"]=1.5f;linked["selectorScale"]=2;linked["selectorScaleLinked"]=true;
            auto unlinked=linked;unlinked["selectorScaleLinked"]=false;auto current=linked;current.erase("selectorScaleLinked");
            check(decodeOptions(linked.dump()).selectorScale==1.5f&&decodeOptions(unlinked.dump()).selectorScale==2&&decodeOptions(current.dump()).selectorScale==2,"A selector that matched the widget's scale keeps that size");
            check(chargeFraction(50,100)==.5f&&chargeFraction(0,100)==0&&chargeFraction(150,100)==1&&chargeFraction(-1,100)==1&&chargeFraction(NAN,100)==1&&chargeFraction(10,0)<0&&chargeFraction(10,NAN)<0,"Enchantment charge reads as a share of the weapon's capacity");
        }
        check(!selectorGeometry(wheel,30).segments&&selectorGeometry(wheel,30).ring.size()==Group::limit,"A tile wheel keeps to a full group of slots");
        ring.wheelStyle=7;sanitizeOptions(ring);check(ring.wheelStyle==1&&decodeOptions(encodeOptions(ring)).wheelStyle==1&&decodeOptions(encodeOptions(Options{})).wheelStyle==0,"The wheel style persists inside its range");
        Options keyed;cardsPreset(keyed);const auto withKeys=groupGeometry(keyed);keyed.showKeys=false;
        check(std::abs(withKeys.tileWidth-groupGeometry(keyed).tileWidth-cardKeyColumn)<.01f,"A card keeps a column for its key while keys are shown");
        Options look;look.cardWidth=150;look.tileSize=1.4f;look.gridColumns=5;look.layout=6;look.mirrorCards=true;look.selectorVisible=2;look.wheelSize=220;look.wheelThickness=.6f;look.wheelHubCircle=false;look.wheelHighlight=3;look.iconScale=1.3f;look.backgroundOpacity=.4f;
        look.customTextColor=look.customSelectionColor=true;look.textColor={.1f,.2f,.3f};look.selectionColor={.4f,.5f,.6f};look.groupColored[2]=true;look.groupColors[2]={.7f,.8f,.9f};
        const auto saved=decodeOptions(encodeOptions(look));
        check(encodeOptions(saved)==encodeOptions(look)&&saved.layout==6&&saved.gridColumns==5&&saved.mirrorCards&&saved.textColor[2]==.3f&&saved.groupColored[2]&&saved.groupColors[2][0]==.7f,"The new look settings persist");
        Options mine;mine.groupColored[0]=true;mine.groupColors[0]={.2f,.3f,.4f};
        const auto styled=mergeOptions(mine,look,presetPart::look);
        check(styled.cardWidth==150&&styled.customTextColor&&styled.groupColored[0]&&!styled.groupColored[2],"A preset's look brings its sizes and colors; group colors stay with the groups");
        check(mergeOptions(mine,look,presetPart::groups).groupColored[2],"Group colors travel with a preset's group setup");
        Options wild;wild.gridColumns=99;wild.tileSize=0;wild.cardWidth=NAN;wild.selectorVisible=40;wild.textColor={5,-1,NAN};wild.groupColors[1]={INFINITY,2,-3};sanitizeOptions(wild);
        check(wild.gridColumns==8&&wild.tileSize==.6f&&wild.cardWidth==97&&wild.selectorVisible==3&&wild.textColor[0]==1&&wild.textColor[1]==0&&wild.groupColors[1][1]==1&&wild.groupColors[1][2]==0,"Invalid values for the new settings are bounded");
        auto earlier=Json::parse(encodeOptions(Options{}));earlier["layoutVersion"]=2;earlier["themeId"]="norden";earlier.erase("iconScale");
        earlier["themeAdjustments"]={{"norden",{{"customAccent",false},{"accent",{.5f,.5f,.5f}},{"frameOpacity",.8f},{"iconScale",1.15f}}},{"edge",{{"customAccent",false},{"accent",{.5f,.5f,.5f}},{"frameOpacity",1.f},{"iconScale",.7f}}}};
        const auto migrated=decodeOptions(earlier.dump());
        check(std::abs(migrated.iconScale-1.15f)<.001f&&migrated.themeAdjustments.at("norden").iconScale==1&&migrated.themeAdjustments.at("norden").frameOpacity==.8f,"The active theme's icon size becomes the one icon size");
    }
    textOptions.textScale=1.35f;textOptions.placementSpacing=3;textOptions.placementFrame=2;textOptions.placementGridSize=27;
    const auto textReload=decodeOptions(encodeOptions(textOptions));
    check(textReload.textScale==1.35f&&textReload.placementSpacing==3&&textReload.placementFrame==2&&textReload.placementGridSize==27,"Text and grid settings persist");
    Options textImported;importAppearance(textImported,textOptions);
    check(textImported.textScale==1.35f&&textImported.placementFrame==2&&textImported.placementGridSize==27,"Appearance presets include text size and grid options");
    Options cards;cardsPreset(cards);cards.shape=0;cards.showLabels=false;const auto namedCards=groupGeometry(cards);cards.showCardNames=false;
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
    {
        // The Free arrangement: every group tile has its own anchor, offset and size.
        const float W=1920,H=1080;
        Options arranged;arranged.groupCount=4;arranged.layout=2;arranged.shape=3;
        const auto before=hudGeometry(arranged,W,H);
        auto free=arranged;freeTilesFrom(free,2,W,H);const auto after=hudGeometry(free,W,H);
        check(free.layout==freeLayout,"Choosing Free switches the arrangement");
        bool same=true;for(int slot=0;slot<4;++slot){const auto a=elementCorner(before,firstTileElement+slot),b=elementCorner(after,firstTileElement+slot);same&=std::abs(a.x-b.x)<.5f&&std::abs(a.y-b.y)<.5f;}
        check(same,"Free starts with every tile where the previous arrangement had it");
        auto split=free;placeElement(split,firstTileElement+0,{3,16,0});placeElement(split,firstTileElement+1,{3,16,120});
        placeElement(split,firstTileElement+2,{5,-16,0});placeElement(split,firstTileElement+3,{5,-16,120});
        const auto sides=hudGeometry(split,W,H);
        const auto left=elementCorner(sides,firstTileElement+0),right=elementCorner(sides,firstTileElement+2);
        check(left.x<W*.2f&&right.x+elementExtent(sides,firstTileElement+2).x>W*.8f&&std::abs(left.y+elementExtent(sides,firstTileElement).y*.5f-H*.5f)<2,"Two tiles can sit on each side of the screen");
        check(std::abs(sides.scale-hudGeometry(arranged,W,H).scale)<.001f,"Spreading tiles across the screen leaves their size alone");
        const auto moved=split.tilePlaces[split.groupOrder[0]];
        auto reordered=split;std::swap(reordered.groupOrder[0],reordered.groupOrder[1]);
        check(reordered.tilePlaces[reordered.groupOrder[1]]==moved,"A tile's position belongs to its group, not to its place in the order");
        auto sized=split;sized.tilePlaces[sized.groupOrder[2]].scale=2;const auto big=hudGeometry(sized,W,H);
        check(std::abs(elementExtent(big,firstTileElement+2).x-2*elementExtent(sides,firstTileElement+2).x)<.5f&&std::abs(elementExtent(big,firstTileElement+0).x-elementExtent(sides,firstTileElement+0).x)<.01f,"A tile's own size changes that tile alone");
        auto added=split;added.groupCount=5;const auto five=hudGeometry(added,W,H);
        const auto last=elementCorner(five,firstTileElement+3),fresh=elementCorner(five,firstTileElement+4);
        check(fresh.x>last.x&&std::abs(fresh.y-last.y)<.5f,"A group added later sits beside the last tile");
        auto docked=split;docked.selectorDock=0;
        const auto atRight=hudGeometry(docked,W,H,4,{},5,2),atLeft=hudGeometry(docked,W,H,4,{},5,0);
        const auto rightTile=elementCorner(atRight,firstTileElement+2),leftTile=elementCorner(atLeft,firstTileElement+0);
        check(selectorCorner(atRight).x>W*.5f&&selectorCorner(atLeft).x<W*.5f&&selectorCorner(atRight).y<rightTile.y&&selectorCorner(atLeft).y<leftTile.y,"A docked selector opens beside the tile in use");
        const auto round=decodeOptions(encodeOptions(sized));
        check(round.tilePlaces==sized.tilePlaces&&round.layout==freeLayout,"Free tile positions and sizes survive reload");
        auto older=Json::parse(encodeOptions(sized));older["tilePlaces"][0]=Json::array({3,10,20});older["tilePlaces"][1]=Json::array({99,1e9,"x"});
        const auto olderLoaded=decodeOptions(older.dump());
        check(olderLoaded.tilePlaces[0]==TilePlace{3,10,20,1}&&olderLoaded.tilePlaces[1]==TilePlace{},"A tile saved without a size gets 100% and an unreadable one keeps its default");
        Options wildTiles;wildTiles.tilePlaces[0]={40,1e9f,NAN,9};sanitizeOptions(wildTiles);
        check(wildTiles.tilePlaces[0]==TilePlace{9,8000,0,3},"Invalid tile placements are bounded");
        Options look;importAppearance(look,sized);check(look.tilePlaces==sized.tilePlaces&&look.layout==freeLayout,"Free tile positions travel with appearance presets");
    }
    {
        // Settings from before anchors take the anchor and offset that keep each element where it was.
        Options legacy;legacy.anchor=unanchored;legacy.positionX=.3f;legacy.positionY=.6f;legacy.equipmentAnchor=unanchored;legacy.equipmentX=.1f;legacy.equipmentY=.4f;
        legacy.selectorDock=4;legacy.selectorAnchor=unanchored;legacy.selectorX=.7f;legacy.selectorY=.2f;
        check(hasUnplacedElements(legacy)&&!hasUnplacedElements(Options{}),"Old proportional positions are recognised and new installations have none");
        const auto was=hudGeometry(legacy,2560,1440);auto converted=legacy;
        check(anchorUnplacedElements(converted,2560,1440)&&!hasUnplacedElements(converted)&&!anchorUnplacedElements(converted,2560,1440),"Conversion anchors every element once");
        const auto now=hudGeometry(converted,2560,1440);bool kept=true;
        for(int element=0;element<3;++element){const auto a=elementCorner(was,element),b=elementCorner(now,element);kept&=std::abs(a.x-b.x)<.5f&&std::abs(a.y-b.y)<.5f;}
        check(kept,"Converted elements stay where they were");
        check(!anchorUnplacedElements(converted,0,0),"Nothing converts before the screen size is known");
    }
    {
        // Concise item descriptions for weapons, armor, ammo, spells, powers, shouts, outfits and combinations.
        check(formatWeaponInfo(42.4f)=="Damage 42","Unenchanted weapons report rounded damage");
        check(formatWeaponInfo(34.f,"Burns target for 15 points.")=="Damage 34 · Burns target for 15 points.","Enchanted weapons combine damage and enchantment");
        check(formatWeaponInfo(11.f,{},"Causes 25 points of poison damage.")=="Damage 11 · Poisoned: Causes 25 points of poison damage.","Poisoned weapons append the active poison effect");
        check(formatWeaponInfo(0.f,"Shoots a fireball.")=="Shoots a fireball.","Staves without physical damage show their enchantment alone");
        check(formatArmorInfo(48.2f,"Increases Health by 50 points.")=="Armor 48 · Increases Health by 50 points.","Enchanted armor combines rating and enchantment");
        check(formatArmorInfo(0.f,"Destruction spells cost 25% less to cast.")=="Destruction spells cost 25% less to cast.","Zero-armor jewelry and clothing omit the armor rating");
        check(formatAmmoInfo(14.f)=="Damage 14"&&formatAmmoInfo(14.f,"Explodes on impact.")=="Damage 14 · Explodes on impact.","Ammunition reports damage and optional effect");
        check(formatSpellInfo(133.2f,false,"A fiery explosion.")=="133 Magicka · A fiery explosion.","Cast spells report Magicka cost and effect");
        check(formatSpellInfo(14.f,true,"A gout of fire.")=="14 Magicka/s · A gout of fire.","Concentration spells report Magicka per second");
        check(formatSpellInfo(0.f,false,"Foes flee for 30 seconds.")=="Foes flee for 30 seconds.","Powers with zero Magicka cost show their effect alone");
        check(formatShoutInfo({15.f,20.f,45.f},"Your Voice is raw power.")=="Cooldown 15s / 20s / 45s · Your Voice is raw power.","Shouts list unlocked word cooldowns and description");
        check(formatShoutInfo({15.f},{})=="Cooldown 15s","Single-word shouts list one cooldown");
        check(formatOutfitInfo(214.f,3)=="Armor 214 · 3 enchanted"&&formatOutfitInfo(0.f,2)=="2 enchanted","Outfits summarize total armor and enchanted piece count");
        check(formatComboInfo("Damage 34","Armor 22")=="Damage 34 + Armor 22"&&formatComboInfo("Damage 34",{})=="Damage 34","Combinations join active hand summaries");
    }
    std::cout<<"PASS: "<<checks<<" preset, ammo, feedback, fixed editor, grid and preview checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

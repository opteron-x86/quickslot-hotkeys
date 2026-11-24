#include "LiveEditor.h"
#include "Settings.h"
#include "Themes.h"
#include "ThemeWatcher.h"
#include "Sha256.h"
#include "Layout.h"
#include "HudMotion.h"
#include "Assets.h"
#include "Presets.h"
#include "EquippedState.h"
#include <fstream>
#include <iostream>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* reason,std::source_location at=std::source_location::current()){
    if(!value)throw std::runtime_error(std::string(reason)+" at line "+std::to_string(at.line()));
    ++checks;
}
bool near(float a,float b){return std::abs(a-b)<.001f;}
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("Expected source directory");
    Options o;check(o.themeId==automaticTheme,"Default appearance follows the installed interface");
    o.themeId="norden";o.themeAdjustments["norden"]={true,{.2f,.6f,.9f},.7f,1.1f};o.selectorDock=4;o.selectorX=.3f;o.selectorY=.2f;o.fadeIdle=true;o.idleOpacity=.2f;
    const auto bytes=encodeOptions(o);check(encodeOptions(decodeOptions(bytes))==bytes,"Theme overrides, free placement, animation and fading persist exactly");
    o.themeId="oathvein";check(adjustments(o).iconScale==1,"Switching themes does not copy unrelated overrides");
    o.themeId="norden";check(adjustments(o).iconScale==1.1f,"Returning to a theme recalls its own customization");
    auto bad=o;bad.themeId="../outside";bad.themeAdjustments["norden"].frameOpacity=NAN;bad.selectorX=INFINITY;bad.selectorY=-4;bad.selectorDock=999;sanitizeOptions(bad);
    check(bad.themeId=="minimal"&&bad.themeAdjustments["norden"].frameOpacity==1&&bad.selectorX==.75f&&bad.selectorY==0&&bad.selectorDock==4,"Invalid custom options bounded without poisoning geometry");
    Themes themes;themes.load(std::filesystem::path(argv[1])/"assets/themes");check(themes.warnings().empty()&&themes.all().size()==18,"All sixteen external packs and two builtins load");
    for(const auto& id:{"minimal","silver","oathvein","norden","veldun","edge","untarnished","nordic","dear-diary-dark","amethyst","horizons","immortal","outlander","crimson-filigree","dwemer-aetherium","sovngarde-astral","imperial-parchment","daedric-oblivion"})check(themes.contains(id),"Expected theme is selectable");
    check(themes.get("missing-pack").id=="minimal","Missing selected theme has a readable fallback");
    for(const auto& t:themes.all()) {
        if(!t.atlas.empty()){const auto image=loadImageAsset(t.atlas);check(image.width==1024&&image.height==320,"Converted theme atlas meets renderer contract");}
        if(t.id!="minimal"&&t.id!="silver") {
            check(!t.assets.card.border.path.empty()&&!t.assets.diamond.border.path.empty()&&!t.assets.circle.border.path.empty()&&!t.assets.wheel.outerRim.path.empty()&&!t.assets.wheel.innerRim.path.empty()&&!t.assets.wheel.ringBg.path.empty()&&!t.recommendedJson.empty(),"Each bundled schema 3 pack carries sculpted SVG layers and a recommended look");
        }
    }

    const auto temp=std::filesystem::temp_directory_path()/"quickslot-theme-tests";std::filesystem::create_directories(temp);
    const auto write=[&](const char* dir,Json j){auto p=temp/dir;std::filesystem::create_directories(p);std::ofstream(p/"theme.json")<<j.dump();};
    write("a-valid",{{"schema",1},{"id","custom"},{"name","Custom"},{"palette",{{"accent",{10,20,30}}}}});
    write("b-duplicate",{{"schema",1},{"id","custom"},{"name","Duplicate"}});
    write("c-traversal",{{"schema",1},{"id","escape"},{"name","Bad path"},{"atlas","../outside.png"}});
    write("d-missing",{{"schema",1},{"id","missing"},{"name","Missing asset"},{"atlas","absent.png"}});
    write("e-version",{{"schema",999},{"id","future"},{"name","Future"}});
    write("f-color",{{"schema",1},{"id","bad-color"},{"name","Color"},{"palette",{{"accent",{256,0,0}}}}});
    themes.load(temp);check(themes.contains("custom")&&themes.get("custom").name=="Custom","First valid custom pack wins a deterministic duplicate");
    check(!themes.contains("escape")&&!themes.contains("future")&&!themes.contains("bad-color")&&themes.contains("missing"),"Bad manifests rejected while a missing optional image can fall back");
    check(themes.warnings().size()==5,"Invalid themes and missing optional assets report diagnostics");
    check(themes.get("custom").selection==themes.get("custom").text&&themes.get("custom").equipped==themes.get("custom").text&&themes.get("custom").atlasMargin==40,"Packs without the optional roles follow their text color and the default corner size");
    check((themePanel(themes.get("custom"))>>24)>=250&&(themePanelBorder(themes.get("custom"))&0xffffff)==(themes.get("custom").border&0xffffff),"Editor surfaces derive from the base palette");
    const auto roles=temp/"roles";std::filesystem::create_directories(roles);
    write("roles",{{"schema",2},{"id","roles"},{"name","Roles"},{"atlasMargin",500},{"palette",{{"text",{200,200,200}},{"selection",{1,2,3}},{"equipped",{4,5,6}},{"warning",{7,8,9}},{"depleted",{10,11,12}},{"poison",{13,14,15}},{"panel",{16,17,18,240}},{"panelBorder",{19,20,21}}}}});
    themes.load(roles.parent_path());const auto& styled=themes.get("roles");
    check(styled.selection==rgba(1,2,3)&&styled.equipped==rgba(4,5,6)&&styled.warning==rgba(7,8,9)&&styled.depleted==rgba(10,11,12)&&styled.poison==rgba(13,14,15),"Schema 2 packs set each optional role");
    {
        const auto bytes=[](std::string_view text){return std::vector<unsigned char>(text.begin(),text.end());};
        check(sha256(bytes(""))=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"&&sha256(bytes("abc"))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 matches its published vectors");
        check(sha256(bytes(std::string(1000,'a')))=="41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3","SHA-256 handles multi-block input");
        const auto marks=temp/"marks";std::filesystem::remove_all(marks);
        const auto pack=[&](const char* id,Json detect){auto folder=marks/id;std::filesystem::create_directories(folder);std::ofstream(folder/"theme.json")<<Json{{"schema",2},{"id",id},{"name",id},{"detect",detect}}.dump();};
        pack("first",Json::array({{{"file","Interface/hudmenu.swf"},{"sha256",sha256(bytes("first hud"))}}}));
        pack("second",Json::array({{{"file","Interface/hudmenu.swf"},{"sha256",sha256(bytes("second hud"))}},{{"file","Interface/startmenu.swf"},{"sha256",sha256(bytes("second start"))}}}));
        pack("unsafe",Json::array({{{"file","../Skyrim.ini"},{"sha256",sha256(bytes("x"))}}}));
        Themes marked;marked.load(marks);
        check(marked.contains("first")&&marked.contains("second")&&!marked.contains("unsafe"),"Detection signatures accept Data-relative paths only");
        unsigned reads=0;std::map<std::string,std::string> files{{"Interface/hudmenu.swf","second hud"}};
        const auto reader=[&](const std::string& path){++reads;const auto it=files.find(path);return it==files.end()?std::vector<unsigned char>{}:bytes(it->second);};
        check(marked.detect(reader)=="second"&&reads==1,"The pack whose interface file is installed is detected with one read per file");
        files={{"Interface/hudmenu.swf","patched by another mod"},{"Interface/startmenu.swf","second start"}};
        check(marked.detect(reader)=="second","Any one matching file identifies the interface");
        files={{"Interface/hudmenu.swf","unknown"}};check(marked.detect(reader).empty()&&marked.detect({}).empty(),"An unrecognized interface matches no pack");
        Themes bundled;bundled.load(std::filesystem::path(argv[1])/"assets/themes");unsigned signed_=0;for(const auto& t:bundled.all())signed_+=!t.detect.empty();
        check(bundled.warnings().empty()&&signed_==11,"Each bundled interface-matching pack carries a detection signature");
    }
    check(themePanel(styled)==rgba(16,17,18,240)&&themePanelBorder(styled)==rgba(19,20,21)&&styled.atlasMargin==120,"Schema 2 packs set editor surfaces and a bounded corner size");
    {
        // Schema 3: modular PNG/SVG assets, style controls, extends inheritance, Tier 1/2 detection and ThemeWatcher.
        const auto v3=temp/"v3";std::filesystem::remove_all(v3);
        const auto writePack=[&](const char* dir,const Json& j){auto p=v3/dir;std::filesystem::create_directories(p);std::ofstream(p/"theme.json")<<j.dump();return p;};
        const auto parentDir=writePack("parent",{
            {"schema",3},{"id","base-v3"},{"name","Base V3"},{"detectPriority",60},
            {"palette",{{"text",{240,235,220}},{"accent",{180,40,40}},{"charge",{100,190,255}}}},
            {"style",{{"cornerCut",8},{"borderWidth",1.5},{"shadowOpacity",0.5},{"glowOpacity",0.4},{"iconInset",0.18},{"editorRounding",6},{"accentLine",false}}},
            {"recommended",{{"wheelStyle",1},{"wheelThickness",0.5},{"wheelHighlight",3},{"equippedHighlight",1}}},
            {"assets",{
                {"card",{{"bg","card_bg.png"},{"border",{{"file","card_border.svg"},{"tint","border"},{"slice",{24,16,24,16}},{"rasterSize",256},{"outset",0.08}}}}},
                {"diamond",{{"bg","diamond.svg"},{"selected",{{"file","diamond_sel.svg"},{"tint","accent"},{"opacity",0.8}}}}},
                {"panel",{{"bg",{{"file","card_bg.png"},{"slice",24},{"sliceSize",14}}},{"border",{{"file","card_border.svg"},{"slice",24},{"sliceSize",14}}}}},
                {"hexagon",{{"bg","diamond.svg"},{"border","diamond.svg"},{"selected","diamond_sel.svg"}}},
                {"wheel",{{"ringBg","ring.png"},{"outerRim","ring.png"},{"innerRim","ring.png"},{"pointer",{{"file","ptr.svg"},{"tint","selection"}}}}},
                {"divider","rule.svg"},
                {"markers",{{"equipped",{{"file","eq.svg"},{"tint","equipped"}}}}},
                {"equipmentPanel",{{"bg","equip.png"}}},
                {"glyphs",Json::array({"diamond.svg","rule.svg"})}
            }},
            {"detect",Json::array({
                {{"file","Interface/hudmenu.swf"},{"sha256",sha256(std::vector<unsigned char>{'h','u','d'})}},
                {{"file","Interface/unique.ini"},{"exists",true}}
            })}
        });
        for(const char* name:{"card_bg.png","card_border.svg","diamond.svg","diamond_sel.svg","ring.png","ptr.svg","rule.svg","eq.svg","equip.png"})std::ofstream(parentDir/name)<<"x";
        const auto childDir=writePack("child",{
            {"schema",3},{"id","child-v3"},{"name","Child V3"},{"extends","base-v3"},{"detectPriority",80},
            {"palette",{{"accent",{40,180,90}}}},
            {"assets",{
                {"diamond",{{"selected",nullptr}}},
                {"circle",{{"border",{{"file","circle.svg"},{"tint","accent"},{"slice",20}}}}}
            }},
            {"detect",Json::array({{{"file","Interface/fontconfig.txt"},{"contains","Trattatello"}}})}
        });
        std::ofstream(childDir/"circle.svg")<<"x";
        writePack("cycle-a",{{"schema",3},{"id","cycle-a"},{"name","Cycle A"},{"extends","cycle-b"}});
        writePack("cycle-b",{{"schema",3},{"id","cycle-b"},{"name","Cycle B"},{"extends","cycle-a"}});
        writePack("orphan",{{"schema",3},{"id","orphan"},{"name","Orphan"},{"extends","does-not-exist"}});
        writePack("bad-tint",{{"schema",3},{"id","bad-tint"},{"name","Bad Tint"},{"assets",{{"divider",{{"file","a.svg"},{"tint","rainbow"}}}}}});
        writePack("bad-ext",{{"schema",3},{"id","bad-ext"},{"name","Bad Ext"},{"assets",{{"divider","a.bmp"}}}});
        Themes t3;t3.load(v3);
        check(t3.contains("base-v3")&&t3.contains("child-v3"),"Schema 3 base and child packs load");
        check(!t3.contains("cycle-a")&&!t3.contains("cycle-b")&&!t3.contains("orphan")&&!t3.contains("bad-tint")&&!t3.contains("bad-ext"),"Cyclic extends, missing parents, invalid tints and non-PNG/SVG assets are rejected");
        const auto& base=t3.get("base-v3");const auto& child=t3.get("child-v3");
        check(base.charge==rgba(100,190,255)&&near(base.style.cornerCut,8)&&near(base.style.borderWidth,1.5f)&&near(base.style.editorRounding,6)&&!base.style.accentLine,"Schema 3 style and charge color load");
        check(near(base.assets.card.border.outset,.08f)&&base.assets.glyphs.size()==2&&child.assets.glyphs.size()==2,"Schema 3 outset and glyphs array load and inherit cleanly");
        check(near(base.assets.panel.border.sliceSize,14.f)&&child.assets.panel.bg.path==parentDir/"card_bg.png"&&child.assets.hexagon.selected.path==parentDir/"diamond_sel.svg","Schema 3 sliceSize, panel and hexagon slots load and inherit cleanly");
        check(base.assets.card.border.tint==ThemeTint::border&&base.assets.card.border.slice==std::array<unsigned,4>{24,16,24,16}&&base.assets.card.border.rasterSize==256,"Schema 3 asset slice, tint and rasterSize load");
        check(child.accent==rgba(40,180,90)&&child.text==base.text&&child.charge==base.charge&&near(child.style.cornerCut,8),"Child inherits parent palette and style while overriding accent");
        check(child.assets.card.bg.path==parentDir/"card_bg.png"&&child.assets.wheel.ringBg.path==parentDir/"ring.png"&&child.assets.wheel.outerRim.path==parentDir/"ring.png"&&child.assets.wheel.innerRim.path==parentDir/"ring.png","Inherited asset paths resolve inside the parent folder");
        {
            Options cur;cur.themeId="child-v3";cur.fontId="ui";cur.keys[0]=42;
            check(!recommendedLookMatches(cur,child.recommendedJson),"Recommended look detects differences from defaults");
            const auto applied=mergeRecommendedLook(cur,child.recommendedJson);
            check(applied.wheelStyle==1&&near(applied.wheelThickness,.5f)&&applied.wheelHighlight==3&&applied.equippedHighlight==1,"Recommended look applies wheel and highlight settings");
            check(applied.themeId=="child-v3"&&applied.fontId=="ui"&&applied.keys[0]==42&&recommendedLookMatches(applied,child.recommendedJson),"Recommended look preserves theme, font and controls");
        }
        check(child.assets.diamond.bg.path==parentDir/"diamond.svg"&&!child.assets.diamond.selected,"Setting an inherited asset slot to null clears it");
        check(child.assets.circle.border.path==childDir/"circle.svg"&&child.assets.circle.border.slice==std::array<unsigned,4>{20,20,20,20},"Child adds new modular slots with scalar slice");
        const auto bytes=[](std::string_view text){return std::vector<unsigned char>(text.begin(),text.end());};
        std::map<std::string,std::string> res{{"Interface/hudmenu.swf","hud"},{"Interface/fontconfig.txt","map \"$TitleFont\" = \"Trattatello\""}};
        const auto reader=[&](const std::string& path){const auto it=res.find(path);return it==res.end()?std::vector<unsigned char>{}:bytes(it->second);};
        check(t3.detect(reader)=="base-v3","Tier 1 sha256 detection takes priority over Tier 2 contains/exists");
        res.erase("Interface/hudmenu.swf");res["Interface/unique.ini"]="x";
        check(t3.detect(reader)=="child-v3","Higher detectPriority wins among Tier 2 exists and contains matches");
        res.erase("Interface/fontconfig.txt");
        check(t3.detect(reader)=="base-v3","Tier 2 exists rule matches when higher-priority contains does not");

        ThemeWatcher watcher(.5);
        check(!watcher.poll(v3,10.0),"First poll primes the watcher without triggering");
        check(!watcher.poll(v3,10.2),"Polls within the throttle interval do not trigger");
        std::ofstream(childDir/"circle.svg")<<"updated-svg-content";
        check(!watcher.poll(v3,10.3),"Changes wait until the throttle interval elapses");
        check(watcher.poll(v3,10.6),"File edits trigger the watcher once the interval elapses");
        check(!watcher.poll(v3,11.2),"No further trigger fires when files stay unchanged");
    }

    Arsenal a;Entry e;e.form=0x1234;e.owner=0x14;e.unique=9;e.name="Tempered sword";a.groups[0].add(e);Options start;
    EditHistory history;history.begin(a,start);auto edited=start;
    history.prepare(a,edited);edited.scale=1.1f;history.finish(true,true,1);
    history.prepare(a,edited);edited.scale=1.2f;history.finish(true,true,1.1);
    history.prepare(a,edited);edited.scale=1.3f;history.finish(true,true,1.2);
    check(history.canUndo()&&history.undo(a,edited)&&edited.scale==1&&!history.canUndo(),"Dragging a slider undoes in one step, not one step per frame");
    history.prepare(a,edited);edited.themeId="edge";history.finish(true,false,2);
    history.prepare(a,edited);a.groups[0].entries.clear();history.finish(true,false,2.1);
    history.identity({e.form,e.owner,0xff000010,9,0});
    check(history.undo(a,edited)&&a.groups[0].entries.size()==1&&a.groups[0].entries[0].owner==0xff000010&&a.groups[0].entries[0].unique==0,"Undo a removal follows exact copy identity changes");
    check(edited.themeId=="edge","Undo of equipment does not erase the prior theme edit");
    history.prepare(a,edited);history.identity({e.form,0xff000010,0x14,0,42});a.groups[0].entries.clear();history.finish(true,false,3);
    check(history.undo(a,edited)&&a.groups[0].entries[0].unique==42,"Identity events during a native edit also update its pending checkpoint");
    check(history.revert(a,edited)&&edited.themeId==start.themeId&&a.groups[0].entries[0].unique==42,"Session revert restores original choices with current engine identities");
    check(history.undo(a,edited)&&edited.themeId=="edge"&&history.canRevert(),"Revert itself can be undone and reverted again");
    history.prepare(a,edited);edited.opacity=.8f;history.finish(false,false,4);
    for(unsigned i=0;i<40;++i){history.prepare(a,edited);edited.spacing=float(i);history.finish(true,false,5+i);}
    unsigned undoCount=0;while(history.undo(a,edited))++undoCount;check(undoCount==32,"Undo history is bounded during long editing sessions");
    SettingsAutosave autosave;check(!autosave.due(100),"No disk writes without edits");autosave.change(1);check(!autosave.due(1.2),"Continuous editing waits before writing");autosave.change(1.2);check(!autosave.due(1.4)&&autosave.due(1.56),"Each new edit extends the debounce");autosave.failed(1.56);check(autosave.pending()&&!autosave.due(2)&&autosave.due(4.57),"Write failure retains pending settings and backs off");autosave.saved();check(!autosave.pending()&&!autosave.due(100),"Successful write clears pending work");

    for(unsigned dock=0;dock<5;++dock)for(unsigned count:{3u,4u,8u})for(unsigned layout=0;layout<6;++layout)for(unsigned shape=0;shape<4;++shape)for(unsigned selector=0;selector<5;++selector)for(float corner:{0.f,.5f,1.f})for(const auto size:{Point{800,450},Point{1920,1080},Point{3440,1440},Point{3840,2160}}) {
        Options v;v.selectorDock=dock;v.groupCount=count;v.layout=layout;v.shape=shape;v.selector=selector;v.selectorSpacing=32;v.selectorGap=100;v.anchor=unanchored;v.positionX=v.positionY=v.selectorX=v.selectorY=corner;
        const auto g=hudGeometry(v,size.x,size.y);const float sx=g.origin.x+g.selectorOrigin.x*g.scale,sy=g.origin.y+(g.selectorOrigin.y-32)*g.scale;
        check(g.origin.x>=0&&g.origin.y>=0&&g.origin.x+g.width*g.scale<=size.x+.1f&&g.origin.y+g.height*g.scale<=size.y+.1f,"Widget fits at every anchor and dock");
        check(sx>=-.1f&&sy>=-.1f&&sx+g.selectorWidth*g.scale<=size.x+.1f&&sy+(g.selectorHeight+32)*g.scale<=size.y+.1f,"Selector fits independently at every anchor and dock");
    }
    ScrollMotion motion;Snapshot s;s.visible=true;s.browsing=true;s.rows.resize(4);Options motionOptions;
    check(motion.update(s,motionOptions,0)==0,"Opening snaps to current selection");s.preview=1;check(near(motion.update(s,motionOptions,1),1),"Scroll starts from the previous visual position");
    const auto halfway=motion.update(s,motionOptions,1.06);check(halfway>0&&halfway<1,"Scroll interpolates over real time");s.preview=2;
    check(near(motion.update(s,motionOptions,1.06),1+halfway),"Rapid scroll retargets continuously instead of queueing animation");
    check(near(motion.update(s,motionOptions,1.3),0),"Animation settles on current input target");s.preview=3;motion.update(s,motionOptions,2);motion.update(s,motionOptions,2.2);s.preview=0;
    check(near(motion.update(s,motionOptions,3),1),"Last-to-first wraps forward one step");motionOptions.animateScroll=false;check(motion.update(s,motionOptions,3.01)==0,"Reduced motion immediately snaps without changing input");
    motionOptions.animateScroll=true;s.editorPreview=true;check(motion.update(s,motionOptions,3.1)==0,"Editor previews do not inherit gameplay motion");
    IdleFade fade;Options fadeOptions;fadeOptions.fadeIdle=true;fadeOptions.fadeDelay=2;fadeOptions.idleOpacity=.25f;s.editorPreview=false;s.browsing=false;
    check(fade.update(s,fadeOptions,0)==1&&fade.update(s,fadeOptions,1)==1,"Idle delay starts fully visible");
    check(near(fade.update(s,fadeOptions,3),.25f),"Idle group fades to chosen opacity");s.browsing=true;check(fade.update(s,fadeOptions,3.1)==1,"Holding a hotkey restores full opacity immediately");s.browsing=false;check(fade.update(s,fadeOptions,3.2)==1,"Releasing restarts idle delay");s.inCombat=true;check(fade.update(s,fadeOptions,10)==1,"Combat can keep groups visible");s.inCombat=false;s.noticeUntil=15;check(fade.update(s,fadeOptions,11)==1,"Notifications reset the fade timer");
    Options hidden;setWidgetVisibility(hidden,WidgetVisibility::hide);hidden.fadeDelay=2;
    check(hidden.showIdle&&hidden.fadeIdle&&hidden.idleOpacity==0,"Hide when idle selects complete transparency");
    Snapshot activity;activity.visible=true;IdleFade visibility;
    check(visibility.update(activity,hidden,0)==1&&near(visibility.update(activity,hidden,2.175),.5f)&&visibility.update(activity,hidden,2.4)==0,"Hide delay fades smoothly to exactly zero");
    ++activity.interaction;check(visibility.update(activity,hidden,3)==1,"Using the same group and item reveals the widget");
    check(visibility.update(activity,hidden,5.4)==0,"Repeated selection gets a fresh idle delay");
    activity.inCombat=true;check(visibility.update(activity,hidden,6)==1&&visibility.update(activity,hidden,60)==1,"Combat overrides hiding until combat ends");
    activity.inCombat=false;check(visibility.update(activity,hidden,60.1)==1&&visibility.update(activity,hidden,62.4)==0,"Combat exit resumes the idle delay");
    activity.weaponsDrawn=true;check(visibility.update(activity,hidden,63)==0,"Drawn weapons do not override hiding by default");
    hidden.keepDrawnVisible=true;check(visibility.update(activity,hidden,63.1)==1,"Optional drawn-weapon visibility reveals the widget");
    activity.weaponsDrawn=false;check(visibility.update(activity,hidden,65.5)==0,"Sheathing allows complete hiding again");
    hidden.keepCombatVisible=false;activity.inCombat=true;check(visibility.update(activity,hidden,70)==0,"Combat override can be disabled independently");
    activity.browsing=true;check(visibility.update(activity,hidden,71)==1,"Browsing always reveals a hidden widget");
    activity.browsing=false;visibility.update(activity,hidden,71.1);activity.notice="Ready";activity.noticeUntil=99;
    check(visibility.update(activity,hidden,72)==1&&visibility.update(activity,hidden,74.4)==0,"A notification wakes the widget but cannot prevent complete fading");
    setWidgetVisibility(hidden,WidgetVisibility::selecting);
    check(visibility.update(activity,hidden,75)==0,"Only while selecting hides idle messages and combat state");
    activity.editorPreview=true;check(visibility.update(activity,hidden,76)==1,"Editor preview stays visible with hidden gameplay settings");
    activity.visible=false;check(visibility.update(activity,hidden,77)==0,"Blocking menus suppress every visibility mode");
    activity.visible=true;activity.editorPreview=false;activity.browsing=true;check(visibility.update(activity,hidden,78)==1,"Selection can reveal the widget after a menu closes");
    for(const auto mode:{WidgetVisibility::always,WidgetVisibility::fade,WidgetVisibility::hide,WidgetVisibility::selecting}) {
        setWidgetVisibility(hidden,mode);const auto restored=decodeOptions(encodeOptions(hidden));
        check(widgetVisibility(restored)==mode&&restored.keepDrawnVisible,"Visibility mode and drawn-weapon override survive settings reload");
        Options imported;importAppearance(imported,restored);check(widgetVisibility(imported)==mode&&imported.keepDrawnVisible,"Appearance presets include visibility settings");
    }
    auto prior=Json::parse(encodeOptions(hidden));prior.erase("keepDrawnVisible");check(!decodeOptions(prior.dump()).keepDrawnVisible,"Existing settings leave the new override off");
    {
        // Phase 3: Free per-tile overrides, Free equipment placement, per-role visibility, mirrorEquipment, anchors & TileSwapMotion.
        Options p3;p3.layout=freeLayout;p3.groupCount=4;
        p3.tilePlaces[0]={0,12,18,1.25f,1,1,2}; // Cards, Show label, Hide key
        p3.tilePlaces[1]={2,-16,20,1.f,4,2,1};  // Diamonds, Hide label, Show key
        p3.equipmentLayout=freeLayout;p3.showEquipment=true;p3.mirrorEquipment=true;
        p3.equipmentKeepCombatVisible=false;p3.equipmentKeepDrawnVisible=true;
        p3.equipmentPlaces[unsigned(EquipmentRole::rightHand)]={3,24,-30,1.2f};
        p3.equipmentPlaces[unsigned(EquipmentRole::consumable)]={5,-24,10,1.f};
        p3.equipmentRoles[unsigned(EquipmentRole::consumable)]=1; // When equipped
        p3.equipmentRoles[unsigned(EquipmentRole::voice)]=2;      // Hide
        p3.keyPosition=3;p3.badgePosition=2;
        const auto savedP3=encodeOptions(p3);
        const auto loadedP3=decodeOptions(savedP3);
        check(loadedP3.tilePlaces[0]==p3.tilePlaces[0]&&loadedP3.tilePlaces[1]==p3.tilePlaces[1],"Per-tile Free shape, label and key overrides persist");
        check(loadedP3.equipmentLayout==freeLayout&&loadedP3.equipmentPlaces==p3.equipmentPlaces&&loadedP3.equipmentRoles==p3.equipmentRoles,"Free equipment positions and per-role visibility persist");
        check(loadedP3.mirrorEquipment&&!loadedP3.equipmentKeepCombatVisible&&loadedP3.equipmentKeepDrawnVisible&&loadedP3.keyPosition==3&&loadedP3.badgePosition==2,"Mirrored equipment, combat/drawn rules and key/badge anchors persist");
        check(effectiveTileShape(loadedP3,0)==0&&effectiveTileShowLabel(loadedP3,0)&&!effectiveTileShowKey(loadedP3,0),"Tile 0 resolves its Card/label/key overrides");
        check(effectiveTileShape(loadedP3,1)==3&&!effectiveTileShowLabel(loadedP3,1)&&effectiveTileShowKey(loadedP3,1),"Tile 1 resolves its Diamond/label/key overrides");
        const auto geo=hudGeometry(loadedP3,1920,1080);
        check(tileSizeFor(geo.groups,0).x!=tileSizeFor(geo.groups,1).x,"Overridden Free tiles compute distinct footprints");

        EquippedDisplay eq;Entry sword;sword.name="Sword";eq.hands[0]=Row{sword,true,true};
        check(equipmentRows(eq,loadedP3)==2,"Per-role Hide and empty When-equipped roles omit rows cleanly");
        Entry pot;pot.kind=Kind::potion;pot.name="Potion";eq.consumable=Row{pot,true,false,{},3};
        check(equipmentRows(eq,loadedP3)==3,"When-equipped role appears once populated");

        Options mirrorList;mirrorList.equipmentStyle=0;mirrorList.equipmentLayout=1;
        const auto leftList=equipmentGeometry(mirrorList,1920,1080,4);
        mirrorList.mirrorEquipment=true;
        const auto rightList=equipmentGeometry(mirrorList,1920,1080,4);
        check(rightList.cells[0].icon.x>leftList.cells[0].icon.x&&rightList.cells[0].text.x<rightList.cells[0].icon.x,"Mirrored vertical equipment list places icons on the right");

        Options fromVert=mirrorList;freeEquipmentFrom(fromVert,1,1920,1080,4);
        check(fromVert.equipmentLayout==freeLayout&&fromVert.equipmentPlaces[0].anchor<unanchored,"Switching equipment to Free snapshots current role anchors");

        const std::array<std::array<float,2>,2> siblings{{{300.f,380.f},{620.f,700.f}}};
        const auto multiSnap=snapPlacementAxis((618.f-8.f)/(1920.f-16.f-80.f),80,1920,0,80,0,1920,16,siblings);
        check(multiSnap.aligned&&near(multiSnap.guide,620.f),"Multi-element snapping aligns against any sibling bounding box");

        Snapshot eqSnap;eqSnap.visible=true;eqSnap.equipment=eq;
        Options eqFadeOpts=loadedP3;eqFadeOpts.equipmentDim=true;eqFadeOpts.equipmentFadeDelay=1;eqFadeOpts.equipmentIdleOpacity=.2f;
        EquipmentFade eqFade;
        check(eqFade.update(eqSnap,eqFadeOpts,0)==1&&near(eqFade.update(eqSnap,eqFadeOpts,2),.2f),"Equipment fades after idle delay");
        eqSnap.inCombat=true;
        check(near(eqFade.update(eqSnap,eqFadeOpts,3),.2f),"Disabling equipmentKeepCombatVisible lets equipment stay dimmed in combat");
        eqSnap.weaponsDrawn=true;
        check(eqFade.update(eqSnap,eqFadeOpts,3.1)==1,"Enabling equipmentKeepDrawnVisible wakes dimmed equipment when weapons are drawn");

        TileSwapMotion swap;
        Entry a;a.form=0x100;a.name="Iron Sword";
        Entry b;b.form=0x200;b.name="Steel Mace";
        check(!swap.update(&a,1.0,true).hasPrevious,"Initial tile item does not play a swap transition");
        const auto mid=swap.update(&b,1.05,true);
        check(mid.hasPrevious&&mid.previous.form==0x100&&mid.progress>=0,"Changing tile item outside browsing starts a directional swap transition");
        const auto prog=swap.update(&b,1.12,true);
        check(prog.hasPrevious&&prog.progress>0&&prog.progress<1&&!swap.update(&b,1.30,true).hasPrevious,"Swap transition progresses and settles cleanly");

        // Widget pass: true 1:1 Squares, Hexagons, Frameless, Unified hotbar, label position, active tile emphasis, compact List & Souls/Action Bar presets.
        Options w;w.layout=0;w.shape=2;w.groupCount=4;w.showLabels=false;w.spacing=12;
        const auto sq=groupGeometry(w);
        check(near(sq.tileWidth,78.f)&&near(sq.tileHeight,78.f),"Squares use a true 1:1 footprint");
        w.unifiedBar=true;const auto bar=groupGeometry(w);
        check(near(bar.width,4*78.f),"Unified hotbar packs slots edge-to-edge regardless of spacing");
        w.unifiedBar=false;w.shape=4;const auto hex=groupGeometry(w);
        w.shape=5;const auto bare=groupGeometry(w);
        check(hex.tileWidth>0&&bare.tileWidth>0&&bare.tileHeight<hex.tileHeight,"Hexagons and Frameless compute distinct footprints");
        w.showLabels=true;w.labelPosition=2;const auto inside=groupGeometry(w);
        w.labelPosition=1;const auto above=groupGeometry(w);
        check(above.tileHeight>inside.tileHeight,"Inside labels fit inside the tile without adding external height");
        w.activeGroupBoost=.12f;
        const auto wLoaded=decodeOptions(encodeOptions(w));
        check(wLoaded.shape==5&&wLoaded.labelPosition==1&&near(wLoaded.activeGroupBoost,.12f)&&!wLoaded.unifiedBar,"Widget shape, label position, unified bar and emphasis persist");
        const auto souls=applyBuiltin(Options{},"builtin:souls-cross");
        const auto actionBar=applyBuiltin(Options{},"builtin:action-bar");
        check(souls.layout==5&&souls.shape==2&&souls.selector==4&&souls.activeGroupBoost>0,"Souls Cross preset configures compact squares cross and list selector");
        check(actionBar.layout==0&&actionBar.shape==2&&actionBar.unifiedBar&&actionBar.labelPosition==1,"Action Bar preset configures unified hotbar with labels above");
        // Size and Text size stay independent across widget, selector and equipment.
        Options ind;ind.scale=2.2f;ind.textScale=1.1f;ind.selectorScale=3.0f;ind.selectorTextScale=1.0f;ind.equipmentScale=1.8f;ind.equipmentTextScale=1.25f;
        check(near(ind.scale*hudTextFactor(ind),1.1f)&&near(ind.selectorScale*selectorTextFactor(ind),1.0f)&&near(ind.equipmentScale*equipmentTextFactor(ind),1.25f),
            "Element Size and Text size remain independent across widget, selector and equipment");
    }
    std::cout<<"PASS: "<<checks<<" live editing, identity-safe undo, autosave, theme loading, placement and motion checks.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

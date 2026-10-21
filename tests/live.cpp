#include "LiveEditor.h"
#include "Settings.h"
#include "Themes.h"
#include "Layout.h"
#include "HudMotion.h"
#include "Assets.h"
#include "Presets.h"
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
    Options o;check(o.themeId=="minimal","Default appearance has an explicit theme ID");
    o.themeId="norden";o.themeAdjustments["norden"]={true,{.2f,.6f,.9f},.7f,1.1f};o.selectorDock=4;o.selectorX=.3f;o.selectorY=.2f;o.fadeIdle=true;o.idleOpacity=.2f;
    const auto bytes=encodeOptions(o);check(encodeOptions(decodeOptions(bytes))==bytes,"Theme overrides, free placement, animation and fading persist exactly");
    o.themeId="oathvein";check(adjustments(o).iconScale==1,"Switching themes does not copy unrelated overrides");
    o.themeId="norden";check(adjustments(o).iconScale==1.1f,"Returning to a theme recalls its own customization");
    auto bad=o;bad.themeId="../outside";bad.themeAdjustments["norden"].frameOpacity=NAN;bad.selectorX=INFINITY;bad.selectorY=-4;bad.selectorDock=999;sanitizeOptions(bad);
    check(bad.themeId=="minimal"&&bad.themeAdjustments["norden"].frameOpacity==1&&bad.selectorX==.75f&&bad.selectorY==0&&bad.selectorDock==4,"Invalid custom options bounded without poisoning geometry");
    Themes themes;themes.load(std::filesystem::path(argv[1])/"assets/themes");check(themes.warnings().empty()&&themes.all().size()==9,"All seven external packs and two builtins load");
    for(const auto& id:{"minimal","silver","oathvein","norden","veldun","edge","untarnished","nordic","dear-diary-dark"})check(themes.contains(id),"Expected theme is selectable");
    check(themes.get("missing-pack").id=="minimal","Missing selected theme has a readable fallback");
    for(const auto& t:themes.all())if(!t.atlas.empty()){const auto image=loadImageAsset(t.atlas);check(image.width==1024&&image.height==320,"Converted theme atlas meets renderer contract");}

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
    check(history.revert(a,edited)&&edited.themeId=="minimal"&&a.groups[0].entries[0].unique==42,"Session revert restores original choices with current engine identities");
    check(history.undo(a,edited)&&edited.themeId=="edge"&&history.canRevert(),"Revert itself can be undone and reverted again");
    history.prepare(a,edited);edited.opacity=.8f;history.finish(false,false,4);
    for(unsigned i=0;i<40;++i){history.prepare(a,edited);edited.spacing=float(i);history.finish(true,false,5+i);}
    unsigned undoCount=0;while(history.undo(a,edited))++undoCount;check(undoCount==32,"Undo history is bounded during long editing sessions");
    SettingsAutosave autosave;check(!autosave.due(100),"No disk writes without edits");autosave.change(1);check(!autosave.due(1.2),"Continuous editing waits before writing");autosave.change(1.2);check(!autosave.due(1.4)&&autosave.due(1.56),"Each new edit extends the debounce");autosave.failed(1.56);check(autosave.pending()&&!autosave.due(2)&&autosave.due(4.57),"Write failure retains pending settings and backs off");autosave.saved();check(!autosave.pending()&&!autosave.due(100),"Successful write clears pending work");

    for(unsigned dock=0;dock<5;++dock)for(unsigned count:{3u,4u,8u})for(unsigned layout=0;layout<6;++layout)for(unsigned shape=0;shape<4;++shape)for(unsigned selector=0;selector<5;++selector)for(float corner:{0.f,.5f,1.f})for(const auto size:{Point{800,450},Point{1920,1080},Point{3440,1440},Point{3840,2160}}) {
        Options v;v.selectorDock=dock;v.groupCount=count;v.layout=layout;v.shape=shape;v.selector=selector;v.selectorSpacing=32;v.selectorGap=100;v.positionX=v.positionY=v.selectorX=v.selectorY=corner;
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
    std::cout<<"PASS: "<<checks<<" live editing, identity-safe undo, autosave, theme loading, placement and motion checks.\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

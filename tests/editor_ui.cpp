#include "Editor.h"
#include "EditorFixture.h"
#include "UiProbe.h"
#include "Layout.h"
#include <iostream>
#include <source_location>
using namespace quickslot;
unsigned checks=0;
void check(bool value,const char* why,std::source_location at=std::source_location::current()){if(!value)throw std::runtime_error(std::string(why)+" at line "+std::to_string(at.line()));++checks;}
int main(int argc,char** argv)try {
    if(argc!=2)return 2;
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={1600,900};io.DeltaTime=1.f/60;
    ImGui::GetCurrentContext()->TestEngineHookItems=true;
    Hud hud;hud.initialize(std::string(argv[1])+"/assets/fonts");unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    Editor editor;auto state=fixture::editor();editor.beginSession(state);std::vector<EditCommand> commands;
    EditHistory history;std::uint64_t historySession=0;
    const auto frame=[&](const std::vector<EditorInput>& events=std::vector<EditorInput>{}) {
        if(historySession!=state.session){historySession=state.session;history.begin(state.draft,state.options);}
        editor.beginSession(state);editor.input(events);probe::clear();ImGui::NewFrame();commands=editor.draw(state,hud,1);ImGui::Render();
        for(const auto& command:commands)check(fixture::apply(state,command,history),"UI produces applicable command");
    };
    const auto mouse=[&](ImVec2 point){frame({{EditorInputType::move,0,false,point.x-io.MousePos.x,point.y-io.MousePos.y}});};
    const auto click=[&](const std::string& label,std::size_t index=0){
        // Combo menus clip rows; scroll the open popup until the requested
        // item is submitted before asking for its hit rectangle.
        for(unsigned attempt=0;attempt<16&&probe::find(label).empty();++attempt) {
            auto& popups=ImGui::GetCurrentContext()->OpenPopupStack;
            if(popups.empty()||!popups.back().Window)break;
            mouse(popups.back().Window->InnerClipRect.GetCenter());frame({{EditorInputType::wheel,0,false,0,-2}});frame();
        }
        // Exercise real scrolling when taller fonts/controls put an item below
        // its child window. Clicking an off-screen rectangle is not a UI test.
        for(unsigned attempt=0;attempt<24;++attempt) {
            const auto item=probe::one(label,index);auto* window=ImGui::FindWindowByName(item.window.c_str());
            if(!window||window->InnerClipRect.Contains(item.rect.GetCenter()))break;
            if(attempt==23)throw std::runtime_error("Cannot scroll to "+label);
            const auto clip=window->InnerClipRect;const float wheel=item.rect.GetCenter().y>clip.Max.y?-2.f:2.f;
            mouse(clip.GetCenter());frame({{EditorInputType::wheel,0,false,0,wheel}});frame();
        }
        for(unsigned settle=0;settle<4;++settle){const auto target=probe::one(label,index).rect.GetCenter();mouse(target);const auto next=probe::one(label,index).rect.GetCenter();if(std::abs(next.x-target.x)<.1f&&std::abs(next.y-target.y)<.1f)break;}
        frame({{EditorInputType::button,0,true}});frame({{EditorInputType::button,0,false}});frame();
    };
    const auto key=[&](unsigned scan){frame({{EditorInputType::key,scan,true}});frame({{EditorInputType::key,scan,false}});};
    frame();frame();
    check(probe::one("Close").rect.Max.y<900,"Apply is visible at normal resolution");
    click("##filter");click("Spells");click("##search");for(const auto ch:std::string("healing"))frame({{EditorInputType::character,unsigned(ch)}});frame();
    check(probe::find("##candidate").size()==1,"Search filters the rendered catalog");
    click("##candidate");click("Add left");
    check(state.draft.groups[0].entries.back().name=="Healing"&&state.draft.groups[0].entries.back().hand==Hand::left,"Click adds a known spell directly to the chosen hand");
    click("Change hand"); // First row is selected by default.
    check(state.draft.groups[0].entries[0].hand==Hand::left,"Saved row hand changes without equipment calls");
    click("Remove");check(state.draft.groups[0].entries.size()==3,"Remove updates editor groups");
    click("Undo");check(state.draft.groups[0].entries.size()==4,"Undo restores removed shortcut from the real editor");
    click("Remove");
    click("##Group name");frame({{EditorInputType::key,29,true},{EditorInputType::key,30,true}});frame({{EditorInputType::key,30,false},{EditorInputType::key,29,false}});
    for(const auto ch:std::string("Combat"))frame({{EditorInputType::character,unsigned(ch)}});key(28);
    check(state.options.names[0]=="Combat","Typing renames a group through settings commands");
    click("1##group binding");
    for(const auto scan:{59u,65u,66u}){key(scan);check(state.options.keys[0]==scan,"Group binding accepts F1, F7 and F8");click((keyLabel(scan)+"##group binding").c_str());}
    key(60);check(state.options.keys[0]==60,"Binding capture accepts an unreserved function key");
    click("F2##group binding");key(3);check(state.options.keys[0]==60,"Duplicate binding is rejected");key(1);check(state.open,"Escape cancels key capture without closing editor");
    click("+ Add group");check(state.options.groupCount==4,"Add group reveals a fourth group");
    click("F2   Combat\n     3 / 16");
    click("Appearance");click("Groups");click("##layout");click("Diamond");check(state.options.layout==2,"Layout picker applies diamond");
    click("Controls");click("Keyboard");click("Shift + \\##editor binding");key(66);check(state.options.editorKey==66,"Editor shortcut accepts F8");click("F8##editor binding");
    key(68);check(state.options.editorKey==68&&state.options.editorModifiers==0,"Unmodified editor shortcut can be assigned");
    click("F10##editor binding");
    frame({{EditorInputType::key,54,true}});check(state.options.editorKey==68,"A modifier alone waits for the shortcut key");
    frame({{EditorInputType::key,43,true},{EditorInputType::key,43,false},{EditorInputType::key,54,false}});frame();
    check(state.options.editorKey==43&&state.options.editorModifiers==keyModifier::shift&&state.open,"Capture remembers right Shift even when the chord is released before rendering");
    key(43);check(state.open,"Bare backslash does not close the editor");
    click("Undo");check(state.options.editorKey==68&&state.options.editorModifiers==0,"Undo restores the whole previous editor binding");
    click("Clear##editor binding");check(state.options.editorKey==0&&state.options.editorModifiers==0,"Direct shortcut and modifiers can be cleared");
    click("Appearance");click("Groups");click("##shape");click("Circles");check(state.options.shape==1,"Tile shape is independent");
    click("Selector");click("##selector");click("Ring");check(state.options.selector==2,"Selector layout is independent");
    click("Show selector hints");check(state.options.showHints,"Selector hints can be enabled from the editor");
    click("Show selector hints");check(!state.options.showHints,"Minimal selector can hide hints again");
    click("##selector");click("Vertical strip");check(state.options.selector==3,"Vertical selector applies immediately");
    click("##selector");click("List");check(state.options.selector==4,"List selector applies immediately");
    check(probe::find("##selector shape").empty(),"List uses cards without an ineffective shape control");
    click("Undo");check(state.options.selector==3,"Selector layout changes support Undo");
    click("##selector");click("Ring");
    click("Groups");click("##shape");click("Cards");click("Show card item names");
    check(!state.options.showCardNames,"Card item names can be hidden independently");
    click("Undo");check(state.options.showCardNames,"Card names support live Undo");
    click("##shape");click("Circles");
    const auto beforeKeys=state.options.keys;const auto beforeGroups=state.draft.groups;
    click("Groups");click("Cross layout");check(state.options.layout==5&&state.options.shape==2&&!state.options.showLabels,"Cross preset applies the compact panel layout");
    check(state.options.keys==beforeKeys&&state.options.groupCount==4&&state.draft.groups[0].entries.size()==beforeGroups[0].entries.size(),"Preset does not disturb bindings or equipment");
    click("Diamond layout");check(state.options.shape==3&&state.options.layout==2&&state.options.selectorShape==3,"Diamond preset changes tile and selector geometry");
    click("Circle layout");check(state.options.shape==1&&state.options.layout==4&&!state.options.showLabels,"Circle preset enables compact packing");
    click("Style");click("##theme");click("Skyrim silver");check(state.options.themeId=="silver"&&state.options.shape==1,"Theme remains independent of geometry");
    click("Selector");click("##selector shape");click("Squares");check(state.options.selectorShape==2,"Selector shapes can be chosen independently");
    click("Groups");click("Show group names");check(state.options.showLabels,"Names can be restored without changing theme");
    click("Style");click("Key icons");check(!state.options.keyIcons,"Key artwork can switch to text caps");
    click("Key icons");check(state.options.keyIcons,"Key artwork can be restored");
    check(probe::find("##item icon source").empty(),"Removed inventory source selector is not offered");
    click("##item icon color");click("White");check(state.options.itemIconColor==2,"White tint applies immediately");
    click("Undo");check(state.options.itemIconColor==1,"Atlas tint change is undoable");
    click("##item icon color");click("Theme");check(state.options.itemIconColor==1,"Theme tint can be chosen");
    const bool colored=state.options.coloredMagic;click("Tint magic icons");check(state.options.coloredMagic!=colored,"Magic coloring is independent of the frame theme");
    for(const auto& pair:std::vector<std::pair<const char*,const char*>>{{"Oathvein","oathvein"},{"Vel'dun","veldun"},{"Norden","norden"},{"Edge","edge"},{"Untarnished","untarnished"},{"Nordic","nordic"},{"Dear Diary Dark","dear-diary-dark"}}) {
        click("##theme");click(pair.first);check(state.options.themeId==pair.second,"External theme is selectable through the real ImGui picker");
    }
    click("##theme");click("Norden");click("Custom accent");check(adjustments(state.options).customAccent,"Theme customization applies immediately");
    click("##theme");click("Edge");check(!adjustments(state.options).customAccent,"A new theme starts with its own defaults");
    click("##theme");click("Norden");check(adjustments(state.options).customAccent,"Returning to a theme restores its prior overrides");
    click("Reset theme");check(!adjustments(state.options).customAccent,"Theme reset removes only current overrides");
    click("Undo");check(adjustments(state.options).customAccent,"Theme reset is undoable");
    click("Visibility");click("Show keys");check(!state.options.showKeys,"Widget key visibility changes live");click("Undo");check(state.options.showKeys,"Key visibility is undoable");
    click("Fade while idle");check(state.options.fadeIdle,"Idle fade can be enabled");
    click("Selector");click("Animate scrolling");check(!state.options.animateScroll,"Animation can be disabled immediately");
    click("##selector dock");click("Independent");check(state.options.selectorDock==4,"Selector can be detached through settings");
    click("Position");check(!probe::find("Drag widget").empty(),"Full-screen placement opens");
    const auto dragArea=probe::one("Drag widget").rect;mouse(dragArea.GetCenter());frame({{EditorInputType::button,0,true}});
    frame({{EditorInputType::move,0,false,-60,-40}});frame({{EditorInputType::button,0,false}});frame();
    check(state.options.positionX<1&&state.options.positionY<1,"Dragging changes actual HUD anchor");
    const auto widgetPosition=ImVec2{state.options.positionX,state.options.positionY};
    click("Selector");check(!probe::find("Drag selector").empty(),"Selector has its own full-screen drag target");
    const auto selectorPosition=ImVec2{state.options.selectorX,state.options.selectorY};mouse(probe::one("Drag selector").rect.GetCenter());frame({{EditorInputType::button,0,true}});
    frame({{EditorInputType::move,0,false,-70,-35}});frame({{EditorInputType::button,0,false}});frame();
    check(state.options.selectorX<selectorPosition.x&&state.options.selectorY<selectorPosition.y&&state.options.positionX==widgetPosition.x&&state.options.positionY==widgetPosition.y,"Dragging selector preserves the widget anchor");
    click("Grid");click("Snap to grid");check(state.options.placementGrid&&state.options.placementSnap,"Placement grid and snapping toggle independently");
    check(!probe::find("X").empty()&&!probe::find("Y").empty(),"Placement exposes both axes");
    mouse(probe::one("Drag selector").rect.GetCenter());frame({{EditorInputType::button,0,true}});
    frame({{EditorInputType::move,0,false,-101,-77}});frame({{EditorInputType::button,0,false}});frame();
    const auto snapped=hudGeometry(state.options,io.DisplaySize.x,io.DisplaySize.y);const float centerX=snapped.origin.x+(snapped.selectorOrigin.x+snapped.selectorWidth*.5f)*snapped.scale;
    const auto gridX=(centerX-io.DisplaySize.x*.5f)/placementGridStep(io.DisplaySize.y);
    check(std::abs(gridX-std::round(gridX))<.001f,"Dragging snaps selector center to actual displayed grid");
    click("Back");click("Equipment");
    // A native drag/drop uses the real item rectangles and payload implementation.
    const auto rows=probe::find("##entry");check(rows.size()==3,"Three draft rows available for drag test");
    const auto oldFirst=state.draft.groups[0].entries[0];mouse(rows[0].rect.GetCenter());frame({{EditorInputType::button,0,true}});
    const auto target=rows[2].rect.GetCenter();mouse({target.x,target.y-2});frame();frame({{EditorInputType::button,0,false}});frame();
    check(state.draft.groups[0].entries[2].same(oldFirst),"Dragging a row reorders the actual draft");
    auto moving=state.draft.groups[0].entries[2];
    auto row=probe::one("##entry",2).rect.GetCenter();auto groupTarget=probe::one("2   Offhand\n     1 / 16").rect.GetCenter();
    mouse(row);frame({{EditorInputType::button,0,true}});mouse(groupTarget);frame();frame({{EditorInputType::button,0,false}});frame();
    check(state.draft.active==1&&state.draft.groups[1].entries.back().same(moving),"Dropping a shortcut on another group moves it");
    auto choice=probe::one("##candidate").rect.GetCenter();groupTarget=probe::one("3   Magic\n     2 / 16").rect.GetCenter();
    mouse(choice);frame({{EditorInputType::button,0,true}});mouse(groupTarget);frame();frame({{EditorInputType::button,0,false}});frame();
    check(state.draft.groups[2].entries.back().name=="Healing","Dragging an inventory choice onto a group adds it");
    const auto beforeRevert=state.options.themeId;click("Revert session");
    check(state.options.themeId=="minimal"&&state.options.names[0]=="Arsenal"&&state.draft.groups[0].entries.size()==3,"Revert restores the opening groups, names and settings");
    click("Undo");check(state.options.themeId==beforeRevert&&state.options.names[0]=="Combat","Session revert can itself be undone");
    click("Close");check(!state.open,"Save closes editor and submits settings");
    state=fixture::editor();state.session=2;frame();frame();key(1);check(!state.open,"Escape closes with changes kept");
    state=fixture::editor();state.session=3;state.draft.groups[0]={};fixture::rows(state);io.DisplaySize={800,600};frame();frame();
    check(probe::one("Close").rect.Max.y<=600,"Editor actions remain accessible at 800x600");
    check(probe::one("Change hand").disabled,"Empty group cannot change a nonexistent entry's hand");
    io.DisplaySize={1600,900};unsigned session=4;
    for(const unsigned shift:{42u,54u}) {
        state=fixture::editor();state.session=session++;frame();frame();
        key(43);check(state.open,"Bare backslash leaves a freshly opened editor visible");
        frame({{EditorInputType::key,shift,true},{EditorInputType::key,43,true},{EditorInputType::key,43,false},{EditorInputType::key,shift,false}});
        check(!state.open,"Either Shift + backslash closes once, even with all events in one frame");
    }
    state=fixture::editor();state.session=session++;frame();frame();click("##search");frame();
    check(io.WantTextInput,"Search owns text input before typing a shortcut character");
    frame({{EditorInputType::key,42,true},{EditorInputType::key,43,true},{EditorInputType::character,'|'},
           {EditorInputType::key,43,false},{EditorInputType::key,42,false}});frame();
    check(state.open,"Typing a pipe into text input does not close the editor");
    click("Close");
    for(const auto& [filter,query,kind]:std::vector<std::tuple<const char*,const char*,Kind>>{
        {"Potions","Potion of Healing",Kind::potion},{"Food","Vegetable Soup",Kind::food},
        {"Poisons","Weak Poison",Kind::poison},{"Scrolls","Scroll of Fireball",Kind::scroll},
        {"Powers & shouts","Battle Cry",Kind::power},{"Powers & shouts","Night Eye",Kind::lesserPower},
        {"Powers & shouts","Unrelenting Force",Kind::shout},{"Ammo","Steel Arrow",Kind::ammo}}) {
        state=fixture::editor();state.session=session++;state.draft.groups[0]={};fixture::rows(state);frame();frame();
        click("##filter");click(filter);click("##search");
        for(const auto ch:std::string(query))frame({{EditorInputType::character,unsigned(ch)}});frame();
        check(probe::find("##candidate").size()==1,"New category filter and search isolate the requested item");
        click("##candidate");click(kind==Kind::scroll?"Add left":"Add to group");
        const auto& entry=state.draft.groups[0].entries.at(0);
        check(entry.kind==kind&&entry.owner==0&&entry.unique==0,"New shortcuts add without assigning physical-copy identities");
        check(entry.hand==(kind==Kind::scroll?Hand::left:Hand::right),"Scroll uses chosen hand; consumable/voice uses canonical target");
        check(probe::one("Change hand").disabled==(kind!=Kind::scroll),"Hand editing is offered only for hand equipment");
        if(isStack(entry))check(state.rows[0][0].quantity==7,"Editor preview retains stack quantity");
        click("Remove");check(state.draft.groups[0].entries.empty(),"New category shortcuts can be removed");
        click("Undo");check(state.draft.groups[0].entries.size()==1,"Undo restores the new shortcut category");
    }
    state=fixture::editor();state.session=session++;frame();frame();
    const auto initialKeys=state.options.keys;
    click("Remove group");check(state.options.groupCount==2&&state.draft.groups[0].entries.empty(),"Real editor removes a populated active group");
    check(state.options.keys==initialKeys,"Group removal leaves other hotkeys intact");
    click("Undo");check(state.options.groupCount==3&&state.draft.active==0&&state.draft.groups[0].entries.size()==3,"Undo restores removed group's entries and focus");
    click("Group down");check(state.options.groupOrder[1]==0&&state.options.keys==initialKeys,"Group ordering changes independently of keys");
    click("Clear group");check(state.draft.groups[0].entries.empty()&&state.options.groupCount==3,"Clear keeps the group available");click("Undo");
    click("Shortcuts");click("Add smart slot");check(state.draft.groups[0].entries.back().kind==Kind::smartPotion,"Smart slot is created through actual editor controls");
    click("Health restoration · lowest");click("Choose");
    click("Highest strength");
    check(state.draft.groups[0].entries.back().policy==1,"Editing smart policy applies immediately");
    click("Rare crafted potion");check(state.draft.groups[0].entries.back().exclusions.size()==1,"Excluding a bottle changes the live rule");
    click("Resource");click("Stamina");check(state.draft.groups[0].entries.back().resource==2,"Resource change updates the selected smart rule");
    click("Effect");click("Regeneration");check(state.draft.groups[0].entries.back().effect==1,"Regeneration has a separate live rule");
    click("Clear exclusions");check(state.draft.groups[0].entries.back().exclusions.empty(),"Exclusions can be reset");
    click("Undo");check(state.draft.groups[0].entries.back().exclusions.size()==1,"Exclusion changes can be undone");
    click("New combination");click("##right component");click("Imperial Sword (Superior) · Arsenal");
    click("##left component");click("Steel Dagger · Offhand");click("Add combination");
    check(state.draft.groups[0].entries.back().kind==Kind::combo&&state.draft.groups[0].entries.back().parts.size()==2,"Editor creates a two-hand combination from saved copies");
    check(state.draft.groups[0].entries.back().parts[0].hand==Hand::right&&state.draft.groups[0].entries.back().parts[1].hand==Hand::left,"Combination captures explicit hand assignments");
    click("Equipment combination");const auto rightCopy=state.draft.groups[0].entries.back().parts[0].unique;
    click("Swap hands");check(state.draft.groups[0].entries.back().parts[1].unique==rightCopy&&state.draft.groups[0].entries.back().parts[1].hand==Hand::left,"Swap moves exact components and updates hand targets live");
    click("Remove shortcut");check(state.draft.groups[0].entries.back().kind==Kind::smartPotion,"Special shortcuts can be removed without deleting inventory");
    click("Undo");check(state.draft.groups[0].entries.back().kind==Kind::combo,"Undo restores both exact combination components");
    click("Controls");click("General");click("Use consumables on release");check(state.options.consumableOnRelease,"Optional release-to-use changes live");
    click("Skip unavailable items");check(state.options.skipEmpty,"Skip empty is configurable");
    click("Add from Inventory / Magic");check(!state.options.menuCapture,"Menu capture can be disabled for key conflicts");
    click("##selection clock");click("Slow motion");check(state.options.selectionSpeed==.5f,"Slow motion is selectable");
    click("##selection clock");click("Pause");check(state.options.selectionSpeed==0,"Full pause is selectable");
    click("##selection clock");click("Full speed");check(state.options.selectionSpeed==1,"Full speed is selectable");
    click("Undo");check(state.options.selectionSpeed==0,"Undo restores pause mode");
    state=fixture::editor();state.session=session++;io.DisplaySize={1920,1080};frame();frame();
    const auto normalTab=probe::one("Equipment").rect.GetSize();
    const auto normalWindow=ImGui::FindWindowByName("Quickslot")->Size;
    io.DisplaySize={3840,2160};frame();frame();
    const auto largeTab=probe::one("Equipment").rect.GetSize();
    const auto largeWindow=ImGui::FindWindowByName("Quickslot")->Size;
    check(std::abs(largeWindow.x-normalWindow.x*2)<1&&std::abs(largeWindow.y-normalWindow.y*2)<1,"4K doubles the editor window's size");
    check(std::abs(largeTab.x-normalTab.x*2)<1&&std::abs(largeTab.y-normalTab.y*2)<3,"4K scales font and hit target height together");
    check(probe::one("Close").rect.Max.y<2160,"4K editor footer stays reachable");
    const auto stable=largeWindow;for(unsigned i=0;i<5;++i)frame();
    check(ImGui::FindWindowByName("Quickslot")->Size.x==stable.x&&probe::one("Equipment").rect.GetSize().y==largeTab.y,"Repeated frames do not compound UI scaling");
    click("Controls");click("##selection clock");click("Pause");check(state.options.selectionSpeed==0,"4K popup hit targets select pause correctly");
    check(probe::find("Editor size").empty(),"Editor sizing is automatic with no resize slider");
    click("Appearance");
    click("Position");
    for(const auto corner:std::array<ImVec2,4>{{{0,0},{3839,0},{0,2159},{3839,2159}}}) {
        mouse(probe::one("Drag widget").rect.GetCenter());frame({{EditorInputType::button,0,true}});
        mouse(corner);frame({{EditorInputType::button,0,false}});frame();
        const auto geo=hudGeometry(state.options,3840,2160);
        check(corner.x==0?geo.origin.x<9:geo.origin.x+geo.width*geo.scale>3831,"Widget drag reaches horizontal screen extents at 4K");
        check(corner.y==0?geo.origin.y<9:geo.origin.y+geo.height*geo.scale>2151,"Widget drag reaches vertical screen extents at 4K");
    }
    click("Back");click("Shortcuts");check(!probe::find("Add smart slot").empty(),"4K smart editor remains usable");
    io.DisplaySize={1280,720};frame();frame();
    check(probe::one("Close").rect.Max.y<720&&ImGui::FindWindowByName("Quickslot")->Pos.x>=0,"Fixed editor remains usable at 720p");
    click("Close");check(!state.open,"Largest 4K editor remains interactive");
    state=fixture::editor();state.session=session++;state.draft.presetIdentity="ui-character";io.DisplaySize={1920,1080};frame();frame();
    click("Presets");click("##preset name");for(const auto ch:std::string("Combat backup"))frame({{EditorInputType::character,unsigned(ch)}});
    click("##export scope");click("Complete setup");click("Export preset");check(state.presets.size()==1,"Export command saves named preset");
    click("Update preset");check(state.presets.size()==1,"Updating a preset retains one list entry");
    click("Equipment");click("Clear group");check(state.draft.groups[0].entries.empty(),"Fixture changed before preset restore");
    click("Presets");click("##preset file");click("Combat backup");click("Appearance only");click("Import preset");
    check(state.draft.groups[0].entries.size()==3,"Complete preset restores exact saved group");
    click("Undo");check(state.draft.groups[0].entries.empty(),"Preset import is a single undoable edit");
    for(const auto screen:std::array<ImVec2,3>{{{1280,720},{1920,1080},{3840,2160}}}) {
        state=fixture::editor();state.session=session++;io.DisplaySize=screen;frame();frame();
        click("Shortcuts");click("New hand action");click("Add action");
        check(state.draft.groups[0].entries.back().kind==Kind::unarmed,"Unarmed can be added at every editor scale");
        click("Action");click("Empty right hand");click("Add action");
        check(state.draft.groups[0].entries.back().same(emptyHandEntry(Hand::right)),"Action picker adds an empty right hand");
        click("Action");click("Empty left hand");click("Add action");
        check(state.draft.groups[0].entries.back().same(emptyHandEntry(Hand::left)),"Action picker adds an empty left hand");
        click("Unarmed");click("Remove shortcut");
        check(std::none_of(state.draft.groups[0].entries.begin(),state.draft.groups[0].entries.end(),[](const auto& e){return e.kind==Kind::unarmed;}),"Hand actions can be removed from shortcuts list");
        click("Undo");check(state.draft.groups[0].entries[3].kind==Kind::unarmed,"Undo restores an action in its original slot");
        click("New combination");click("##right component");click("Imperial Sword (Superior) · Arsenal");
        click("##left component");click("Empty left hand");click("Add combination");
        check(state.draft.groups[0].entries.back().parts[1].same(emptyHandEntry(Hand::left)),"Combination picker offers empty hand without an inventory entry");
        click("Equipment combination");click("Swap hands");
        const auto& pair=state.draft.groups[0].entries.back();
        check(pair.parts[0].same(emptyHandEntry(Hand::right))&&pair.parts[0].name=="Empty right hand"&&pair.parts[1].hand==Hand::left,"Swapping a combination keeps empty-hand target and label aligned");
    }
    state=fixture::editor();state.session=session++;io.DisplaySize={1920,1080};frame();frame();
    click("Shortcuts");click("New hand action");click("Add action");click("Unarmed");
    click("Action");click("Empty left hand");
    check(state.draft.groups[0].entries.size()==4&&state.draft.groups[0].entries.back().same(emptyHandEntry(Hand::left)),"Editing action updates its existing slot immediately");
    click("Undo");check(state.draft.groups[0].entries.back().kind==Kind::unarmed,"Undo restores the prior hand action");
    state=fixture::editor();state.session=session++;state.controller=true;io.DisplaySize={1920,1080};frame();frame();
    const auto padKey=[&](PadButton b){frame({{EditorInputType::pad,unsigned(b),true}});frame({{EditorInputType::pad,unsigned(b),false}});frame();};
    // Establish focus through a real click, then navigate/activate with the pad.
    click("Equipment");padKey(PadButton::right);
    check(ImGui::GetCurrentContext()->NavId==probe::one("Shortcuts").id,"D-pad moves between actual top-level tabs");
    padKey(PadButton::a);check(!probe::find("Add smart slot").empty(),"A activates focused tab without mouse input");
    padKey(PadButton::right);padKey(PadButton::right);padKey(PadButton::a);
    check(!probe::find("General").empty(),"Controller reaches Controls tab");
    click("General");click("Activate with group hotkey");check(!state.options.voiceHotkey,"Voice activation can be disabled live");
    click("Undo");check(state.options.voiceHotkey,"Undo restores voice activation");
    click("Activate powers / shouts on release");check(state.options.voiceOnRelease,"Release casting is configurable");
    click("Controller");click("Button icons");click("PlayStation");check(state.options.controllerStyle==1,"Controller glyph family changes live");
    click("Browse with");click("Left stick");check(state.options.controllerStick==0,"Browse stick is configurable");
    click("Browse with bumpers");check(!state.options.controllerBumpers,"Combat bumper capture can be disabled");
    click("Enable controller");check(!state.options.controllerEnabled,"Controller gameplay bindings can be disabled");click("Undo");
    // Open an actual group binding combo; choosing its current value is a no-op.
    click("##pad button0");click("Triangle");check(state.options.controllerGroups[0].button==PadButton::y,"Controller group button changes independently of keyboard key");
    check(state.options.keys[0]==2,"Controller binding preserves keyboard binding");
    click("##pad modifier0");click("L1");check(state.options.controllerGroups[0].modifier==PadButton::lb,"Controller modifiers apply immediately");
    click("Equipment");click("Edit text##1");click("Clear text");click("a");click("b");click("c");click("Done");
    check(state.options.names[0]=="abc","On-screen keyboard renames the active group");
    click("Edit text##0");click("f");click("l");click("a");click("m");click("e");click("s");click("Done");
    check(probe::find("##candidate").size()==1,"On-screen keyboard filters inventory and magic");
    click("Edit text##0");padKey(PadButton::b);check(ImGui::GetCurrentContext()->OpenPopupStack.empty(),"B dismisses text entry without changing text");
    click("Presets");click("Edit text##2");click("Clear text");click("p");click("a");click("d");click("Done");click("Export preset");
    check(std::find(state.presets.begin(),state.presets.end(),"pad")!=state.presets.end(),"Controller text entry can name an exported preset");
    padKey(PadButton::start);check(!state.open,"Start closes the editor with live edits retained");
    state=fixture::editor();state.session=session++;state.vr=true;state.controller=false;frame();frame();
    const auto desktopBindings=state.options.controllerGroups;
    click("Controls");click("Controller");
    check(probe::find("Button icons").empty()&&probe::find("Browse with bumpers").empty(),"VR controls omit unrelated desktop artwork/bumper options");
    click("##pad button0");check(probe::find("D-pad Up").empty(),"VR picker contains only buttons available on tracked controllers");
    click("Main pad");check(state.options.vrGroups[0].button==PadButton::start,"VR touchpad can be assigned in the actual editor");
    check(state.options.controllerGroups==desktopBindings,"Editing VR controls preserves desktop bindings");
    click("Browse with");click("Off-hand stick / pad");check(state.options.vrStick==0&&state.options.controllerStick==1,"VR browse hand is configured independently");
    click("Undo");check(state.options.vrStick==1,"VR control changes support Undo");
    click("Close");check(!state.open,"VR pointer interaction reaches the close button");
    ImGui::DestroyContext();std::cout<<"PASS: "<<checks<<" real ImGui interaction checks (search, add, hand, remove, rename, rebind, duplicate guard, layouts, dragging, live changes, undo/revert, external themes, independent placement, empty group).\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}

#include "Editor.h"
#include "VRInput.h"
#include "Layout.h"
#include "SelectionTime.h"
#include "Presets.h"
#include <imgui_internal.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace quickslot {
namespace {
constexpr ImVec4 brass{.79f,.77f,.72f,1},muted{.57f,.57f,.55f,1},ink{.92f,.90f,.87f,1};
struct Scan {unsigned code;ImGuiKey key;const char* name;};
constexpr Scan scans[]={
 {1,ImGuiKey_Escape,"Esc"},{14,ImGuiKey_Backspace,"Backspace"},{15,ImGuiKey_Tab,"Tab"},{28,ImGuiKey_Enter,"Enter"},
 {29,ImGuiKey_LeftCtrl,"Ctrl"},{157,ImGuiKey_RightCtrl,"Right Ctrl"},{42,ImGuiKey_LeftShift,"Shift"},{54,ImGuiKey_RightShift,"Right Shift"},
 {56,ImGuiKey_LeftAlt,"Alt"},{184,ImGuiKey_RightAlt,"Right Alt"},{57,ImGuiKey_Space,"Space"},
 {199,ImGuiKey_Home,"Home"},{207,ImGuiKey_End,"End"},{201,ImGuiKey_PageUp,"Page Up"},{209,ImGuiKey_PageDown,"Page Down"},
 {210,ImGuiKey_Insert,"Insert"},{211,ImGuiKey_Delete,"Delete"},{203,ImGuiKey_LeftArrow,"Left"},{205,ImGuiKey_RightArrow,"Right"},
 {200,ImGuiKey_UpArrow,"Up"},{208,ImGuiKey_DownArrow,"Down"},
 {30,ImGuiKey_A,"A"},{48,ImGuiKey_B,"B"},{46,ImGuiKey_C,"C"},{32,ImGuiKey_D,"D"},{18,ImGuiKey_E,"E"},{33,ImGuiKey_F,"F"},
 {34,ImGuiKey_G,"G"},{35,ImGuiKey_H,"H"},{23,ImGuiKey_I,"I"},{36,ImGuiKey_J,"J"},{37,ImGuiKey_K,"K"},{38,ImGuiKey_L,"L"},
 {50,ImGuiKey_M,"M"},{49,ImGuiKey_N,"N"},{24,ImGuiKey_O,"O"},{25,ImGuiKey_P,"P"},{16,ImGuiKey_Q,"Q"},{19,ImGuiKey_R,"R"},
 {31,ImGuiKey_S,"S"},{20,ImGuiKey_T,"T"},{22,ImGuiKey_U,"U"},{47,ImGuiKey_V,"V"},{17,ImGuiKey_W,"W"},{45,ImGuiKey_X,"X"},
 {21,ImGuiKey_Y,"Y"},{44,ImGuiKey_Z,"Z"},{12,ImGuiKey_Minus,"-"},{13,ImGuiKey_Equal,"="},{26,ImGuiKey_LeftBracket,"["},
 {27,ImGuiKey_RightBracket,"]"},{39,ImGuiKey_Semicolon,";"},{40,ImGuiKey_Apostrophe,"'"},{41,ImGuiKey_GraveAccent,"`"},
 {43,ImGuiKey_Backslash,"\\"},{51,ImGuiKey_Comma,","},{52,ImGuiKey_Period,"."},{53,ImGuiKey_Slash,"/"},
 {87,ImGuiKey_F11,"F11"},{88,ImGuiKey_F12,"F12"}
};
ImGuiKey imguiKey(unsigned scan) {
    if(scan>=2&&scan<=10)return ImGuiKey(ImGuiKey_1+scan-2);
    if(scan==11)return ImGuiKey_0;
    if(scan>=59&&scan<=68)return ImGuiKey(ImGuiKey_F1+scan-59);
    for(const auto& s:scans)if(s.code==scan)return s.key;
    return ImGuiKey_None;
}
struct Drag {std::uint64_t session,revision,catalogVersion,candidate;std::size_t group,index;};
void style(const Theme& theme,const Options& options) {
    auto& s=ImGui::GetStyle();s.WindowRounding=2;s.ChildRounding=0;s.FrameRounding=1;s.PopupRounding=1;s.ScrollbarRounding=2;
    s.WindowPadding={20,16};s.FramePadding={10,7};s.ItemSpacing={10,9};s.ItemInnerSpacing={6,6};s.WindowBorderSize=1;
    auto* c=s.Colors;c[ImGuiCol_Text]=ink;c[ImGuiCol_TextDisabled]=muted;
    c[ImGuiCol_WindowBg]={.055f,.055f,.06f,.985f};c[ImGuiCol_ChildBg]={.07f,.07f,.075f,.7f};
    c[ImGuiCol_Border]={.32f,.30f,.28f,.7f};c[ImGuiCol_FrameBg]={.12f,.12f,.125f,1};
    c[ImGuiCol_FrameBgHovered]={.21f,.18f,.18f,1};c[ImGuiCol_FrameBgActive]={.29f,.19f,.18f,1};
    c[ImGuiCol_Button]={.19f,.18f,.18f,1};c[ImGuiCol_ButtonHovered]={.31f,.21f,.20f,1};c[ImGuiCol_ButtonActive]={.40f,.21f,.19f,1};
    c[ImGuiCol_Header]={.30f,.18f,.17f,.9f};c[ImGuiCol_HeaderHovered]={.30f,.22f,.21f,1};c[ImGuiCol_HeaderActive]={.38f,.21f,.19f,1};
    c[ImGuiCol_CheckMark]=brass;c[ImGuiCol_SliderGrab]={.58f,.30f,.27f,1};c[ImGuiCol_SliderGrabActive]={.75f,.43f,.36f,1};
    c[ImGuiCol_Separator]={.32f,.30f,.28f,1};c[ImGuiCol_DragDropTarget]=brass;
    c[ImGuiCol_Tab]={.13f,.16f,.16f,1};c[ImGuiCol_TabSelected]={.26f,.27f,.21f,1};c[ImGuiCol_TabHovered]={.33f,.33f,.25f,1};
    const auto accent=ImGui::ColorConvertU32ToFloat4(themeAccent(theme,options));
    const auto blend=[&](float intensity){return ImVec4{.08f+accent.x*intensity,.08f+accent.y*intensity,.08f+accent.z*intensity,1};};
    c[ImGuiCol_Text]=ImGui::ColorConvertU32ToFloat4(theme.text);c[ImGuiCol_TextDisabled]=ImGui::ColorConvertU32ToFloat4(theme.secondary);
    c[ImGuiCol_ButtonHovered]=c[ImGuiCol_HeaderHovered]=blend(.4f);c[ImGuiCol_ButtonActive]=c[ImGuiCol_HeaderActive]=blend(.6f);
    c[ImGuiCol_TitleBg]=blend(.1f);c[ImGuiCol_TitleBgActive]=blend(.25f);
    c[ImGuiCol_Header]=c[ImGuiCol_TabSelected]=blend(.3f);c[ImGuiCol_SliderGrab]=accent;c[ImGuiCol_SliderGrabActive]=blend(.8f);
}
Snapshot preview(const EditorState& state) {
    Snapshot s;s.editorPreview=true;s.visible=true;s.browsing=true;s.activeGroup=state.draft.active;
    s.rows=state.rows[s.activeGroup];s.selected=s.preview=state.draft.current().selected;
    for(std::size_t i=0;i<Arsenal::count;++i) {
        const auto& g=state.draft.groups[i];auto& tile=s.groups[i];tile.count=g.entries.size();
        if(g.selected<state.rows[i].size()){const auto& row=state.rows[i][g.selected];tile.entry=row.entry;tile.available=row.available;tile.equipped=row.equipped;tile.quantity=row.quantity;tile.cooldown=row.cooldown;}
    }
    return s;
}
void label(const char* text){ImGui::TextColored(brass,"%s",text);}
}
std::string scanName(unsigned scan) {
    if(!scan)return "None";
    if(scan>=2&&scan<=10)return std::to_string(scan-1);if(scan==11)return "0";
    if(scan>=59&&scan<=68)return "F"+std::to_string(scan-58);
    for(const auto& s:scans)if(s.code==scan)return s.name;
    return "Scan "+std::to_string(scan);
}
static std::string shortcutName(unsigned scan,unsigned modifiers) {
    if(!scan)return "None";
    std::string name;
    if(modifiers&keyModifier::ctrl)name+="Ctrl + ";
    if(modifiers&keyModifier::shift)name+="Shift + ";
    if(modifiers&keyModifier::alt)name+="Alt + ";
    return name+scanName(scan);
}
void Editor::beginSession(const EditorState& state) {
    controllerMode_=state.controller;
    auto& inputIO=ImGui::GetIO();inputIO.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;
    inputIO.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    if(session_==state.session) {
        if(restoreSerial_!=state.restoreSerial){restoreSerial_=state.restoreSerial;for(std::size_t i=0;i<Arsenal::count;++i)std::snprintf(names_[i].data(),names_[i].size(),"%s",state.options.names[i].c_str());selected_=0;binding_=-1;}
        return;
    }
    restoreSerial_=state.restoreSerial;
    session_=state.session;catalogVersion_=state.catalogVersion;candidate_=0;selected_=0;binding_=-1;placement_=false;hint_.clear();downs_.clear();
    search_.fill(0);keys_.fill(false);padKeys_.fill(false);padDowns_.clear();textTarget_=-1;filter_=0;tab_=0;controlsPage_=0;appearancePage_=0;previewMode_=0;shortcutIndex_.reset();shortcutGroup_=Arsenal::count;shortcut_=smartEntry(0,0,0);comboName_.fill(0);
    for(std::size_t i=0;i<Arsenal::count;++i)std::snprintf(names_[i].data(),names_[i].size(),"%s",state.options.names[i].c_str());
    auto& io=ImGui::GetIO();io.ClearInputKeys();io.ClearInputMouse();mouse_={io.DisplaySize.x*.5f,io.DisplaySize.y*.5f};io.AddMousePosEvent(mouse_.x,mouse_.y);
}
void Editor::input(const std::vector<EditorInput>& events) {
    auto& io=ImGui::GetIO();
    for(const auto& e:events) {
        switch(e.type) {
        case EditorInputType::key:
            if(e.code<keys_.size())keys_[e.code]=e.down;
            if(e.down)downs_.push_back({e.code,keyboardModifiers([&](unsigned scan){return keys_[scan];})});
            if(const auto key=imguiKey(e.code);key!=ImGuiKey_None)io.AddKeyEvent(key,e.down);
            if(e.code==29||e.code==157)io.AddKeyEvent(ImGuiMod_Ctrl,keys_[29]||keys_[157]);
            if(e.code==42||e.code==54)io.AddKeyEvent(ImGuiMod_Shift,keys_[42]||keys_[54]);
            if(e.code==56||e.code==184)io.AddKeyEvent(ImGuiMod_Alt,keys_[56]||keys_[184]);
            break;
        case EditorInputType::character:
            if(binding_==-1){if(e.code<=0xffff)io.AddInputCharacterUTF16(ImWchar16(e.code));else io.AddInputCharacter(e.code);}break;
        case EditorInputType::move:
            mouse_.x=std::clamp(mouse_.x+e.x,0.f,io.DisplaySize.x-1);mouse_.y=std::clamp(mouse_.y+e.y,0.f,io.DisplaySize.y-1);io.AddMousePosEvent(mouse_.x,mouse_.y);break;
        case EditorInputType::button:if(e.code<5)io.AddMouseButtonEvent(int(e.code),e.down);break;
        case EditorInputType::wheel:io.AddMouseWheelEvent(e.x,e.y);break;
        case EditorInputType::pad: {
            if(e.code==0||e.code>=unsigned(PadButton::count))break;
            padKeys_[e.code]=e.down;if(e.down)padDowns_.push_back(PadButton(e.code));
            constexpr ImGuiKey keys[]{ImGuiKey_None,ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadRight,ImGuiKey_GamepadDpadDown,ImGuiKey_GamepadDpadLeft,
                ImGuiKey_GamepadFaceDown,ImGuiKey_GamepadFaceRight,ImGuiKey_GamepadFaceLeft,ImGuiKey_GamepadFaceUp,ImGuiKey_GamepadL1,ImGuiKey_GamepadR1,
                ImGuiKey_GamepadL2,ImGuiKey_GamepadR2,ImGuiKey_GamepadL3,ImGuiKey_GamepadR3,ImGuiKey_GamepadBack,ImGuiKey_GamepadStart};
            io.AddKeyEvent(keys[e.code],e.down);break;
        }
        case EditorInputType::stick: {
            const auto axis=[&](ImGuiKey negative,ImGuiKey positive,float value) {
                value=std::isfinite(value)?value:0;
                const float lo=std::clamp((-value-.2f)/.8f,0.f,1.f),hi=std::clamp((value-.2f)/.8f,0.f,1.f);
                io.AddKeyAnalogEvent(negative,lo>0,lo);io.AddKeyAnalogEvent(positive,hi>0,hi);
            };
            axis(e.code?ImGuiKey_GamepadRStickLeft:ImGuiKey_GamepadLStickLeft,e.code?ImGuiKey_GamepadRStickRight:ImGuiKey_GamepadLStickRight,e.x);
            axis(e.code?ImGuiKey_GamepadRStickDown:ImGuiKey_GamepadLStickDown,e.code?ImGuiKey_GamepadRStickUp:ImGuiKey_GamepadLStickUp,e.y);break;
        }
        }
    }
}
std::vector<EditCommand> Editor::draw(const EditorState& state,Hud& hud,double now) {
    const auto screen=ImGui::GetIO().DisplaySize;auto options=state.options;
    const auto metrics=editorGeometry(options,screen.x,screen.y);const auto px=[&](float value){return value*metrics.scale;};
    struct StyleGuard {float cursor;ImGuiStyle old=ImGui::GetStyle();~StyleGuard(){ImGui::GetStyle()=old;ImGui::GetStyle().MouseCursorScale=cursor;}} guard{metrics.scale};
    ImGui::GetStyle()=ImGuiStyle{};style(hud.themes().get(options.themeId),options);ImGui::GetStyle().ScaleAllSizes(metrics.scale);
    ImGui::PushFont(hud.bodyFont(options),px(18)*(metrics.width/metrics.scale<1000?.86f:1.f));
    std::vector<EditCommand> out;bool changed=false,openText=false;
    const bool padCancel=std::find(padDowns_.begin(),padDowns_.end(),PadButton::b)!=padDowns_.end();
    const auto textButton=[&](int target,const char* value) {
        if(!state.controller)return;
        if(ImGui::Button((std::string("Edit text##")+std::to_string(target)).c_str())) {
            textTarget_=target;textGroup_=state.draft.active;std::snprintf(padText_.data(),padText_.size(),"%s",value);openText=true;
        }
    };
    auto snapshot=preview(state);snapshot.browsing=previewOpen_;snapshot.controller=state.controller||state.vr;snapshot.vr=state.vr;
    const auto command=[&](EditAction action,std::size_t group,std::size_t index=0,std::size_t target=0) {
        EditCommand c;c.action=action;c.session=state.session;c.revision=state.revision;c.catalogVersion=state.catalogVersion;c.group=group;c.index=index;c.target=target;return c;
    };
    const auto emit=[&](EditCommand c){if(out.empty())out.push_back(std::move(c));};
    const auto accept=[&](std::optional<std::size_t> index,std::size_t destination) {
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("QUICKSLOT_ENTRY")) {
                if(payload->DataSize==sizeof(Drag)) {
                    const auto d=*static_cast<const Drag*>(payload->Data);
                    if(d.session==state.session&&d.revision==state.revision) {
                        auto c=command(d.candidate?EditAction::add:d.group==destination?EditAction::reorder:EditAction::move,d.candidate?destination:d.group,d.index,destination);
                        if(d.candidate){c.candidate=d.candidate;c.catalogVersion=d.catalogVersion;}
                        else if(d.group==destination)c.target=index.value_or(state.draft.groups[destination].entries.empty()?0:state.draft.groups[destination].entries.size()-1);
                        emit(c);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    };
    if(catalogVersion_!=state.catalogVersion){catalogVersion_=state.catalogVersion;candidate_=0;}
    const bool wasBinding=binding_!=-1;
    if(wasBinding&&padCancel){binding_=-1;hint_.clear();}
    for(const auto press:downs_) {
        const auto scan=press.scan;
        if(binding_!=-1) {
            if(scan==1){binding_=-1;hint_.clear();break;}
            if(modifierForScan(scan))continue;
            if(!validGroupKey(scan)){hint_="Key unavailable";continue;}
            bool duplicate=binding_!=-2&&scan==options.editorKey;
            for(std::size_t i=0;i<options.keys.size();++i)if(int(i)!=binding_&&groupVisible(options,i)&&options.keys[i]==scan)duplicate=true;
            if(duplicate){hint_="Key already assigned";continue;}
            if(binding_==-2){options.editorKey=scan;options.editorModifiers=press.modifiers;}
            else options.keys[std::size_t(binding_)]=scan;
            changed=true;binding_=-1;hint_.clear();break;
        }
    }
    const bool shortcutPressed=std::any_of(downs_.begin(),downs_.end(),[&](const auto& press){return editorShortcutMatches(state.options,press.scan,press.modifiers);});
    if(!wasBinding&&(ImGui::IsKeyPressed(ImGuiKey_Escape,false)||(!ImGui::GetIO().WantTextInput&&shortcutPressed)))emit(command(EditAction::close,state.draft.active));
    if(!wasBinding&&std::find(padDowns_.begin(),padDowns_.end(),PadButton::start)!=padDowns_.end())emit(command(EditAction::close,state.draft.active));
    if(placement_&&std::find(padDowns_.begin(),padDowns_.end(),PadButton::b)!=padDowns_.end())placement_=false;
    padDowns_.clear();
    downs_.clear();
    const auto bindButton=[&](int index,unsigned key,const char* id) {
        const auto text=(binding_==index?std::string("Press a key…"):shortcutName(key,index==-2?options.editorModifiers:0))+id;
        const float width=index==-2?std::max(px(150),ImGui::CalcTextSize(text.c_str(),nullptr,true).x+px(20)):px(95);
        if(ImGui::Button(text.c_str(),{width,0})){binding_=index;hint_="Press a key · Esc cancels";}
    };
    auto& io=ImGui::GetIO();
    if(placement_) {
        if(options.placementGrid) {
            auto* grid=ImGui::GetBackgroundDrawList();const float step=placementGridStep(screen.y);
            const auto thin=IM_COL32(225,224,218,48),axis=IM_COL32(245,239,219,170);
            for(float x=std::fmod(screen.x*.5f,step);x<screen.x;x+=step)grid->AddLine({x,0},{x,screen.y},thin);
            for(float y=std::fmod(screen.y*.5f,step);y<screen.y;y+=step)grid->AddLine({0,y},{screen.x,y},thin);
            grid->AddLine({screen.x*.5f,0},{screen.x*.5f,screen.y},axis,px(1.5f));
            grid->AddLine({0,screen.y*.5f},{screen.x,screen.y*.5f},axis,px(1.5f));
        }
        hud.draw(snapshot,options,now);
        const auto geometry=hudGeometry(options,screen.x,screen.y);
        const bool freeSelector=placementTarget_==1;
        const ImVec2 dragOrigin=freeSelector?ImVec2{geometry.origin.x+geometry.selectorOrigin.x*geometry.scale,geometry.origin.y+(geometry.selectorOrigin.y-32)*geometry.scale}:ImVec2{geometry.origin.x,geometry.origin.y};
        const ImVec2 dragSize=freeSelector?ImVec2{geometry.selectorWidth*geometry.scale,(geometry.selectorHeight+32)*geometry.scale}:ImVec2{geometry.width*geometry.scale,geometry.height*geometry.scale};
        ImGui::SetNextWindowPos(dragOrigin);ImGui::SetNextWindowSize(dragSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
        ImGui::Begin("##placement",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove);
        ImGui::InvisibleButton(freeSelector?"Drag selector":"Drag widget",ImGui::GetContentRegionAvail());
        if(ImGui::IsItemActivated())dragStart_={freeSelector?options.selectorX:options.positionX,freeSelector?options.selectorY:options.positionY};
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left,0)) {
            const auto delta=ImGui::GetMouseDragDelta(ImGuiMouseButton_Left,0);
            float x=std::clamp(dragStart_.x+delta.x/std::max(1.f,screen.x-16-dragSize.x),0.f,1.f);
            float y=std::clamp(dragStart_.y+delta.y/std::max(1.f,screen.y-16-dragSize.y),0.f,1.f);
            if(options.placementSnap){x=snapPosition(x,dragSize.x,screen.x,placementGridStep(screen.y));y=snapPosition(y,dragSize.y,screen.y,placementGridStep(screen.y));}
            if(freeSelector){options.selectorX=x;options.selectorY=y;}else{options.positionX=x;options.positionY=y;}changed=true;
        }
        ImGui::End();ImGui::PopStyleVar();
        const float panelWidth=std::min(px(310),screen.x-px(40));
        const float panelX=dragOrigin.x+dragSize.x*.5f<screen.x*.5f?screen.x-panelWidth-px(20):px(20);
        ImGui::SetNextWindowPos({panelX,px(20)},ImGuiCond_Always);ImGui::SetNextWindowSize({panelWidth,0});
        ImGui::Begin("Position",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse);
        if(ImGui::RadioButton("Widget",placementTarget_==0))placementTarget_=0;ImGui::SameLine();
        if(ImGui::RadioButton("Selector",placementTarget_==1)){placementTarget_=1;options.selectorDock=4;previewOpen_=true;changed=true;}
        ImGui::SetNextItemWidth(-px(40));changed|=ImGui::SliderFloat("X",freeSelector?&options.selectorX:&options.positionX,0,1,"%.3f");
        ImGui::SetNextItemWidth(-px(40));changed|=ImGui::SliderFloat("Y",freeSelector?&options.selectorY:&options.positionY,0,1,"%.3f");
        changed|=ImGui::Checkbox("Grid",&options.placementGrid);
        changed|=ImGui::Checkbox("Snap to grid",&options.placementSnap);
        ImGui::SetNextItemWidth(-px(60));changed|=ImGui::SliderFloat("Scale",&options.scale,.55f,1.5f,"%.2fx");
        ImGui::Checkbox("Show open selector",&previewOpen_);
        if(ImGui::Button("Back",{-1,0}))placement_=false;
        if(ImGui::Button("Close",{-1,0}))emit(command(EditAction::close,state.draft.active));
        ImGui::BeginDisabled(!state.canUndo);if(ImGui::Button("Undo",{-1,0}))emit(command(EditAction::undo,state.draft.active));ImGui::EndDisabled();
        ImGui::End();
    } else {
        const float w=metrics.width,h=metrics.height;
        ImGui::SetNextWindowPos({(screen.x-w)*.5f,(screen.y-h)*.5f},ImGuiCond_Always);ImGui::SetNextWindowSize({w,h});
        ImGui::Begin("Quickslot",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        label("Quickslot");
        const std::array<const char*,5> tabs{"Equipment","Shortcuts","Appearance","Controls","Presets"};
        const std::array<int,5> pages{0,2,1,3,4};
        for(std::size_t i=0;i<tabs.size();++i){if(i)ImGui::SameLine();const bool active=tab_==pages[i];if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_Header));if(ImGui::Button(tabs[i],{px(i==2?130:114),0}))tab_=pages[i];if(active)ImGui::PopStyleColor();}
        ImGui::Separator();
        const float contentHeight=std::max(px(120),ImGui::GetContentRegionAvail().y-px(87));
        if(tab_==0||tab_==2) {
        ImGui::BeginChild("groups",{px(156),contentHeight},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
        label("GROUPS");
        for(std::size_t position=0;position<options.groupCount;++position) {
            const auto i=options.groupOrder[position];ImGui::PushID(int(i));
            const auto text=scanName(options.keys[i])+"   "+options.names[i]+"\n     "+std::to_string(state.draft.groups[i].entries.size())+" / 16";
            if(ImGui::Selectable(text.c_str(),i==state.draft.active,0,{0,px(48)})){selected_=0;emit(command(EditAction::select,i));}
            accept({},i);ImGui::PopID();
        }
        if(options.groupCount<Arsenal::count&&ImGui::Button("+ Add group",{-1,0}))emit(command(EditAction::addGroup,state.draft.active));
        ImGui::Separator();
        if(ImGui::Button("Group up",{-1,0}))emit(command(EditAction::groupUp,state.draft.active));
        if(ImGui::Button("Group down",{-1,0}))emit(command(EditAction::groupDown,state.draft.active));
        ImGui::BeginDisabled(state.draft.current().entries.empty());
        if(ImGui::Button("Clear group",{-1,0}))emit(command(EditAction::clearGroup,state.draft.active));ImGui::EndDisabled();
        ImGui::BeginDisabled(options.groupCount<=1);
        if(ImGui::Button("Remove group",{-1,0}))emit(command(EditAction::removeGroup,state.draft.active));ImGui::EndDisabled();
        ImGui::EndChild();ImGui::SameLine();
        }
        ImGui::BeginChild("workspace",{0,contentHeight},ImGuiChildFlags_NavFlattened);
        const auto groupID=state.draft.active;const auto& group=state.draft.current();
        if(tab_==0) {
            ImGui::SetNextItemWidth(std::max(px(100),ImGui::GetContentRegionAvail().x-px(210)));
            if(ImGui::InputText("##Group name",names_[groupID].data(),names_[groupID].size())){options.names[groupID]=names_[groupID].data();changed=true;}
            ImGui::SameLine();ImGui::TextDisabled("Key");ImGui::SameLine();bindButton(int(groupID),options.keys[groupID],"##group binding");
            textButton(1,names_[groupID].data());
            const float column=(ImGui::GetContentRegionAvail().x-px(12))*.5f;
            ImGui::BeginChild("catalog",{column,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label("INVENTORY & MAGIC");
            ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##search","Search…",search_.data(),search_.size());
            textButton(0,search_.data());
            ImGui::SetNextItemWidth(-1);ImGui::Combo("##filter",&filter_,"All items & magic\0Weapons\0Spells\0Shields & torches\0Potions\0Food\0Poisons\0Scrolls\0Powers & shouts\0Ammo\0");
            std::vector<const CatalogItem*> matches;
            for(const auto& c:state.catalog)if(catalogMatches(c,search_.data(),filter_))matches.push_back(&c);
            const CatalogItem* picked=nullptr;for(const auto& c:state.catalog)if(c.id==candidate_)picked=&c;
            ImGui::BeginChild("catalog list",{0,std::max(px(70),ImGui::GetContentRegionAvail().y-px(80))},ImGuiChildFlags_NavFlattened);
            if(matches.empty())ImGui::TextWrapped("No matches");
            ImGuiListClipper clipper;clipper.Begin(int(matches.size()),px(32)+ImGui::GetStyle().ItemSpacing.y);
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                const auto& c=*matches[std::size_t(i)];ImGui::PushID(int(c.id));const auto pos=ImGui::GetCursorScreenPos();
                if(ImGui::Selectable("##candidate",candidate_==c.id,ImGuiSelectableFlags_None,{0,px(32)}))candidate_=c.id;
                const auto end=ImGui::GetItemRectMax();auto* d=ImGui::GetWindowDrawList();d->PushClipRect(pos,end,true);
                std::string meta=c.equipped?"Equipped":"";
                if(isStack(c.entry)||c.count>1)meta+=(meta.empty()?"":" · ")+std::string("×")+std::to_string(c.count);
                const float metaWidth=ImGui::GetFont()->CalcTextSizeA(px(12),FLT_MAX,0,meta.c_str()).x;
                const ImVec2 textPos{pos.x+px(6),pos.y+(px(32)-ImGui::GetFontSize())*.5f};
                const float nameEnd=std::max(textPos.x,end.x-px(6)-(meta.empty()?0:metaWidth+px(14)));
                const auto nameSize=ImGui::CalcTextSize(c.entry.name.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text,c.addable?ink:muted);
                ImGui::RenderTextEllipsis(d,textPos,{nameEnd,end.y},nameEnd,c.entry.name.c_str(),nullptr,&nameSize);
                ImGui::PopStyleColor();
                d->AddText(nullptr,px(12),{end.x-px(6)-metaWidth,pos.y+px(10)},ImGui::GetColorU32(muted),meta.c_str());d->PopClipRect();
                if((nameSize.x>nameEnd-textPos.x||!c.detail.empty())&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::BeginTooltip();ImGui::TextUnformatted(c.entry.name.c_str());
                    if(!c.detail.empty())ImGui::TextUnformatted(c.detail.c_str());
                    ImGui::EndTooltip();
                }
                if(c.addable&&ImGui::BeginDragDropSource()) {
                    Drag drag{state.session,state.revision,state.catalogVersion,c.id,groupID,0};ImGui::SetDragDropPayload("QUICKSLOT_ENTRY",&drag,sizeof(drag));
                    ImGui::TextUnformatted(c.entry.name.c_str());ImGui::EndDragDropSource();
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            picked=nullptr;for(const auto& c:state.catalog)if(c.id==candidate_)picked=&c;
            ImGui::BeginDisabled(!picked||!picked->addable);
            const bool flexible=picked&&flexibleHand(picked->entry);
            const auto add=[&](Hand hand){auto c=command(EditAction::add,groupID);c.candidate=candidate_;c.hand=hand;emit(c);};
            if(flexible) {
                const float bw=(ImGui::GetContentRegionAvail().x-px(10))*.5f;
                if(ImGui::Button("Add right",{bw,0}))add(Hand::right);ImGui::SameLine();if(ImGui::Button("Add left",{bw,0}))add(Hand::left);
            } else if(ImGui::Button(picked&&picked->entry.twoHanded?"Add · both hands":"Add to group",{-1,0}))add(Hand::right);
            ImGui::EndDisabled();
            if(ImGui::Button("Refresh inventory",{-1,0}))emit(command(EditAction::refresh,groupID));
            ImGui::EndChild();ImGui::SameLine();
            ImGui::BeginChild("entries",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label("GROUP ITEMS");
            ImGui::BeginChild("group list",{0,std::max(px(70),ImGui::GetContentRegionAvail().y-px(123))},ImGuiChildFlags_NavFlattened);
            if(group.entries.empty()) {
                ImGui::Button("Drop equipment here",{-1,px(100)});accept({},groupID);
            }
            for(std::size_t i=0;i<group.entries.size();++i) {
                ImGui::PushID(int(i));const auto& e=group.entries[i];const auto pos=ImGui::GetCursorScreenPos();
                if(ImGui::Selectable("##entry",selected_==i,0,{0,px(47)}))selected_=i;
                const auto end=ImGui::GetItemRectMax();auto* d=ImGui::GetWindowDrawList();d->PushClipRect(pos,end,true);
                const bool available=i<state.rows[groupID].size()&&state.rows[groupID][i].available;
                const auto name=std::to_string(i+1)+"  "+e.name;const auto nameSize=ImGui::CalcTextSize(name.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text,available?ink:muted);
                ImGui::RenderTextEllipsis(d,{pos.x+px(6),pos.y+px(4)},{end.x-px(6),end.y},end.x-px(6),name.c_str(),nullptr,&nameSize);
                ImGui::PopStyleColor();
                const auto status=i<state.rows[groupID].size()?rowStatus(state.rows[groupID][i]):handName(e);
                d->AddText(nullptr,px(12),{pos.x+px(6),pos.y+px(27)},ImGui::GetColorU32(brass),status.c_str());d->PopClipRect();
                const auto statusWidth=ImGui::GetFont()->CalcTextSizeA(px(12),FLT_MAX,0,status.c_str()).x;
                if(std::max(nameSize.x,statusWidth)>end.x-pos.x-px(12)&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))ImGui::SetTooltip("%s\n%s",e.name.c_str(),status.c_str());
                if(ImGui::BeginDragDropSource()) {
                    Drag drag{state.session,state.revision,state.catalogVersion,0,groupID,i};ImGui::SetDragDropPayload("QUICKSLOT_ENTRY",&drag,sizeof(drag));
                    ImGui::TextUnformatted(e.name.c_str());ImGui::EndDragDropSource();
                }
                accept(i,groupID);ImGui::PopID();
            }
            ImGui::EndChild();
            const bool has=selected_<group.entries.size();ImGui::BeginDisabled(!has);
            if(ImGui::Button("Remove"))emit(command(EditAction::remove,groupID,selected_));ImGui::SameLine();
            ImGui::BeginDisabled(!has||selected_==0);if(ImGui::Button("Up"))emit(command(EditAction::reorder,groupID,selected_,selected_-1));ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled(!has||selected_+1>=group.entries.size());if(ImGui::Button("Down"))emit(command(EditAction::reorder,groupID,selected_,selected_+1));ImGui::EndDisabled();
            ImGui::BeginDisabled(!has||!flexibleHand(group.entries[has?selected_:0]));
            if(ImGui::Button("Change hand",{-1,0})&&has){auto c=command(EditAction::hand,groupID,selected_);c.hand=group.entries[selected_].hand==Hand::left?Hand::right:Hand::left;emit(c);}ImGui::EndDisabled();
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##move","Move to another group…")) {
                for(std::size_t position=0;position<options.groupCount;++position){const auto i=options.groupOrder[position];if(i!=groupID&&ImGui::Selectable(options.names[i].c_str()))emit(command(EditAction::move,groupID,selected_,i));}
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();ImGui::EndChild();
        } else if(tab_==2) {
            if(shortcutGroup_!=groupID){shortcutGroup_=groupID;shortcutIndex_.reset();shortcut_=smartEntry(0,0,0);}
            if(shortcutIndex_&&(*shortcutIndex_>=group.entries.size()||(group.entries[*shortcutIndex_].kind!=Kind::smartPotion&&group.entries[*shortcutIndex_].kind!=Kind::combo&&!isHandAction(group.entries[*shortcutIndex_]))))shortcutIndex_.reset();
            if(shortcutIndex_)shortcut_=group.entries[*shortcutIndex_];
            bool shortcutChanged=false;
            const float column=std::max(px(190),(ImGui::GetContentRegionAvail().x-px(12))*.55f);
            ImGui::BeginChild("shortcut builder",{column,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label(shortcutIndex_?"EDIT SHORTCUT":"NEW SHORTCUT");
            if(ImGui::Button("New smart potion",{-1,0})){shortcutIndex_.reset();shortcut_=smartEntry(0,0,0);}
            if(ImGui::Button("New combination",{-1,0})){shortcutIndex_.reset();shortcut_=Entry{};shortcut_.kind=Kind::combo;shortcut_.name="Equipment combination";comboName_.fill(0);}
            if(ImGui::Button("New hand action",{-1,0})){shortcutIndex_.reset();shortcut_=unarmedEntry();}
            if(isHandAction(shortcut_)) {
                int action=shortcut_.kind==Kind::unarmed?0:shortcut_.hand==Hand::right?1:2;
                ImGui::SetNextItemWidth(-px(70));
                if(ImGui::Combo("Action",&action,"Unarmed\0Empty right hand\0Empty left hand\0")){shortcut_=action==0?unarmedEntry():emptyHandEntry(action==1?Hand::right:Hand::left);shortcutChanged=true;}
                if(!shortcutIndex_&&ImGui::Button("Add action",{-1,0})){auto c=command(EditAction::saveShortcut,groupID);c.shortcut=shortcut_;emit(c);}
            } else if(shortcut_.kind==Kind::smartPotion) {
                int resource=int(shortcut_.resource),effect=int(shortcut_.effect),policy=int(shortcut_.policy);
                ImGui::SetNextItemWidth(-px(100));if(ImGui::Combo("Resource",&resource,"Health\0Magicka\0Stamina\0")){shortcut_.resource=unsigned(resource);shortcutChanged=true;}
                ImGui::SetNextItemWidth(-px(100));if(ImGui::Combo("Effect",&effect,"Restoration\0Regeneration\0")){shortcut_.effect=unsigned(effect);if(effect&&shortcut_.policy==2)shortcut_.policy=0;shortcutChanged=true;}
                policy=int(shortcut_.policy);ImGui::SetNextItemWidth(-px(100));
                if(ImGui::Combo("Choose",&policy,shortcut_.effect?"Lowest strength\0Highest strength\0":"Lowest strength\0Highest strength\0Best fit\0")){shortcut_.policy=unsigned(policy);shortcutChanged=true;}
                shortcut_.name=smartName(shortcut_);shortcut_.icon=smartIcon(shortcut_.resource);
                if(!shortcutIndex_&&ImGui::Button("Add smart slot",{-1,0})){auto c=command(EditAction::saveShortcut,groupID);c.shortcut=shortcut_;emit(c);}
                ImGui::Spacing();label("NEXT BOTTLE");
                const auto* bottle=choosePotion(shortcut_,state.potions,state.deficits[shortcut_.resource]);
                const auto feedback=potionFeedback(shortcut_,bottle,state.deficits[shortcut_.resource]);
                if(bottle)ImGui::TextWrapped("%s ×%d",bottle->entry.name.c_str(),bottle->count);
                if(!bottle||(!shortcut_.effect&&shortcut_.policy==2&&state.deficits[shortcut_.resource]<.01f))ImGui::TextWrapped("%s",feedback.reason.c_str());
                if(!feedback.estimate.empty()&&ImGui::CollapsingHeader("Details"))ImGui::TextWrapped("%s",feedback.estimate.c_str());
                label("EXCLUDED POTIONS");
                ImGui::BeginChild("potion exclusions",{0,px(190)},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
                for(const auto& bottle:state.potions) {
                    const PotionExclusion ref{bottle.entry.form,bottle.entry.recipe};
                    const auto it=std::find_if(shortcut_.exclusions.begin(),shortcut_.exclusions.end(),[&](const auto& x){return exclusionMatches(x,bottle.entry);});bool excluded=it!=shortcut_.exclusions.end();
                    ImGui::PushID(int(bottle.entry.form));
                    if(ImGui::Checkbox(bottle.entry.name.c_str(),&excluded)) {
                        if(excluded&&shortcut_.exclusions.size()<128)shortcut_.exclusions.push_back(ref);
                        else if(!excluded)std::erase_if(shortcut_.exclusions,[&](const auto& x){return exclusionMatches(x,bottle.entry);});
                        shortcutChanged=true;
                    }ImGui::PopID();
                }
                ImGui::EndChild();
                if(ImGui::Button("Clear exclusions")){shortcut_.exclusions.clear();shortcutChanged=true;}

            } else {
                if(shortcutIndex_)std::snprintf(comboName_.data(),comboName_.size(),"%s",shortcut_.name.c_str());
                ImGui::SetNextItemWidth(-1);if(ImGui::InputTextWithHint("##combination name","Combination name",comboName_.data(),comboName_.size())){shortcut_.name=*comboName_.data()?comboName_.data():"Equipment combination";shortcutChanged=true;}
                textButton(3,comboName_.data());
                if(shortcut_.parts.empty())shortcut_.parts.resize(2);
                for(unsigned hand=0;hand<2;++hand) {
                    label(hand?"LEFT HAND":"RIGHT HAND");ImGui::SetNextItemWidth(-1);
                    if(ImGui::BeginCombo(hand?"##left component":"##right component",shortcut_.parts[hand].form||isHandAction(shortcut_.parts[hand])?shortcut_.parts[hand].name.c_str():hand?"Choose left hand…":"Choose right hand…")) {
                        const auto empty=emptyHandEntry(Hand(hand));
                        if(ImGui::Selectable(empty.name.c_str())){shortcut_.parts[hand]=empty;shortcutChanged=true;}
                        for(std::size_t g=0;g<Arsenal::count;++g)for(std::size_t i=0;i<state.draft.groups[g].entries.size();++i) {
                            auto entry=state.draft.groups[g].entries[i];if(isHandAction(entry)||!pairable(entry)||(hand==0&&(entry.kind==Kind::shield||entry.kind==Kind::torch)))continue;
                            entry.hand=Hand(hand);ImGui::PushID(int(g*Group::limit+i));
                            if(ImGui::Selectable((entry.name+" · "+options.names[g]).c_str())){shortcut_.parts[hand]=entry;shortcutChanged=true;}
                            ImGui::PopID();
                        }ImGui::EndCombo();
                    }
                }
                const bool canSwap=shortcut_.parts.size()==2&&flexibleHand(shortcut_.parts[0])&&flexibleHand(shortcut_.parts[1]);
                ImGui::BeginDisabled(!canSwap);if(ImGui::Button("Swap hands")){std::swap(shortcut_.parts[0],shortcut_.parts[1]);shortcut_.parts[0].hand=Hand::right;shortcut_.parts[1].hand=Hand::left;for(auto& part:shortcut_.parts)if(isHandAction(part))part.name=handActionName(part);shortcutChanged=true;}ImGui::EndDisabled();
                const bool valid=shortcut_.parts.size()==2&&validPair(shortcut_.parts[0],shortcut_.parts[1]);
                if(!valid)ImGui::TextWrapped("Choose compatible equipment");
                ImGui::BeginDisabled(!valid);if(!shortcutIndex_&&ImGui::Button("Add combination",{-1,0})){auto c=command(EditAction::saveShortcut,groupID);c.shortcut=shortcut_;emit(c);}ImGui::EndDisabled();
                if(!valid)shortcutChanged=false;
            }
            if(shortcutIndex_&&shortcutChanged){auto c=command(EditAction::saveShortcut,groupID,*shortcutIndex_);c.shortcut=shortcut_;c.replace=true;c.continuous=ImGui::IsAnyItemActive();emit(c);}
            ImGui::EndChild();ImGui::SameLine();
            ImGui::BeginChild("special shortcuts",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label("SHORTCUTS");
            for(std::size_t i=0;i<group.entries.size();++i) {
                const auto& entry=group.entries[i];if(entry.kind!=Kind::smartPotion&&entry.kind!=Kind::combo&&!isHandAction(entry))continue;
                ImGui::PushID(int(i));if(ImGui::Selectable(entry.name.c_str(),shortcutIndex_==i,0,{0,px(42)})){shortcutIndex_=i;shortcut_=entry;}
                if(i<state.rows[groupID].size())ImGui::TextWrapped("%s",rowStatus(state.rows[groupID][i]).c_str());ImGui::PopID();
            }
            if(shortcutIndex_&&ImGui::Button("Remove shortcut",{-1,0})){emit(command(EditAction::remove,groupID,*shortcutIndex_));shortcutIndex_.reset();}
            ImGui::EndChild();
        } else if(tab_==3) {
            ImGui::BeginChild("control settings",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            for(int i=0;i<3;++i){if(i)ImGui::SameLine();if(ImGui::Button(i==0?"General":i==1?"Keyboard":"Controller",{px(125),0}))controlsPage_=i;}
            ImGui::Separator();
            if(controlsPage_==0){
            label("SELECTION");
            changed|=ImGui::Checkbox("Use consumables on release",&options.consumableOnRelease);
            changed|=ImGui::Checkbox("Skip unavailable items",&options.skipEmpty);
            changed|=ImGui::Checkbox("Add from Inventory / Magic",&options.menuCapture);
            label("Game speed while selecting");int timeMode=int(selectionClock(options.selectionSpeed));ImGui::SetNextItemWidth(px(300));
            if(ImGui::Combo("##selection clock",&timeMode,"Full speed\0Slow motion\0Pause\0")){options.selectionSpeed=timeMode==2?0.f:timeMode==1?.5f:1.f;changed=true;}
            if(timeMode==1&&!state.soulsSlowdown){ImGui::SetNextItemWidth(px(200));changed|=ImGui::SliderFloat("Game speed",&options.selectionSpeed,.1f,.95f,"%.2fx");}
            ImGui::Separator();label("POWERS & SHOUTS");
            changed|=ImGui::Checkbox("Activate with group hotkey",&options.voiceHotkey);
            ImGui::BeginDisabled(!options.voiceHotkey);
            changed|=ImGui::Checkbox("Activate powers / shouts on release",&options.voiceOnRelease);
            ImGui::SetNextItemWidth(px(240));changed|=ImGui::SliderFloat("Shout charge",&options.shoutCharge,.05f,2.5f,"%.2f s");
            ImGui::EndDisabled();
            }else if(controlsPage_==1){
            label("KEYS");
            for(std::size_t i=0;i<options.groupCount;++i){const auto id=options.groupOrder[i];ImGui::PushID(int(id));bindButton(int(id),options.keys[id],"##control binding");ImGui::SameLine();ImGui::TextUnformatted(options.names[id].c_str());ImGui::PopID();}
            ImGui::Separator();label("Editor");bindButton(-2,options.editorKey,"##editor binding");
            ImGui::SameLine();if(ImGui::Button("Clear##editor binding")){options.editorKey=options.editorModifiers=0;binding_=-1;changed=true;hint_.clear();}
            if(ImGui::CollapsingHeader("Shortcuts")) {
            ImGui::TextUnformatted("Hold + scroll: select");ImGui::TextUnformatted("Tap: equip / use");
            ImGui::TextUnformatted("Shift + group key: unequip");ImGui::TextUnformatted("Ctrl + group key: add right hand / highlighted item");
            ImGui::TextUnformatted("Ctrl + Shift + group key: add left hand");
            }
            }else{
            changed|=ImGui::Checkbox("Enable controller",&options.controllerEnabled);
            const auto style=state.vr?2u:options.controllerStyle;
            auto& groups=state.vr?options.vrGroups:options.controllerGroups;
            auto& editorBinding=state.vr?options.vrEditor:options.controllerEditor;
            auto& browseStick=state.vr?options.vrStick:options.controllerStick;
            const auto chooseButton=[&](const char* id,PadButton& value) {
                ImGui::SetNextItemWidth(px(155));
                if(ImGui::BeginCombo(id,padLabel(value,style).c_str())){
                    for(unsigned b=0;b<unsigned(PadButton::count);++b)if((!state.vr||validVRButton(PadButton(b)))&&ImGui::Selectable(padLabel(PadButton(b),style).c_str(),unsigned(value)==b)){value=PadButton(b);changed=true;}
                    ImGui::EndCombo();
                }
            };
            const auto padBinding=[&](PadBinding& value,const char* name,unsigned index) {
                const auto old=value;
                chooseButton(("##pad modifier"+std::to_string(index)).c_str(),value.modifier);ImGui::SameLine();chooseButton(("##pad button"+std::to_string(index)).c_str(),value.button);ImGui::SameLine();ImGui::TextUnformatted(name);
                if(value!=old) {
                    bool duplicate=value.button!=PadButton::none&& &value!=&editorBinding&&value==editorBinding;
                    for(std::size_t i=0;i<options.groupCount;++i){const auto& other=groups[options.groupOrder[i]];if(&other!=&value&&value.button!=PadButton::none&&other==value)duplicate=true;}
                    if(value.button==PadButton::none)value.modifier=PadButton::none;
                    if(duplicate||!validPadBinding(value)){value=old;hint_=duplicate?"Button combination already assigned":"Choose different buttons";}else hint_.clear();
                }
            };
            ImGui::TextDisabled("Modifier");ImGui::SameLine(px(175));ImGui::TextDisabled("Button");
            for(std::size_t i=0;i<options.groupCount;++i){const auto id=options.groupOrder[i];padBinding(groups[id],options.names[id].c_str(),unsigned(id));}
            ImGui::Separator();padBinding(editorBinding,"Open editor",8);
            int stick=int(browseStick);ImGui::SetNextItemWidth(px(220));if(ImGui::Combo("Browse with",&stick,state.vr?"Off-hand stick / pad\0Main-hand stick / pad\0Off\0":"Left stick\0Right stick\0Off\0")){browseStick=unsigned(stick);changed=true;} 
            if(!state.vr)changed|=ImGui::Checkbox("Browse with bumpers",&options.controllerBumpers);
            ImGui::SetNextItemWidth(px(220));changed|=ImGui::SliderFloat("Stick deadzone",&options.controllerDeadzone,.2f,.9f,"%.2f");
            if(!state.vr){int buttonStyle=int(options.controllerStyle);ImGui::SetNextItemWidth(px(220));if(ImGui::Combo("Button icons",&buttonStyle,"Xbox\0PlayStation\0")){options.controllerStyle=unsigned(buttonStyle);changed=true;}}
            if(ImGui::CollapsingHeader("Shortcuts")) {
            ImGui::TextUnformatted("Hold: browse  |  Release: select");
            ImGui::TextUnformatted(state.vr?"Tap: equip / use  |  Main B/Y: cancel  |  Off A/X: unequip":"Tap: equip / use  |  B / Circle: cancel  |  X / Square: unequip");
            if(state.vr){ImGui::TextUnformatted("Point + trigger: select");ImGui::TextUnformatted("Main / Off: Skyrim handedness");}
            else {ImGui::TextUnformatted("Editor: D-pad / left stick to navigate, A / Cross to confirm");ImGui::TextUnformatted("Start / Options: close editor");}
            }
            }
            ImGui::EndChild();
        } else if(tab_==4) {
            ImGui::BeginChild("preset settings",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label("EXPORT");
            ImGui::SetNextItemWidth(px(340));ImGui::InputTextWithHint("##preset name","Preset name",presetName_.data(),presetName_.size());
            textButton(2,presetName_.data());
            ImGui::SetNextItemWidth(px(340));ImGui::Combo("##export scope",&exportScope_,"Appearance only\0Complete setup\0");
            if(*presetName_.data()&&!validPresetName(presetName_.data()))ImGui::TextWrapped("Use letters, numbers, spaces, - or _");
            ImGui::BeginDisabled(!validPresetName(presetName_.data()));
            const bool exists=std::find(state.presets.begin(),state.presets.end(),presetName_.data())!=state.presets.end();
            if(ImGui::Button(exists?"Update preset":"Export preset",{px(160),0})){auto c=command(EditAction::exportPreset,groupID);c.presetName=presetName_.data();c.appearanceOnly=exportScope_==0;c.replace=exists;emit(c);}ImGui::EndDisabled();
            ImGui::Separator();label("IMPORT");
            ImGui::SetNextItemWidth(px(340));
            if(ImGui::BeginCombo("##preset file",presetFile_.empty()?"Select preset":presetFile_.c_str())){
                for(const auto& name:state.presets)if(ImGui::Selectable(name.c_str(),name==presetFile_))presetFile_=name;
                ImGui::EndCombo();
            }
            ImGui::SameLine();if(ImGui::Button("Refresh##presets"))emit(command(EditAction::refreshPresets,groupID));
            ImGui::Checkbox("Appearance only",&importAppearance_);
            if(!importAppearance_)ImGui::TextWrapped("Replaces groups and controls. Items may need reassignment.");
            ImGui::BeginDisabled(presetFile_.empty());
            if(ImGui::Button("Import preset",{px(160),0})){auto c=command(EditAction::importPreset,groupID);c.presetName=presetFile_;c.appearanceOnly=importAppearance_;emit(c);}ImGui::EndDisabled();
            ImGui::EndChild();
        } else {
            const std::array<const char*,4> sections{"Style","Groups","Selector","Visibility"};
            for(int i=0;i<4;++i){if(i)ImGui::SameLine();const bool active=appearancePage_==i;if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_Header));if(ImGui::Button(sections[std::size_t(i)],{px(110),0})){appearancePage_=i;previewMode_=i==2?1:0;}if(active)ImGui::PopStyleColor();}
            ImGui::Separator();
            const float controlsWidth=std::min(px(340),ImGui::GetContentRegionAvail().x*.44f);
            ImGui::BeginChild("appearance",{controlsWidth,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            if(ImGui::Button("Position",{-1,0})){placement_=true;placementTarget_=0;}
            const auto id=options.themeId;const auto& currentTheme=hud.themes().get(id);
            const auto slider=[&](const char* name,float& value,float low,float high,const char* format="%.0f"){label(name);ImGui::SetNextItemWidth(-1);const auto key=std::string("##")+name;changed|=ImGui::SliderFloat(key.c_str(),&value,low,high,format);};
            const auto combo=[&](const char* name,const char* key,unsigned& value,const char* choices){label(name);int current=int(value);ImGui::SetNextItemWidth(-1);if(ImGui::Combo(key,&current,choices)){value=unsigned(current);changed=true;}};
            if(appearancePage_==0) {
                label("Theme");ImGui::SetNextItemWidth(-1);
                if(ImGui::BeginCombo("##theme",currentTheme.name.c_str())) {
                    std::vector<const Theme*> themes;for(const auto& t:hud.themes().all())themes.push_back(&t);
                    std::stable_sort(themes.begin(),themes.end(),[](const Theme* a,const Theme* b) {
                        if((a->id=="silver")!=(b->id=="silver"))return a->id=="silver";
                        return std::lexicographical_compare(a->name.begin(),a->name.end(),b->name.begin(),b->name.end(),
                            [](unsigned char x,unsigned char y){return std::tolower(x)<std::tolower(y);});
                    });
                    for(const auto* t:themes)if(ImGui::Selectable(t->name.c_str(),id==t->id)){options.themeId=t->id;changed=true;}
                    ImGui::EndCombo();
                }
                if(!hud.themes().contains(id))ImGui::TextWrapped("Missing theme: %s",id.c_str());
                slider("Widget scale",options.scale,.55f,1.5f,"%.2fx");slider("Opacity",options.opacity,.4f,1,"%.2f");
                ImGui::Separator();auto tweak=adjustments(options);bool tuned=false;
                if(ImGui::Checkbox("Custom accent",&tweak.customAccent)){tuned=true;if(tweak.customAccent){const auto color=ImGui::ColorConvertU32ToFloat4(currentTheme.accent);tweak.accent={color.x,color.y,color.z};}}
                if(tweak.customAccent){ImGui::SetNextItemWidth(-1);tuned|=ImGui::ColorEdit3("##accent",tweak.accent.data(),ImGuiColorEditFlags_NoInputs);}
                label("Frame opacity");ImGui::SetNextItemWidth(-1);tuned|=ImGui::SliderFloat("##Frame opacity",&tweak.frameOpacity,0,1,"%.2f");
                label("Icon size");ImGui::SetNextItemWidth(-1);tuned|=ImGui::SliderFloat("##Icon size",&tweak.iconScale,.65f,1.2f,"%.2fx");
                if(tuned){options.themeAdjustments[options.themeId]=tweak;changed=true;}
                combo("Icon color","##item icon color",options.itemIconColor,"Original\0Theme\0White\0");
                if(options.itemIconColor==1)changed|=ImGui::Checkbox("Tint magic icons",&options.coloredMagic);
                changed|=ImGui::Checkbox("Key icons",&options.keyIcons);
                if(ImGui::Button("Reset theme",{-1,0})){options.themeAdjustments.erase(options.themeId);changed=true;}
            } else if(appearancePage_==1) {
                label("Layout presets");
                const auto preset=[&](unsigned n){layoutPreset(options,n);changed=true;};
                if(ImGui::Button("Cross layout",{-1,0}))preset(0);
                if(ImGui::Button("Diamond layout",{-1,0}))preset(1);
                if(ImGui::Button("Circle layout",{-1,0}))preset(2);
                ImGui::Separator();
                combo("Arrangement","##layout",options.layout,"Horizontal\0Vertical\0Diamond\0Cross\0Circle\0Staggered cross\0");
                combo("Tile shape","##shape",options.shape,"Cards\0Circles\0Squares\0Diamonds\0");
                slider("Group gap",options.spacing,0,40);
                changed|=ImGui::Checkbox("Show group names",&options.showLabels);changed|=ImGui::Checkbox("Show keys",&options.showKeys);
                if(options.shape==0)changed|=ImGui::Checkbox("Show card item names",&options.showCardNames);
            } else if(appearancePage_==2) {
                combo("Arrangement","##selector",options.selector,"Horizontal strip\0Arc\0Ring\0Vertical strip\0List\0");
                if(options.selector!=4)combo("Item shape","##selector shape",options.selectorShape,"Automatic\0Circles\0Squares\0Diamonds\0");
                slider("Item gap",options.selectorSpacing,0,32);
                combo("Position","##selector dock",options.selectorDock,"Above widget\0Below widget\0Left of widget\0Right of widget\0Independent\0");
                if(options.selectorDock!=4)slider("Distance",options.selectorGap,0,100);
                changed|=ImGui::Checkbox("Animate scrolling",&options.animateScroll);
                changed|=ImGui::Checkbox("Show selector hints",&options.showHints);
            } else {
                changed|=ImGui::Checkbox("Show keys",&options.showKeys);
                changed|=ImGui::Checkbox("Show groups while idle",&options.showIdle);
                changed|=ImGui::Checkbox("Fade while idle",&options.fadeIdle);
                if(options.fadeIdle){slider("Fade delay",options.fadeDelay,0,20,"%.1f s");slider("Idle opacity",options.idleOpacity,0,1,"%.2f");changed|=ImGui::Checkbox("Keep visible in combat",&options.keepCombatVisible);}
            }
            ImGui::EndChild();ImGui::SameLine();
            ImGui::BeginChild("live preview",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label("Preview");ImGui::SetNextItemWidth(px(180));ImGui::Combo("##preview view",&previewMode_,"Widget\0Selector\0Both\0");snapshot.browsing=previewMode_!=0;snapshot.selectorOnly=previewMode_==1;
            const auto origin=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();
            auto previewOptions=options;
            previewOptions.showIdle=true;auto* d=ImGui::GetWindowDrawList();d->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
            hud.draw(snapshot,previewOptions,now,origin,size,d);d->PopClipRect();
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ImGui::Separator();
        const auto message=!state.saveError.empty()?"Settings could not be saved":!hint_.empty()?hint_:state.message;
        ImGui::TextColored(muted,"%s",message.c_str());
        if(ImGui::Button("Close",{px(170),0}))emit(command(EditAction::close,groupID));ImGui::SameLine();
        ImGui::BeginDisabled(!state.canUndo);if(ImGui::Button("Undo",{px(100),0}))emit(command(EditAction::undo,groupID));ImGui::EndDisabled();ImGui::SameLine();
        ImGui::BeginDisabled(!state.canRevert);if(ImGui::Button("Revert session",{px(145),0}))emit(command(EditAction::revert,groupID));ImGui::EndDisabled();
        ImGui::End();
    }
    if(openText)ImGui::OpenPopup("Text entry");
    ImGui::SetNextWindowSize({px(660),0},ImGuiCond_Always);
    if(ImGui::BeginPopupModal("Text entry",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("%s",padText_.data());ImGui::Separator();
        const char* rows[]{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm-_"};
        for(const auto* row:rows)for(unsigned col=0;row[col];++col){
            if(col)ImGui::SameLine();const char ch=textUpper_?char(std::toupper(static_cast<unsigned char>(row[col]))):row[col];
            const char label[]{ch,0};ImGui::PushID(int(row[col]));
            if(ImGui::Button(label,{px(49),0})) {const auto n=std::strlen(padText_.data());const auto limit=textTarget_==0?127u:64u;if(n<limit){padText_[n]=ch;padText_[n+1]=0;}}
            ImGui::PopID();
        }
        if(ImGui::Button(textUpper_?"abc":"ABC"))textUpper_=!textUpper_;ImGui::SameLine();
        if(ImGui::Button("Space")){const auto n=std::strlen(padText_.data());if(n<(textTarget_==0?127u:64u)){padText_[n]=' ';padText_[n+1]=0;}}ImGui::SameLine();
        if(ImGui::Button("Backspace")){auto n=std::strlen(padText_.data());if(n){do{--n;}while(n&&(static_cast<unsigned char>(padText_[n])&0xc0)==0x80);padText_[n]=0;}}ImGui::SameLine();
        if(ImGui::Button("Clear text"))padText_.fill(0);
        ImGui::Separator();
        if(ImGui::Button("Done",{px(140),0})) {
            if(textTarget_==0)std::snprintf(search_.data(),search_.size(),"%s",padText_.data());
            else if(textTarget_==1){std::snprintf(names_[textGroup_].data(),names_[textGroup_].size(),"%s",padText_.data());options.names[textGroup_]=padText_.data();changed=true;}
            else if(textTarget_==2)std::snprintf(presetName_.data(),presetName_.size(),"%.64s",padText_.data());
            else if(textTarget_==3){
                std::snprintf(comboName_.data(),comboName_.size(),"%s",padText_.data());shortcut_.name=*padText_.data()?padText_.data():"Equipment combination";
                if(shortcutIndex_){auto c=command(EditAction::saveShortcut,textGroup_,*shortcutIndex_);c.shortcut=shortcut_;c.replace=true;emit(c);}
            }
            textTarget_=-1;ImGui::CloseCurrentPopup();
        }ImGui::SameLine();
        if(ImGui::Button("Cancel",{px(140),0})||padCancel){textTarget_=-1;ImGui::CloseCurrentPopup();}
        ImGui::EndPopup();
    }
    if(changed&&out.empty()){auto c=command(EditAction::settings,state.draft.active);c.options=options;c.continuous=ImGui::IsAnyItemActive();emit(std::move(c));}
    else if(changed&&!out.empty()&&out.front().action==EditAction::close){out.front().hasSettings=true;out.front().options=options;}
    ImGui::PopFont();return out;
}
}

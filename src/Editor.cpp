#include "Editor.h"
#include "LocalizedUI.h"
#include "VRInput.h"
#include "Layout.h"
#include "SelectionTime.h"
#include "Presets.h"
#include "EquippedState.h"
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
struct Drag {std::uint64_t session,revision,catalogVersion,candidate;std::size_t group,index;std::uint32_t shortcut=0;};
void style(const Theme& theme,const Options& options) {
    auto& s=ImGui::GetStyle();s.WindowRounding=2;s.ChildRounding=0;s.FrameRounding=1;s.PopupRounding=1;s.ScrollbarRounding=2;
    s.WindowPadding={20,16};s.FramePadding={10,7};s.ItemSpacing={10,9};s.ItemInnerSpacing={6,6};s.WindowBorderSize=1;
    auto* c=s.Colors;c[ImGuiCol_Text]=ink;c[ImGuiCol_TextDisabled]=muted;
    c[ImGuiCol_WindowBg]={.055f,.055f,.06f,.985f};c[ImGuiCol_ChildBg]={.07f,.07f,.075f,.7f};
    c[ImGuiCol_PopupBg]={.07f,.07f,.075f,1};
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
    s.equipment=state.equippedDisplay;
    for(std::size_t i=0;i<Arsenal::count;++i) {
        const auto& g=state.draft.groups[i];auto& tile=s.groups[i];tile.count=g.entries.size();
        if(g.selected<state.rows[i].size()){const auto& row=state.rows[i][g.selected];tile.entry=row.entry;tile.available=row.available;tile.equipped=row.equipped;tile.quantity=row.quantity;tile.cooldown=row.cooldown;tile.voice=isVoice(row.entry)&&row.equipped;tile.consumable=i==selectedConsumableGroup(state.draft);}
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
static std::string modifierName(unsigned modifiers) {
    std::string name;
    if(modifiers&keyModifier::ctrl)name+="Ctrl + ";
    if(modifiers&keyModifier::shift)name+="Shift + ";
    if(modifiers&keyModifier::alt)name+="Alt + ";
    if(!name.empty())name.resize(name.size()-3);
    return name.empty()?tr("None"):name;
}
static std::string shortcutName(unsigned scan,unsigned modifiers) {
    if(!scan)return "None";
    return (modifiers?modifierName(modifiers)+" + ":std::string{})+scanName(scan);
}
void Editor::beginSession(const EditorState& state) {
    controllerMode_=state.controller;vrMode_=state.vr;
    if(!controllerMode_){pointerMode_=false;pointerAxis_={};if(pointerDown_){ImGui::GetIO().AddMouseButtonEvent(0,false);pointerDown_=false;}}
    auto& inputIO=ImGui::GetIO();inputIO.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;
    inputIO.BackendFlags|=ImGuiBackendFlags_HasGamepad;
    if(session_==state.session) {
        if(restoreSerial_!=state.restoreSerial){restoreSerial_=state.restoreSerial;for(std::size_t i=0;i<Arsenal::count;++i)std::snprintf(names_[i].data(),names_[i].size(),"%s",state.options.names[i].c_str());selected_=0;binding_=-1;builder_=0;shortcutIndex_.reset();editingLibrary_=0;comboCandidates_={};orderSelection_.reset();}
        if(orderSelection_&&orderSelection_->revision!=state.revision) {
            const auto saved=std::move(*orderSelection_);orderSelection_.reset();
            if(saved.group==state.draft.active) {
                const auto& group=state.draft.current();
                const auto locate=[&](const std::optional<Entry>& entry)->std::optional<std::size_t> {
                    if(entry)for(std::size_t i=0;i<group.entries.size();++i) {
                        const auto& value=group.entries[i];
                        if(entry->shortcut?entry->shortcut==value.shortcut:entry->same(value)||sameDefinition(*entry,value))return i;
                    }
                    return {};
                };
                if(saved.added)selected_=group.selected;
                else if(const auto index=locate(saved.selected))selected_=*index;
                if(saved.shortcut){shortcutIndex_=locate(saved.shortcut);if(!shortcutIndex_)builder_=0;}
            }
        }
        return;
    }
    restoreSerial_=state.restoreSerial;orderSelection_.reset();
    session_=state.session;catalogVersion_=state.catalogVersion;candidate_=0;selected_=0;binding_=-1;placement_=false;placementPanelTarget_=-1;returnTextFocus_=0;hint_.clear();downs_.clear();
    search_.fill(0);keys_.fill(false);padKeys_.fill(false);padDowns_.clear();pointerMode_=pointerDown_=false;pointerAxis_={};filter_=0;shortcutIndex_.reset();editingLibrary_=0;shortcutGroup_=Arsenal::count;shortcut_=smartEntry(0,0,0);comboName_.fill(0);builder_=0;comboCandidates_={};handSearch_={};
    for(std::size_t i=0;i<Arsenal::count;++i)std::snprintf(names_[i].data(),names_[i].size(),"%s",state.options.names[i].c_str());
    auto& io=ImGui::GetIO();io.ClearInputKeys();io.ClearInputMouse();mouse_={io.DisplaySize.x*.5f,io.DisplaySize.y*.5f};io.AddMousePosEvent(mouse_.x,mouse_.y);
}
void Editor::input(const std::vector<EditorInput>& events) {
    auto& io=ImGui::GetIO();
    const auto pointer=[&]{if(ImGui::GetCurrentContext()->ActiveIdSource==ImGuiInputSource_Gamepad)ImGui::ClearActiveID();};
    const auto navigation=[&]{pointerMode_=false;pointerAxis_={};if(pointerDown_){io.AddMouseButtonEvent(0,false);pointerDown_=false;}};
    for(const auto& e:events) {
        switch(e.type) {
        case EditorInputType::key:
            if(e.down)navigation();
            if(e.code==1&&e.down&&ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel))dismissingPopup_=true;
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
            if(e.x!=0||e.y!=0){navigation();pointer();}
            mouse_.x=std::clamp(mouse_.x+e.x,0.f,io.DisplaySize.x-1);mouse_.y=std::clamp(mouse_.y+e.y,0.f,io.DisplaySize.y-1);io.AddMousePosEvent(mouse_.x,mouse_.y);break;
        case EditorInputType::button:navigation();if(e.down)pointer();if(e.code<5)io.AddMouseButtonEvent(int(e.code),e.down);break;
        case EditorInputType::wheel:io.AddMouseWheelEvent(e.x,e.y);break;
        case EditorInputType::pad: {
            if(e.code==0||e.code>=unsigned(PadButton::count))break;
            padKeys_[e.code]=e.down;if(e.down)padDowns_.push_back(PadButton(e.code));
            if(e.down&&e.code>=unsigned(PadButton::up)&&e.code<=unsigned(PadButton::left))navigation();
            if(!vrMode_&&e.code==unsigned(PadButton::a)&&(pointerMode_||pointerDown_)) {
                pointerDown_=e.down;io.AddMouseButtonEvent(0,e.down);io.AddKeyEvent(ImGuiKey_GamepadFaceDown,false);break;
            }
            constexpr ImGuiKey keys[]{ImGuiKey_None,ImGuiKey_GamepadDpadUp,ImGuiKey_GamepadDpadRight,ImGuiKey_GamepadDpadDown,ImGuiKey_GamepadDpadLeft,
                ImGuiKey_GamepadFaceDown,ImGuiKey_GamepadFaceRight,ImGuiKey_GamepadFaceLeft,ImGuiKey_GamepadFaceUp,ImGuiKey_GamepadL1,ImGuiKey_GamepadR1,
                ImGuiKey_GamepadL2,ImGuiKey_GamepadR2,ImGuiKey_GamepadL3,ImGuiKey_GamepadR3,ImGuiKey_GamepadBack,ImGuiKey_GamepadStart};
            io.AddKeyEvent(keys[e.code],e.down);break;
        }
        case EditorInputType::stick: {
            if(!vrMode_&&e.code==1) {
                pointerAxis_={std::isfinite(e.x)?e.x:0,std::isfinite(e.y)?e.y:0};
                if(std::hypot(pointerAxis_.x,pointerAxis_.y)>.2f){pointer();pointerMode_=true;}
                break;
            }
            if(std::hypot(e.x,e.y)>.2f)navigation();
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
    if(pointerMode_) {
        const float length=std::hypot(pointerAxis_.x,pointerAxis_.y);
        if(length>.2f) {
            const float speed=std::pow(std::clamp((length-.2f)/.8f,0.f,1.f),1.7f)*std::min(io.DisplaySize.y,io.DisplaySize.x*.5625f)*.85f*std::clamp(io.DeltaTime,0.f,.05f)/length;
            mouse_.x=std::clamp(mouse_.x+pointerAxis_.x*speed,0.f,io.DisplaySize.x-1);
            mouse_.y=std::clamp(mouse_.y-pointerAxis_.y*speed,0.f,io.DisplaySize.y-1);
            io.AddMousePosEvent(mouse_.x,mouse_.y);
        }
    }
}
std::vector<EditCommand> Editor::draw(const EditorState& state,Hud& hud,double now) {
    const auto screen=ImGui::GetIO().DisplaySize;auto options=state.options;
    const auto metrics=editorGeometry(options,screen.x,screen.y);const auto px=[&](float value){return value*metrics.scale;};

    struct StyleGuard {float cursor;ImGuiStyle old=ImGui::GetStyle();~StyleGuard(){ImGui::GetStyle()=old;ImGui::GetStyle().MouseCursorScale=cursor;}} guard{metrics.scale};
    ImGui::GetStyle()=ImGuiStyle{};style(hud.themes().get(options.themeId),options);ImGui::GetStyle().ScaleAllSizes(metrics.scale);
    ImGui::PushFont(hud.bodyFont(options),px(18)*(metrics.width/metrics.scale<1000?.86f:1.f));
    std::vector<EditCommand> out;bool changed=false,listNavigation=false;
    const auto navigationList=[&]{listNavigation|=ImGui::GetCurrentContext()->NavWindow==ImGui::GetCurrentWindow();};
    std::vector<bool> skippedPanels;
    const auto beginPanel=[&](const char* name,ImVec2 size,ImGuiChildFlags flags=0,ImGuiWindowFlags windowFlags=0) {
        const bool open=ImGui::BeginChild(name,size,flags,windowFlags);
        auto& nav=*ImGui::GetCurrentContext();const auto* window=ImGui::GetCurrentWindow();
        bool skip=false;
        if(nav.NavMoveScoringItems&&nav.NavWindow&&!(nav.NavMoveFlags&ImGuiNavMoveFlags_IsTabbing)&&
            (nav.NavMoveDir==ImGuiDir_Up||nav.NavMoveDir==ImGuiDir_Down)) {
            auto source=nav.NavWindow->NavRectRel[nav.NavLayer];source.Translate(nav.NavWindow->Pos);
            skip=source.Max.x<window->OuterRectClipped.Min.x||source.Min.x>window->OuterRectClipped.Max.x;
        }

        skippedPanels.push_back(skip);if(skip)nav.NavMoveScoringItems=false;
        return open;
    };
    const auto endPanel=[&]{const bool skipped=skippedPanels.back();skippedPanels.pop_back();ImGui::EndChild();if(skipped)ImGui::GetCurrentContext()->NavMoveScoringItems=true;};
    const bool padCancel=std::find(padDowns_.begin(),padDowns_.end(),PadButton::b)!=padDowns_.end();
    const auto textField=[&](const char* id,const char* hint,auto& buffer,std::size_t limit=64) {
        auto& context=*ImGui::GetCurrentContext();
        const bool padActivate=state.controller&&context.NavActivateId==ImGui::GetID(id)&&context.NavInputSource==ImGuiInputSource_Gamepad;
        bool edited=UI::InputTextWithHint(id,hint,buffer.data(),buffer.size());
        const auto fieldID=ImGui::GetItemID();
        const bool pointerActivate=pointerMode_&&ImGui::IsItemClicked();
        if(returnTextFocus_==fieldID){ImGui::FocusItem();ImGui::SetNavCursorVisible(true);returnTextFocus_=0;}
        ImGui::PushID(id);
        if(padActivate||pointerActivate) {
            ImGui::ClearActiveID();std::snprintf(padText_.data(),padText_.size(),"%s",buffer.data());
            ImGui::OpenPopup(translatedLabel("Text entry").c_str());
        }

        ImGui::SetNextWindowSize({px(660),0},ImGuiCond_Always);
        if(ImGui::BeginPopupModal(translatedLabel("Text entry").c_str(),nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings)) {
            UI::TextWrapped("%s",padText_.data());ImGui::Separator();
            limit=std::min({limit,buffer.size()-1,padText_.size()-1});
            const auto append=[&](char ch){const auto n=std::strlen(padText_.data());if(n<limit){padText_[n]=ch;padText_[n+1]=0;}};
            const char* rows[]{"1234567890","qwertyuiop","asdfghjkl","zxcvbnm-_"};
            for(const auto* row:rows)for(unsigned col=0;row[col];++col) {
                if(col)ImGui::SameLine();const char ch=textUpper_?char(std::toupper(static_cast<unsigned char>(row[col]))):row[col];
                const char key[]{ch,0};ImGui::PushID(int(row[col]));if(UI::Button(key,{px(49),0}))append(ch);ImGui::PopID();
            }
            if(UI::Button(textUpper_?"abc":"ABC"))textUpper_=!textUpper_;ImGui::SameLine();
            if(UI::Button("Space"))append(' ');ImGui::SameLine();
            if(UI::Button("Backspace")){auto n=std::strlen(padText_.data());if(n){do{--n;}while(n&&(static_cast<unsigned char>(padText_[n])&0xc0)==0x80);padText_[n]=0;}}ImGui::SameLine();
            if(UI::Button("Clear text"))padText_.fill(0);
            ImGui::Separator();
            if(UI::Button("Done",{px(140),0})) {
                edited|=std::strcmp(buffer.data(),padText_.data())!=0;
                std::snprintf(buffer.data(),buffer.size(),"%s",padText_.data());returnTextFocus_=fieldID;ImGui::CloseCurrentPopup();
            }ImGui::SameLine();
            if(UI::Button("Cancel",{px(140),0})||padCancel||ImGui::IsKeyPressed(ImGuiKey_Escape,false)){returnTextFocus_=fieldID;ImGui::CloseCurrentPopup();}
            ImGui::EndPopup();
        }
        ImGui::PopID();return edited;
    };
    auto snapshot=preview(state);snapshot.browsing=previewOpen_;snapshot.controller=state.controller||state.vr;snapshot.vr=state.vr;
    const auto command=[&](EditAction action,std::size_t group,std::size_t index=0,std::size_t target=0) {
        EditCommand c;c.action=action;c.session=state.session;c.revision=state.revision;c.catalogVersion=state.catalogVersion;c.group=group;c.index=index;c.target=target;return c;
    };
    const auto emit=[&](EditCommand c){
        if(!out.empty())return;
        orderSelection_.reset();
        const auto active=state.draft.active;
        const bool created=(c.action==EditAction::saveShortcut||c.action==EditAction::saveCombination)&&!c.replace&&!c.libraryID;
        const bool sorting=c.action==EditAction::sortGroup||c.action==EditAction::reorder||c.action==EditAction::move||created||
            ((c.action==EditAction::add||c.action==EditAction::addShortcut)&&state.options.autoSort[c.group])||
            (c.action==EditAction::settings&&c.options.autoSort[active]&&!state.options.autoSort[active]);
        if(sorting) {
            OrderSelection saved;saved.revision=state.revision;saved.group=active;saved.added=created;
            const auto& entries=state.draft.current().entries;
            if(selected_<entries.size())saved.selected=entries[selected_];
            if(shortcutIndex_&&*shortcutIndex_<entries.size())saved.shortcut=entries[*shortcutIndex_];
            if(c.action==EditAction::move){saved.group=c.target;saved.added=true;saved.shortcut.reset();}
            orderSelection_=std::move(saved);
        }
        out.push_back(std::move(c));
    };
    const auto accept=[&](std::optional<std::size_t> index,std::size_t destination) {
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("QUICKSLOT_ENTRY")) {
                if(payload->DataSize==sizeof(Drag)) {
                    const auto d=*static_cast<const Drag*>(payload->Data);
                    if(d.session==state.session&&d.revision==state.revision) {
                        auto c=command(d.candidate?EditAction::add:d.group==destination?EditAction::reorder:EditAction::move,d.candidate?destination:d.group,d.index,destination);
                        if(d.shortcut){c.action=EditAction::addShortcut;c.group=destination;c.libraryID=d.shortcut;}
                        else if(d.candidate){c.candidate=d.candidate;c.catalogVersion=d.catalogVersion;}
                        else if(d.group==destination)c.target=index.value_or(state.draft.groups[destination].entries.empty()?0:state.draft.groups[destination].entries.size()-1);
                        emit(c);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    };
    if(catalogVersion_!=state.catalogVersion){
        catalogVersion_=state.catalogVersion;candidate_=0;
        if(!shortcutIndex_&&!editingLibrary_&&shortcut_.parts.size()==2)for(unsigned hand=0;hand<2;++hand)if(comboCandidates_[hand])shortcut_.parts[hand]={};
        comboCandidates_={};
    }
    const bool wasBinding=binding_!=-1;
    if(wasBinding&&padCancel){binding_=-1;hint_.clear();}
    for(const auto press:downs_) {
        const auto scan=press.scan;
        if(binding_!=-1) {
            if(binding_>=100) {
                if(modifierForScan(scan))continue;
                if(!assignSelectorBinding(options,KeyboardAction(binding_-100),scan==1?KeyBinding{}:KeyBinding{scan,press.modifiers})){hint_=tr("Key already assigned");continue;}
                changed=true;binding_=-1;hint_.clear();break;
            }
            if(scan==1){if(binding_>=0){options.keys[std::size_t(binding_)]=0;changed=true;}binding_=-1;hint_.clear();break;}
            if(modifierForScan(scan))continue;
            if(!validGroupKey(scan)){hint_=tr("Key unavailable");continue;}
            hint_.clear();
            if(binding_==-2) {
                bool duplicate=false;
                for(std::size_t i=0;i<options.keys.size();++i)if(groupVisible(options,i)&&options.keys[i]==scan)duplicate=true;
                if(duplicate||selectorKeyReserved(options,scan)){hint_=tr("Key already assigned");continue;}
                options.editorKey=scan;options.editorModifiers=press.modifiers;
            }else {
                bool swapped=false;
                for(std::size_t i=0;i<options.keys.size();++i)if(int(i)!=binding_&&groupVisible(options,i)&&options.keys[i]==scan)swapped=true;
                if(!assignGroupKey(options,std::size_t(binding_),scan)){hint_=tr("Key already assigned");continue;}
                if(swapped)hint_=tr("Group hotkeys swapped");
            }
            changed=true;binding_=-1;break;
        }
    }
    const bool shortcutPressed=std::any_of(downs_.begin(),downs_.end(),[&](const auto& press){return editorShortcutMatches(state.options,press.scan,press.modifiers);});
    if(!wasBinding&&((!dismissingPopup_&&ImGui::IsKeyPressed(ImGuiKey_Escape,false))||(!ImGui::GetIO().WantTextInput&&shortcutPressed)))emit(command(EditAction::close,state.draft.active));
    dismissingPopup_=false;
    if(!wasBinding&&std::find(padDowns_.begin(),padDowns_.end(),PadButton::start)!=padDowns_.end())emit(command(EditAction::close,state.draft.active));
    if(placement_&&std::find(padDowns_.begin(),padDowns_.end(),PadButton::b)!=padDowns_.end())placement_=false;
    padDowns_.clear();
    downs_.clear();
    const auto bindButton=[&](int index,unsigned key,const char* id) {
        const auto text=(binding_==index?std::string(tr("Press a key…")):shortcutName(key,index==-2?options.editorModifiers:0))+id;
        const float width=index==-2?std::max(px(150),ImGui::CalcTextSize(text.c_str(),nullptr,true).x+px(20)):px(95);
        if(UI::Button(text.c_str(),{width,0})){binding_=index;hint_=tr(index==-2?"Press a key · Esc cancels":"Press a key · Esc clears · Used group keys swap");}
        if(index>=0) {
            ImGui::SameLine();ImGui::BeginDisabled(!key);
            if(UI::Button((std::string("Clear")+id).c_str())){options.keys[std::size_t(index)]=0;binding_=-1;hint_.clear();changed=true;}
            ImGui::EndDisabled();
        }
    };
    auto& io=ImGui::GetIO();
    if(placement_) {
        snapshot.placementPreview=true;
        const auto frame=placementFrame(options,screen.x,screen.y);
        const float step=placementGridStep(options,screen.y);
        auto* guides=ImGui::GetBackgroundDrawList();
        if(options.placementGrid) {
            const auto thin=IM_COL32(225,224,218,38),axis=IM_COL32(245,239,219,180);
            const float centerX=(frame.min.x+frame.max.x)*.5f,centerY=(frame.min.y+frame.max.y)*.5f;
            for(float x=centerX-std::floor((centerX-frame.min.x)/step)*step;x<=frame.max.x;x+=step)guides->AddLine({x,frame.min.y},{x,frame.max.y},thin);
            for(float y=centerY-std::floor((centerY-frame.min.y)/step)*step;y<=frame.max.y;y+=step)guides->AddLine({frame.min.x,y},{frame.max.x,y},thin);
            guides->AddLine({centerX,frame.min.y},{centerX,frame.max.y},axis,px(1.5f));
            guides->AddLine({frame.min.x,centerY},{frame.max.x,centerY},axis,px(1.5f));
            if(options.placementFrame)guides->AddRect({frame.min.x,frame.min.y},{frame.max.x,frame.max.y},axis);
        }
        HudBounds bounds;hud.draw(snapshot,options,now,{},{},nullptr,&bounds);
        const auto geometry=hudGeometry(options,screen.x,screen.y,equipmentRows(snapshot.equipment));
        const auto detachSelector=[&] {
            if(options.selectorDock==4)return;
            options.selectorX=std::clamp((geometry.origin.x+geometry.selectorOrigin.x*geometry.scale-8)/std::max(1.f,screen.x-16-geometry.selectorWidth*geometry.scale),0.f,1.f);
            options.selectorY=std::clamp((geometry.origin.y+(geometry.selectorOrigin.y-32)*geometry.scale-8)/std::max(1.f,screen.y-16-(geometry.selectorHeight+32)*geometry.scale),0.f,1.f);
            options.selectorDock=4;changed=true;
        };
        const auto targetBox=[&](int target)->const Bounds&{return target==2?bounds.equipment:target==1?bounds.selector:bounds.widget;};
        const auto targetPosition=[&](int target){return Point{target==2?options.equipmentX:target==1?options.selectorX:options.positionX,target==2?options.equipmentY:target==1?options.selectorY:options.positionY};};
        const auto targetSize=[&](int target){return Point{target==2?geometry.equipment.width*geometry.equipment.scale:(target==1?geometry.selectorWidth:geometry.width)*geometry.scale,
            target==2?geometry.equipment.height*geometry.equipment.scale:(target==1?geometry.selectorHeight+32:geometry.height)*geometry.scale};};
        const auto move=[&](int target,float x,float y,bool snapX=true,bool snapY=true) {
            const auto& box=targetBox(target);const auto extent=targetSize(target);
            const auto& other=targetBox(target==0?1:0);
            const float originX=target==2?geometry.equipment.origin.x:geometry.origin.x+(target==1?geometry.selectorOrigin.x*geometry.scale:0);
            const float originY=target==2?geometry.equipment.origin.y:geometry.origin.y+(target==1?(geometry.selectorOrigin.y-32)*geometry.scale:0);
            if(options.placementSnap&&!io.KeyShift&&box.valid) {
                const bool alignOther=other.valid&&(target!=0||options.selectorDock==4);
                const auto horizontal=snapPlacementAxis(x,extent.x,screen.x,box.min.x-originX,box.max.x-originX,frame.min.x,frame.max.x,step,
                    alignOther?std::optional{std::array{other.min.x,other.max.x}}:std::nullopt);
                const auto vertical=snapPlacementAxis(y,extent.y,screen.y,box.min.y-originY,box.max.y-originY,frame.min.y,frame.max.y,step,
                    alignOther?std::optional{std::array{other.min.y,other.max.y}}:std::nullopt);
                if(snapX)x=horizontal.value;if(snapY)y=vertical.value;
                const auto color=IM_COL32(120,208,230,220);
                if(snapX&&horizontal.aligned)guides->AddLine({horizontal.guide,0},{horizontal.guide,screen.y},color);
                if(snapY&&vertical.aligned)guides->AddLine({0,vertical.guide},{screen.x,vertical.guide},color);
            }
            if(target==2){options.equipmentX=x;options.equipmentY=y;}
            else if(target==1){options.selectorX=x;options.selectorY=y;}else{options.positionX=x;options.positionY=y;}changed=true;
        };
        for(int target=0;target<3;++target) {
            const auto& box=targetBox(target);if(!box.valid)continue;
            const bool active=placementTarget_==target;
            const auto color=active?IM_COL32(120,208,230,240):IM_COL32(225,224,218,130);
            guides->AddRect({box.min.x,box.min.y},{box.max.x,box.max.y},color,0,0,px(active?1.5f:1));
            guides->AddLine({(box.min.x+box.max.x)*.5f-px(4),(box.min.y+box.max.y)*.5f},{(box.min.x+box.max.x)*.5f+px(4),(box.min.y+box.max.y)*.5f},color);
            guides->AddLine({(box.min.x+box.max.x)*.5f,(box.min.y+box.max.y)*.5f-px(4)},{(box.min.x+box.max.x)*.5f,(box.min.y+box.max.y)*.5f+px(4)},color);
            ImGui::SetNextWindowPos({box.min.x,box.min.y});ImGui::SetNextWindowSize({box.width(),box.height()});
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
            ImGui::Begin(target==2?"##equipment placement":target==1?"##selector placement":"##widget placement",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav);
            ImGui::InvisibleButton(target==2?"Drag equipment":target==1?"Drag selector":"Drag widget",ImGui::GetContentRegionAvail());
            if(ImGui::IsItemActivated()) {
                placementTarget_=target;if(target==1)detachSelector();
                const auto pos=targetPosition(target);dragStart_={pos.x,pos.y};
            }
            if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left,0)) {
                const auto delta=ImGui::GetMouseDragDelta(ImGuiMouseButton_Left,0);const auto extent=targetSize(target);
                move(target,std::clamp(dragStart_.x+delta.x/std::max(1.f,screen.x-16-extent.x),0.f,1.f),std::clamp(dragStart_.y+delta.y/std::max(1.f,screen.y-16-extent.y),0.f,1.f));
            }
            ImGui::End();ImGui::PopStyleVar();
        }
        for(const auto& box:bounds.tiles)if(box.valid)guides->AddRect({box.min.x,box.min.y},{box.max.x,box.max.y},IM_COL32(225,224,218,50));
        const auto& selected=targetBox(placementTarget_);
        const float panelWidth=std::min(px(330),screen.x-px(40));
        if(placementPanelTarget_!=placementTarget_){placementPanelTarget_=placementTarget_;placementPanelRight_=selected.min.x+selected.width()*.5f<screen.x*.5f;}
        const float panelX=placementPanelRight_?screen.x-panelWidth-px(20):px(20);
        ImGui::SetNextWindowPos({panelX,px(20)},ImGuiCond_Always);ImGui::SetNextWindowSize({panelWidth,0});
        ImGui::Begin("Position",nullptr,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoCollapse);
        if(UI::RadioButton("Widget",placementTarget_==0))placementTarget_=0;ImGui::SameLine();
        if(UI::RadioButton("Selector",placementTarget_==1)){placementTarget_=1;detachSelector();previewOpen_=true;}
        if(options.showEquipment){ImGui::SameLine();if(UI::RadioButton("Equipment",placementTarget_==2))placementTarget_=2;}
        const auto pos=targetPosition(placementTarget_);float x=pos.x,y=pos.y;
        ImGui::SetNextItemWidth(-px(40));const bool movedX=UI::SliderFloat("X",&x,0,1,"%.3f");
        ImGui::SetNextItemWidth(-px(40));const bool movedY=UI::SliderFloat("Y",&y,0,1,"%.3f");
        if(movedX||movedY)move(placementTarget_,x,y,movedX,movedY);
        changed|=UI::Checkbox("Grid",&options.placementGrid);ImGui::SameLine();changed|=UI::Checkbox("Snap to grid",&options.placementSnap);
        int spacing=int(options.placementSpacing),gridFrame=int(options.placementFrame);
        ImGui::SetNextItemWidth(-px(90));if(UI::Combo("Spacing",&spacing,"Fine\0Normal\0Coarse\0Custom\0")){options.placementSpacing=unsigned(spacing);changed=true;}
        if(options.placementSpacing==3){ImGui::SetNextItemWidth(-px(90));changed|=UI::SliderFloat("Grid size",&options.placementGridSize,4,128,"%.0f");}
        ImGui::SetNextItemWidth(-px(90));if(UI::Combo("Grid frame",&gridFrame,"Full screen\0" "16:9\0" "2:1\0" "21:9\0")){options.placementFrame=unsigned(gridFrame);changed=true;}
        ImGui::SetNextItemWidth(-px(90));
        if(placementTarget_==2)changed|=UI::SliderFloat("Scale",&options.equipmentScale,.5f,2.f,"%.2fx");
        else changed|=UI::SliderFloat("Scale",&options.scale,.55f,1.5f,"%.2fx");
        if(UI::Checkbox("Show open selector",&previewOpen_)&&!previewOpen_)placementTarget_=0;
        if(state.controller&&!state.vr)UI::TextWrapped("Right stick: cursor · %s: drag",padLabel(PadButton::a,options.controllerStyle).c_str());
        if(UI::Button("Back",{-1,0}))placement_=false;
        if(UI::Button("Close",{-1,0}))emit(command(EditAction::close,state.draft.active));
        ImGui::BeginDisabled(!state.canUndo);if(UI::Button("Undo",{-1,0}))emit(command(EditAction::undo,state.draft.active));ImGui::EndDisabled();
        ImGui::End();
    } else {
        const float w=metrics.width,h=metrics.height;
        ImGui::SetNextWindowPos({(screen.x-w)*.5f,(screen.y-h)*.5f},ImGuiCond_Always);ImGui::SetNextWindowSize({w,h});
        ImGui::Begin("Quickslot",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings);
        label(tr("Quickslot"));
        placementPanelTarget_=-1;
        const std::array<const char*,4> tabs{"Groups##main","Appearance","Controls","Presets"};
        for(std::size_t i=0;i<tabs.size();++i){if(i)ImGui::SameLine();const bool active=tab_==int(i);if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_Header));if(UI::Button(tabs[i],{px(i==1?130:114),0}))tab_=int(i);if(active)ImGui::PopStyleColor();}
        ImGui::Separator();
        const float contentHeight=std::max(px(120),ImGui::GetContentRegionAvail().y-px(87));
        if(tab_==0) {
        beginPanel("groups",{px(156),contentHeight},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
        label(tr("GROUPS"));
        for(std::size_t position=0;position<options.groupCount;++position) {
            const auto i=options.groupOrder[position];ImGui::PushID(int(i));
            const auto text=scanName(options.keys[i])+"   "+options.names[i]+"\n     "+std::to_string(state.draft.groups[i].entries.size())+" / 16";
            if(ImGui::Selectable(text.c_str(),i==state.draft.active,0,{0,px(48)})){selected_=0;emit(command(EditAction::select,i));}
            accept({},i);ImGui::PopID();
        }
        if(options.groupCount<Arsenal::count&&UI::Button("+ Add group",{-1,0}))emit(command(EditAction::addGroup,state.draft.active));
        ImGui::Separator();
        if(UI::Button("Group up",{-1,0}))emit(command(EditAction::groupUp,state.draft.active));
        if(UI::Button("Group down",{-1,0}))emit(command(EditAction::groupDown,state.draft.active));
        ImGui::BeginDisabled(state.draft.current().entries.empty());
        if(UI::Button("Clear group",{-1,0}))emit(command(EditAction::clearGroup,state.draft.active));ImGui::EndDisabled();
        ImGui::BeginDisabled(options.groupCount<=1);
        if(UI::Button("Remove group",{-1,0}))emit(command(EditAction::removeGroup,state.draft.active));ImGui::EndDisabled();
        endPanel();ImGui::SameLine();
        }
        beginPanel("workspace",{0,contentHeight},ImGuiChildFlags_NavFlattened|((tab_==2||tab_==3)?ImGuiChildFlags_Borders:0));
        const auto groupID=state.draft.active;const auto& group=state.draft.current();
        if(tab_==0) {
            ImGui::SetNextItemWidth(std::max(px(100),ImGui::GetContentRegionAvail().x-px(250)));
            if(textField("##Group name","",names_[groupID])){options.names[groupID]=names_[groupID].data();changed=true;}
            ImGui::SameLine();UI::TextDisabled("Key");ImGui::SameLine();bindButton(int(groupID),options.keys[groupID],"##group binding");
            const auto loadDefinition=[&](const Entry& entry) {
                shortcut_=entry;comboCandidates_={};
                builder_=shortcut_.kind==Kind::combo?1:shortcut_.kind==Kind::smartPotion?2:isSmartEquipment(shortcut_)?4:3;
                std::snprintf(comboName_.data(),comboName_.size(),"%s",shortcut_.kind==Kind::combo&&shortcut_.name!=combinationName(shortcut_)?shortcut_.name.c_str():"");
            };
            const auto loadShortcut=[&](std::size_t index) {shortcutIndex_=index;editingLibrary_=0;loadDefinition(group.entries[index]);};
            const auto loadLibrary=[&](const Entry& entry) {shortcutIndex_.reset();editingLibrary_=entry.shortcut;loadDefinition(entry);};
            if(shortcutGroup_!=groupID){shortcutGroup_=groupID;builder_=0;shortcutIndex_.reset();editingLibrary_=0;comboCandidates_={};}
            if(shortcutIndex_) {
                if(*shortcutIndex_>=group.entries.size()||!(group.entries[*shortcutIndex_].kind==Kind::combo||group.entries[*shortcutIndex_].kind==Kind::smartPotion||isSmartEquipment(group.entries[*shortcutIndex_])||isHandAction(group.entries[*shortcutIndex_]))) {shortcutIndex_.reset();editingLibrary_=0;builder_=0;}
                else if(shortcutRevision_!=state.revision)loadShortcut(*shortcutIndex_);
            }
            if(editingLibrary_) {
                const auto* definition=libraryShortcut(state.draft,editingLibrary_);
                if(!definition){editingLibrary_=0;builder_=0;}
                else if(shortcutRevision_!=state.revision)loadDefinition(*definition);
            }
            shortcutRevision_=state.revision;
            const auto& navigation=*ImGui::GetCurrentContext();
            const bool enterCatalog=navigation.NavMoveScoringItems&&navigation.NavMoveDir==ImGuiDir_Down&&
                (navigation.NavId==ImGui::GetID(translatedLabel("Items").c_str())||navigation.NavId==ImGui::GetID(translatedLabel("New").c_str())||navigation.NavId==ImGui::GetID("##group mode"));
            if(!group.entries.empty())selected_=std::min(selected_,group.entries.size()-1);
            const auto newShortcut=[&](int type) {
                builder_=type;shortcutIndex_.reset();editingLibrary_=0;comboCandidates_={};comboName_.fill(0);handSearch_={};
                if(type==1){shortcut_=Entry{};shortcut_.kind=Kind::combo;shortcut_.parts.resize(2);}
                else shortcut_=type==2?smartEntry(0,0,0):type>=4?smartEquipmentEntry(type==5,type==6?unsigned(WeaponFamily::shield):0u):unarmedEntry();
            };
            if(UI::Button("Items")){builder_=0;shortcutIndex_.reset();editingLibrary_=0;}
            ImGui::SameLine();if(UI::Button("New"))ImGui::OpenPopup("new entry");
            if(ImGui::BeginPopup("new entry")) {
                if(UI::MenuItem("Combination"))newShortcut(1);
                if(UI::MenuItem("Smart potion"))newShortcut(2);
                if(UI::MenuItem("Smart weapon"))newShortcut(4);
                if(UI::MenuItem("Smart shield"))newShortcut(6);
                if(UI::MenuItem("Smart ammo"))newShortcut(5);
                if(UI::MenuItem("Hand action"))newShortcut(3);
                ImGui::EndPopup();
            }
            ImGui::SameLine();ImGui::SetNextItemWidth(px(180));
            const bool consumables=hasConsumables(group);int mode=int(groupMode(options,state.draft,groupID));
            ImGui::BeginDisabled(consumables);
            if(UI::Combo("##group mode",&mode,"Hold to browse\0Tap to cycle\0")){options.groupModes[groupID]=GroupMode(mode);changed=true;}
            ImGui::EndDisabled();
            if(consumables&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",tr("Consumables: tap to use, hold to browse"));
            const auto builtCommand=[&] {
                auto c=command(shortcut_.kind==Kind::combo?EditAction::saveCombination:EditAction::saveShortcut,groupID,shortcutIndex_.value_or(0));
                c.shortcut=shortcut_;c.components=comboCandidates_;c.replace=shortcutIndex_.has_value();c.libraryID=editingLibrary_;
                if(shortcut_.kind==Kind::combo)c.shortcut.name=comboName_.data();
                c.continuous=ImGui::IsAnyItemActive();return c;
            };
            const auto saveBuiltShortcut=[&] {
                emit(builtCommand());if(!shortcutIndex_&&!editingLibrary_)builder_=0;
            };
            const float column=(ImGui::GetContentRegionAvail().x-px(12))*.5f;
            beginPanel("catalog",{column,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            if(builder_==0) {
            label(tr("ITEMS & SHORTCUTS"));
            ImGui::SetNextItemWidth(-1);textField("##search","Search…",search_,127);
            if(enterCatalog){auto& nav=*ImGui::GetCurrentContext();ImGui::NavMoveRequestResolveWithLastItem(&nav.NavMoveResultLocal);nav.NavMoveResultOther.Clear();}
            ImGui::SetNextItemWidth(-1);UI::Combo("##filter",&filter_,"All\0Weapons\0Spells\0Shields & torches\0Potions\0Food\0Poisons\0Scrolls\0Powers & shouts\0Ammo\0Shortcuts\0Combinations\0Smart selections\0Hand actions\0");
            std::vector<CatalogItem> library;library.reserve(state.draft.shortcuts.size());
            for(const auto& entry:state.draft.shortcuts){auto shown=entry;shown.name=displayName(entry);library.push_back({shown,(1ull<<63)|entry.shortcut,1,false,true,{}});}
            std::vector<const CatalogItem*> matches;
            for(const auto& c:state.catalog)if(catalogMatches(c,search_.data(),filter_))matches.push_back(&c);
            if(filter_==0||filter_>=10)for(const auto& c:library)if(shortcutMatches(c.entry,search_.data(),filter_))matches.push_back(&c);
            const auto pickedItem=[&]()->const CatalogItem* {for(const auto* c:matches)if(c->id==candidate_)return c;return nullptr;};
            const CatalogItem* picked=pickedItem();bool focusAdd=false;
            beginPanel("catalog list",{0,std::max(px(70),ImGui::GetContentRegionAvail().y-px(80))},ImGuiChildFlags_NavFlattened);
            navigationList();
            if(matches.empty())UI::TextWrapped("No matches");
            ImGuiListClipper clipper;clipper.Begin(int(matches.size()),px(32)+ImGui::GetStyle().ItemSpacing.y);
            while(clipper.Step())for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                const auto& c=*matches[std::size_t(i)];ImGui::PushID(c.entry.shortcut?"saved":"inventory");ImGui::PushID(int(c.id));const auto pos=ImGui::GetCursorScreenPos();
                if(ImGui::Selectable("##candidate",candidate_==c.id,ImGuiSelectableFlags_None,{0,px(32)})) {
                    candidate_=c.id;
                    focusAdd=c.addable&&ImGui::GetCurrentContext()->NavInputSource==ImGuiInputSource_Gamepad&&ImGui::GetCurrentContext()->NavActivateId==ImGui::GetItemID();
                }
                const auto end=ImGui::GetItemRectMax();auto* d=ImGui::GetWindowDrawList();d->PushClipRect(pos,end,true);
                std::string meta=c.equipped?tr("Equipped"):"";
                if(!c.entry.shortcut&&(isStack(c.entry)||c.count>1))meta+=(meta.empty()?"":" · ")+std::string("×")+std::to_string(c.count);
                const float metaWidth=ImGui::GetFont()->CalcTextSizeA(px(14),FLT_MAX,0,meta.c_str()).x;
                const ImVec2 textPos{pos.x+px(6),pos.y+(px(32)-ImGui::GetFontSize())*.5f};
                const float nameEnd=std::max(textPos.x,end.x-px(6)-(meta.empty()?0:metaWidth+px(14)));
                const auto nameSize=ImGui::CalcTextSize(c.entry.name.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text,c.addable?ink:muted);
                ImGui::RenderTextEllipsis(d,textPos,{nameEnd,end.y},nameEnd,c.entry.name.c_str(),nullptr,&nameSize);
                ImGui::PopStyleColor();
                d->AddText(nullptr,px(14),{end.x-px(6)-metaWidth,pos.y+px(10)},ImGui::GetColorU32(muted),meta.c_str());d->PopClipRect();
                if((nameSize.x>nameEnd-textPos.x||!c.detail.empty())&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::BeginTooltip();ImGui::TextUnformatted(c.entry.name.c_str());
                    if(!c.detail.empty())ImGui::TextUnformatted(c.detail.c_str());
                    ImGui::EndTooltip();
                }
                if(c.addable&&ImGui::BeginDragDropSource()) {
                    Drag drag{state.session,state.revision,state.catalogVersion,c.id,groupID,0,c.entry.shortcut};ImGui::SetDragDropPayload("QUICKSLOT_ENTRY",&drag,sizeof(drag));
                    ImGui::TextUnformatted(c.entry.name.c_str());ImGui::EndDragDropSource();
                }
                ImGui::PopID();ImGui::PopID();
            }
            endPanel();

            picked=pickedItem();
            ImGui::BeginDisabled(!picked||!picked->addable);
            const bool flexible=picked&&!picked->entry.shortcut&&flexibleHand(picked->entry);
            const auto add=[&](Hand hand){auto c=command(picked&&picked->entry.shortcut?EditAction::addShortcut:EditAction::add,groupID);c.candidate=candidate_;c.hand=hand;c.libraryID=picked?picked->entry.shortcut:0;emit(c);};
            if(flexible) {
                const float bw=(ImGui::GetContentRegionAvail().x-px(10))*.5f;
                if(UI::Button("Add right",{bw,0}))add(Hand::right);
                if(focusAdd)ImGui::FocusItem();
                ImGui::SameLine();if(UI::Button("Add left",{bw,0}))add(Hand::left);
            } else {
                if(UI::Button(picked&&picked->entry.twoHanded?"Add · both hands":"Add to group",{-1,0}))add(Hand::right);
                if(focusAdd)ImGui::FocusItem();
            }
            ImGui::EndDisabled();
            if(picked&&picked->entry.shortcut) {
                const float half=(ImGui::GetContentRegionAvail().x-px(10))*.5f;
                if(UI::Button("Edit shortcut",{half,0}))loadLibrary(*libraryShortcut(state.draft,picked->entry.shortcut));
                ImGui::SameLine();if(UI::Button("Delete shortcut",{half,0})){librarySelection_=picked->entry.shortcut;ImGui::OpenPopup("delete shortcut");}
            }else if(UI::Button("Refresh inventory",{-1,0}))emit(command(EditAction::refresh,groupID));
            if(ImGui::BeginPopup("delete shortcut")) {
                if(const auto* definition=libraryShortcut(state.draft,librarySelection_)) {
                    ImGui::TextUnformatted(displayName(*definition).c_str());
                    UI::Text("Remove from %u groups?",unsigned(shortcutUses(state.draft,librarySelection_)));
                    if(UI::Button("Delete")){auto c=command(EditAction::deleteShortcut,groupID);c.libraryID=librarySelection_;emit(c);candidate_=0;ImGui::CloseCurrentPopup();}
                    ImGui::SameLine();if(UI::Button("Cancel"))ImGui::CloseCurrentPopup();
                }else ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            } else {
            bool shortcutChanged=false;
            label(builder_==1?tr("COMBINATION"):builder_==2?tr("SMART POTION"):isSmartEquipment(shortcut_)?(shortcut_.kind==Kind::smartAmmo?tr("SMART AMMO"):smartShield(shortcut_)?tr("SMART SHIELD"):tr("SMART WEAPON")):tr("HAND ACTION"));
            if(isHandAction(shortcut_)) {
                int action=shortcut_.kind==Kind::unarmed?0:shortcut_.hand==Hand::right?1:2;
                ImGui::SetNextItemWidth(-px(70));
                if(UI::Combo("Action",&action,"Unarmed\0Empty right hand\0Empty left hand\0")){shortcut_=action==0?unarmedEntry():emptyHandEntry(action==1?Hand::right:Hand::left);shortcutChanged=true;}
                if(!shortcutIndex_&&!editingLibrary_&&UI::Button("Add action",{-1,0}))saveBuiltShortcut();
            } else if(isSmartEquipment(shortcut_)) {
                const bool ammo=shortcut_.kind==Kind::smartAmmo;int type=int(shortcut_.equipmentType),hand=int(shortcut_.hand);
                ImGui::SetNextItemWidth(-px(90));
                if(UI::Combo("Type",&type,ammo?"Arrows\0Bolts\0":"One-handed\0Two-handed\0Sword\0Dagger\0War axe\0Mace\0Greatsword\0Battleaxe\0Warhammer\0Bow\0Crossbow\0Shield\0")) {
                    shortcut_=smartEquipmentEntry(ammo,unsigned(type),Hand(hand));shortcutChanged=true;
                }
                if(flexibleHand(shortcut_)){ImGui::SetNextItemWidth(-px(90));if(UI::Combo("Hand",&hand,"Right\0Left\0")){shortcut_.hand=Hand(hand);shortcutChanged=true;}}
                if(!shortcutIndex_&&!editingLibrary_&&UI::Button(ammo?"Add smart ammo":smartShield(shortcut_)?"Add smart shield":"Add smart weapon",{-1,0}))saveBuiltShortcut();
                ImGui::Spacing();label(tr("CURRENT CHOICE"));
                if(const auto* chosen=chooseEquipment(shortcut_,state.equipment)) {
                    UI::TextWrapped("%s",chosen->entry.name.c_str());
                    UI::Text(smartShield(shortcut_)?"%.0f armor":"%.0f damage",chosen->damage);if(ammo){ImGui::SameLine();UI::Text("×%d",chosen->count);}
                }else ImGui::TextUnformatted(ammo?tr("No matching ammo"):smartShield(shortcut_)?tr("No matching shield"):tr("No matching weapon"));
            } else if(shortcut_.kind==Kind::smartPotion) {
                int resource=int(shortcut_.resource),effect=int(shortcut_.effect),policy=int(shortcut_.policy);
                ImGui::SetNextItemWidth(-px(100));if(UI::Combo("Resource",&resource,"Health\0Magicka\0Stamina\0")){shortcut_.resource=unsigned(resource);shortcutChanged=true;}
                ImGui::SetNextItemWidth(-px(100));if(UI::Combo("Effect",&effect,"Restoration\0Regeneration\0")){shortcut_.effect=unsigned(effect);if(effect&&shortcut_.policy==2)shortcut_.policy=0;shortcutChanged=true;}
                policy=int(shortcut_.policy);ImGui::SetNextItemWidth(-px(100));
                if(UI::Combo("Choose",&policy,shortcut_.effect?"Lowest strength\0Highest strength\0":"Lowest strength\0Highest strength\0Best fit\0")){shortcut_.policy=unsigned(policy);shortcutChanged=true;}
                shortcut_.name=smartName(shortcut_);shortcut_.icon=smartIcon(shortcut_.resource);
                if(!shortcutIndex_&&!editingLibrary_&&UI::Button("Add smart potion",{-1,0}))saveBuiltShortcut();
                ImGui::Spacing();label(tr("NEXT BOTTLE"));
                const auto* bottle=choosePotion(shortcut_,state.potions,state.deficits[shortcut_.resource]);
                const auto feedback=potionFeedback(shortcut_,bottle,state.deficits[shortcut_.resource]);
                if(bottle)UI::TextWrapped("%s ×%d",bottle->entry.name.c_str(),bottle->count);
                if(!bottle||(!shortcut_.effect&&shortcut_.policy==2&&state.deficits[shortcut_.resource]<.01f))UI::TextWrapped("%s",feedback.reason.c_str());
                if(!feedback.estimate.empty()&&UI::CollapsingHeader("Details"))UI::TextWrapped("%s",feedback.estimate.c_str());
                if(UI::CollapsingHeader("Excluded potions")) {
                beginPanel("potion exclusions",{0,px(190)},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
                navigationList();
                for(const auto& bottle:state.potions) {
                    const PotionExclusion ref{bottle.entry.form,bottle.entry.recipe};
                    const auto it=std::find_if(shortcut_.exclusions.begin(),shortcut_.exclusions.end(),[&](const auto& x){return exclusionMatches(x,bottle.entry);});bool excluded=it!=shortcut_.exclusions.end();
                    ImGui::PushID(int(bottle.entry.form));
                    if(UI::Checkbox(bottle.entry.name.c_str(),&excluded)) {
                        if(excluded&&shortcut_.exclusions.size()<128)shortcut_.exclusions.push_back(ref);
                        else if(!excluded)std::erase_if(shortcut_.exclusions,[&](const auto& x){return exclusionMatches(x,bottle.entry);});
                        shortcutChanged=true;
                    }ImGui::PopID();
                }
                endPanel();
                if(UI::Button("Clear exclusions")){shortcut_.exclusions.clear();shortcutChanged=true;}
                }

            } else {
                ImGui::SetNextItemWidth(-1);
                if(textField("##combination name","Name (optional)",comboName_))shortcutChanged=true;
                for(unsigned hand=0;hand<2;++hand) {
                    ImGui::PushID(int(hand));label(hand?tr("LEFT HAND"):tr("RIGHT HAND"));ImGui::SetNextItemWidth(-1);
                    const auto& part=shortcut_.parts[hand];
                    ImGui::SetNextWindowSizeConstraints({px(260),0},{px(480),px(340)});
                    if(ImGui::BeginCombo(hand?"##left component":"##right component",part.form||isHandAction(part)||isSmartEquipment(part)?displayName(part).c_str():tr("Choose…"),ImGuiComboFlags_HeightLarge)) {
                        if(ImGui::IsWindowAppearing()&&!state.controller)ImGui::SetKeyboardFocusHere();
                        ImGui::SetNextItemWidth(-1);textField("##hand search","Search…",handSearch_[hand],127);
                        const auto empty=emptyHandEntry(Hand(hand));
                        if(ImGui::Selectable(displayName(empty).c_str())){shortcut_.parts[hand]=empty;comboCandidates_[hand]=0;shortcutChanged=true;}
                        for(unsigned type=0;type<unsigned(WeaponFamily::count);++type) {
                            const auto rule=smartEquipmentEntry(false,type,Hand(hand));
                            if(!pairable(rule)||rule.hand!=Hand(hand)||!textContains(displayName(rule),handSearch_[hand].data()))continue;
                            ImGui::PushID(int(type));
                            if(ImGui::Selectable(displayName(rule).c_str(),part.same(rule))){shortcut_.parts[hand]=rule;comboCandidates_[hand]=0;shortcutChanged=true;}
                            ImGui::PopID();
                        }
                        ImGui::Separator();
                        for(const auto& choice:state.catalog)if(handCandidate(choice,Hand(hand))&&catalogMatches(choice,handSearch_[hand].data(),0)) {
                            ImGui::PushID(int(choice.id));
                            const auto name=choice.entry.name+(choice.count>1?" ×"+std::to_string(choice.count):"");
                            if(ImGui::Selectable(name.c_str(),comboCandidates_[hand]==choice.id)) {
                                shortcut_.parts[hand]=choice.entry;shortcut_.parts[hand].hand=Hand(hand);comboCandidates_[hand]=choice.id;shortcutChanged=true;
                            }
                            if(!choice.detail.empty()&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))UI::SetTooltip("%s",choice.detail.c_str());
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    if(part.kind==Kind::spell&&part.form) {
                        const auto it=std::find_if(state.catalog.begin(),state.catalog.end(),[&](const auto& item){return item.entry.form==part.form&&handCandidate(item,Hand(1-hand));});
                        ImGui::BeginDisabled(it==state.catalog.end());
                        if(UI::Button("Both hands")&&it!=state.catalog.end()) {
                            for(unsigned target=0;target<2;++target){shortcut_.parts[target]=it->entry;shortcut_.parts[target].hand=Hand(target);comboCandidates_[target]=it->id;}
                            shortcutChanged=true;
                        }ImGui::EndDisabled();
                    }
                    ImGui::PopID();
                }

                const bool canSwap=flexibleHand(shortcut_.parts[0])&&flexibleHand(shortcut_.parts[1])&&pairable(shortcut_.parts[0])&&pairable(shortcut_.parts[1]);
                ImGui::BeginDisabled(!canSwap);
                if(UI::Button("Swap hands")) {
                    std::swap(shortcut_.parts[0],shortcut_.parts[1]);std::swap(comboCandidates_[0],comboCandidates_[1]);
                    for(unsigned hand=0;hand<2;++hand) {
                        auto& part=shortcut_.parts[hand];part.hand=Hand(hand);
                        if(isHandAction(part))part.name=handActionName(part);
                        else if(!isSmartEquipment(part)&&!comboCandidates_[hand])for(const auto& choice:state.catalog){auto copy=choice.entry;copy.hand=Hand(hand);if(copy.same(part)){comboCandidates_[hand]=choice.id;break;}}
                    }
                    shortcutChanged=true;
                }ImGui::EndDisabled();ImGui::SameLine();
                ImGui::BeginDisabled(!state.equippedPair);
                if(UI::Button("Use equipped")) {
                    for(unsigned hand=0;hand<2;++hand){const auto* item=catalogItem(state,state.equippedCandidates[hand]);shortcut_.parts[hand]=item?item->entry:emptyHandEntry(Hand(hand));shortcut_.parts[hand].hand=Hand(hand);comboCandidates_[hand]=item?item->id:0;}
                    shortcutChanged=true;
                }ImGui::EndDisabled();
                const bool valid=validCombinationDraft(state,builtCommand());
                ImGui::BeginDisabled(!valid||(!shortcutIndex_&&!editingLibrary_&&group.entries.size()>=Group::limit));
                if(!shortcutIndex_&&!editingLibrary_&&UI::Button("Add combination",{-1,0}))saveBuiltShortcut();
                if(!valid&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled|ImGuiHoveredFlags_DelayNormal))UI::SetTooltip("Choose compatible items for both hands");
                ImGui::EndDisabled();
                if(!valid)shortcutChanged=false;
            }
            if((shortcutIndex_||editingLibrary_)&&shortcutChanged)emit(builtCommand());
            const auto uses=shortcutUses(state.draft,editingLibrary_?editingLibrary_:shortcut_.shortcut);
            if(uses>1){ImGui::Spacing();UI::TextDisabled("Used in %u groups",unsigned(uses));}
            if(editingLibrary_&&UI::Button("Add to group",{-1,0})){auto c=command(EditAction::addShortcut,groupID);c.libraryID=editingLibrary_;emit(c);}
            }
            endPanel();ImGui::SameLine();
            beginPanel("entries",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label(tr("GROUP ITEMS"));
            ImGui::SameLine();ImGui::BeginDisabled(group.entries.size()<2);
            if(UI::Button("Sort A–Z"))emit(command(EditAction::sortGroup,groupID));
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered())UI::SetTooltip("Sort by name, ignoring case. Equal names keep their order.");
            changed|=UI::Checkbox("Auto-sort when adding",&options.autoSort[groupID]);
            if(ImGui::IsItemHovered())UI::SetTooltip("Sort A–Z now and whenever an item is added to this group.");
            beginPanel("group list",{0,std::max(px(70),ImGui::GetContentRegionAvail().y-px(123))},ImGuiChildFlags_NavFlattened);
            navigationList();
            if(group.entries.empty()) {
                UI::Button("Drop equipment here",{-1,px(100)});accept({},groupID);
            }
            for(std::size_t i=0;i<group.entries.size();++i) {
                ImGui::PushID(int(i));const auto& e=group.entries[i];const auto pos=ImGui::GetCursorScreenPos();
                if(ImGui::Selectable("##entry",selected_==i,0,{0,px(47)})) {
                    selected_=i;
                    if(e.kind==Kind::combo||e.kind==Kind::smartPotion||isSmartEquipment(e)||isHandAction(e))loadShortcut(i);
                    else {builder_=0;shortcutIndex_.reset();editingLibrary_=0;}
                }
                const auto end=ImGui::GetItemRectMax();auto* d=ImGui::GetWindowDrawList();d->PushClipRect(pos,end,true);
                const bool available=i<state.rows[groupID].size()&&state.rows[groupID][i].available;
                const auto name=std::to_string(i+1)+"  "+displayName(i<state.rows[groupID].size()?state.rows[groupID][i].entry:e);const auto nameSize=ImGui::CalcTextSize(name.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text,available?ink:muted);
                ImGui::RenderTextEllipsis(d,{pos.x+px(6),pos.y+px(4)},{end.x-px(6),end.y},end.x-px(6),name.c_str(),nullptr,&nameSize);
                ImGui::PopStyleColor();
                const auto status=i<state.rows[groupID].size()?rowStatus(state.rows[groupID][i]):handName(e);
                d->AddText(nullptr,px(14),{pos.x+px(6),pos.y+px(27)},ImGui::GetColorU32(brass),status.c_str());d->PopClipRect();
                const auto statusWidth=ImGui::GetFont()->CalcTextSizeA(px(14),FLT_MAX,0,status.c_str()).x;
                if(std::max(nameSize.x,statusWidth)>end.x-pos.x-px(12)&&ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))UI::SetTooltip("%s\n%s",e.name.c_str(),status.c_str());
                if(ImGui::BeginDragDropSource()) {
                    Drag drag{state.session,state.revision,state.catalogVersion,0,groupID,i};ImGui::SetDragDropPayload("QUICKSLOT_ENTRY",&drag,sizeof(drag));
                    ImGui::TextUnformatted(displayName(e).c_str());ImGui::EndDragDropSource();
                }
                accept(i,groupID);ImGui::PopID();
            }
            endPanel();
            const bool has=selected_<group.entries.size();ImGui::BeginDisabled(!has);
            if(UI::Button("Remove")){emit(command(EditAction::remove,groupID,selected_));shortcutIndex_.reset();editingLibrary_=0;builder_=0;}ImGui::SameLine();
            ImGui::BeginDisabled(!has||selected_==0);if(UI::Button("Up"))emit(command(EditAction::reorder,groupID,selected_,selected_-1));ImGui::EndDisabled();ImGui::SameLine();
            ImGui::BeginDisabled(!has||selected_+1>=group.entries.size());if(UI::Button("Down"))emit(command(EditAction::reorder,groupID,selected_,selected_+1));ImGui::EndDisabled();
            ImGui::BeginDisabled(!has||!flexibleHand(group.entries[has?selected_:0]));
            if(UI::Button("Change hand",{-1,0})&&has){auto c=command(EditAction::hand,groupID,selected_);c.hand=group.entries[selected_].hand==Hand::left?Hand::right:Hand::left;emit(c);}ImGui::EndDisabled();
            ImGui::SetNextItemWidth(-1);
            if(ImGui::BeginCombo("##move","Move to another group…")) {
                for(std::size_t position=0;position<options.groupCount;++position){const auto i=options.groupOrder[position];if(i!=groupID&&ImGui::Selectable(options.names[i].c_str()))emit(command(EditAction::move,groupID,selected_,i));}
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();endPanel();
        } else if(tab_==2) {
            for(int i=0;i<3;++i){if(i)ImGui::SameLine();if(UI::Button(i==0?"General":i==1?"Keyboard":"Controller",{px(125),0}))controlsPage_=i;}
            ImGui::Separator();
            if(controlsPage_==0){
            label(tr("SELECTION"));
            changed|=UI::Checkbox("Skip unavailable items",&options.skipEmpty);
            changed|=UI::Checkbox("Add from Inventory / Magic",&options.menuCapture);
            if(ImGui::IsItemHovered())UI::SetTooltip("Disable to leave Inventory / Magic capture to another mod. Capture also requires its right- or left-hand shortcut to be enabled under Keyboard.");
            label(tr("Game speed while selecting"));int timeMode=int(selectionClock(options.selectionSpeed));ImGui::SetNextItemWidth(px(300));
            if(UI::Combo("##selection clock",&timeMode,"Full speed\0Slow motion\0Pause\0")){options.selectionSpeed=timeMode==2?0.f:timeMode==1?.5f:1.f;changed=true;}
            if(timeMode==1&&!state.soulsSlowdown){ImGui::SetNextItemWidth(px(200));changed|=UI::SliderFloat("Game speed",&options.selectionSpeed,.1f,.95f,"%.2fx");}
            ImGui::Separator();label(tr("DEFAULT ACTIVATION"));
            changed|=UI::Checkbox("Activate with group hotkey",&options.voiceHotkey);
            ImGui::SetNextItemWidth(px(240));changed|=UI::SliderFloat("Shout charge",&options.shoutCharge,.05f,2.5f,"%.2f s");
            }else if(controlsPage_==1){
            label(tr("KEYS"));
            for(std::size_t i=0;i<options.groupCount;++i){const auto id=options.groupOrder[i];ImGui::PushID(int(id));bindButton(int(id),options.keys[id],"##control binding");ImGui::SameLine();ImGui::TextUnformatted(options.names[id].c_str());ImGui::PopID();}
            ImGui::Separator();label(tr("Editor"));bindButton(-2,options.editorKey,"##editor binding");
            ImGui::SameLine();if(UI::Button("Clear##editor binding")){options.editorKey=options.editorModifiers=0;binding_=-1;changed=true;hint_.clear();}
            ImGui::Separator();label(tr("Actions"));
            UI::TextWrapped("Configure each action in one place. Choose None or clear a binding to disable it.");
            if(UI::Button("Remap actions…"))ImGui::OpenPopup("Keyboard action bindings");
            ImGui::SetNextWindowSize({std::min(px(560),screen.x-40),std::min(px(540),screen.y-40)},ImGuiCond_Appearing);
            ImGui::SetNextWindowSizeConstraints({0,0},{screen.x-40,screen.y-40});
            if(ImGui::BeginPopup("Keyboard action bindings")) {
                const char* groupNames[]{"Add right hand / highlighted item","Add left hand","Move to another group","Unequip group item","Remove group shortcut","Reorder while browsing"};
                UI::TextWrapped("Choose the modifier held with a group or navigation key. None disables that action. Assigning an existing binding swaps the two actions.");
                for(unsigned i=0;i<options.groupModifiers.size();++i) {
                    ImGui::PushID(int(i));ImGui::SetNextItemWidth(px(185));
                    const auto selected=groupActionEnabled(options,GroupModifier(i))?options.groupModifiers[i]:0u;
                    if(ImGui::BeginCombo("##action modifier",modifierName(selected).c_str())) {
                        for(unsigned mask=0;mask<=keyModifier::all;++mask)if(ImGui::Selectable(modifierName(mask).c_str(),mask==selected)){assignGroupModifier(options,GroupModifier(i),mask);changed=true;}
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine();ImGui::TextUnformatted(tr(groupNames[i]));ImGui::PopID();
                }
                changed|=UI::Checkbox("Allow group selection with unused modifiers",&options.allowUnusedModifiers);
                UI::TextWrapped("Enable to use group keys while holding a walking or other unused modifier. Set the conflicting action to None or choose another modifier first.");
                ImGui::Separator();
                const char* actionNames[]{"Previous item","Next item","Unequip highlighted item","Remove highlighted shortcut","Cancel selection"};
                for(unsigned i=0;i<options.selectorBindings.size();++i) {
                    const auto binding=selectorBinding(options,KeyboardAction(i));ImGui::PushID(int(100+i));
                    const auto text=binding_==int(100+i)?std::string(tr("Press a key…")):shortcutName(binding.key,binding.modifiers);
                    if(UI::Button((text+"##action key").c_str(),{px(185),0})){binding_=int(100+i);hint_=tr("Press a key · Esc clears");}
                    ImGui::SameLine();if(UI::Button("Clear##action key")){assignSelectorBinding(options,KeyboardAction(i),{});if(binding_==int(100+i))binding_=-1;changed=true;}
                    ImGui::SameLine();ImGui::TextUnformatted(tr(actionNames[i]));ImGui::PopID();
                }
                if(UI::Button("Reset keyboard actions")){setEditingShortcuts(options,true);options.groupModifiers=defaultGroupModifiers;options.selectorBindings=defaultSelectorBindings;options.allowUnusedModifiers=false;binding_=-1;changed=true;}
                if(!hint_.empty())ImGui::TextWrapped("%s",hint_.c_str());
                ImGui::EndPopup();
            }
            if(UI::CollapsingHeader("Shortcuts")) {
            ImGui::TextUnformatted(tr("Hold + scroll / navigation keys: select"));ImGui::TextUnformatted(tr("Tap: group action"));
            const auto cancel=options.selectorBindings[unsigned(KeyboardAction::cancel)];ImGui::Text("%s / %s",shortcutName(cancel.key,cancel.modifiers).c_str(),tr("right-click: cancel"));
            }
            }else{
            changed|=UI::Checkbox("Enable controller",&options.controllerEnabled);
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
                    if(!state.vr)for(const auto& other:options.controllerActions)if(&other!=&value&&value.button!=PadButton::none&&other==value)duplicate=true;
                    if(value.button==PadButton::none)value.modifier=PadButton::none;
                    if(duplicate||!validPadBinding(value)){value=old;hint_=duplicate?tr("Button combination already assigned"):tr("Choose different buttons");}else hint_.clear();
                }
            };
            UI::TextDisabled("Modifier");ImGui::SameLine(px(175));UI::TextDisabled("Button");
            for(std::size_t i=0;i<options.groupCount;++i){const auto id=options.groupOrder[i];padBinding(groups[id],options.names[id].c_str(),unsigned(id));}
            ImGui::Separator();padBinding(editorBinding,tr("Open editor"),8);
            int stick=int(browseStick);ImGui::SetNextItemWidth(px(220));if(UI::Combo("Browse with",&stick,state.vr?"Off-hand stick / pad\0Main-hand stick / pad\0Off\0":"Left stick\0Right stick\0Off\0")){browseStick=unsigned(stick);changed=true;}
            if(!state.vr)changed|=UI::Checkbox("Enable previous / next buttons",&options.controllerBumpers);
            ImGui::SetNextItemWidth(px(220));changed|=UI::SliderFloat("Stick deadzone",&options.controllerDeadzone,.2f,.9f,"%.2f");
            if(!state.vr){int buttonStyle=int(options.controllerStyle);ImGui::SetNextItemWidth(px(220));if(UI::Combo("Button icons",&buttonStyle,"Xbox\0PlayStation\0")){options.controllerStyle=unsigned(buttonStyle);changed=true;}}
            if(!state.vr&&UI::CollapsingHeader("Browsing actions")) {
                const char* names[]{"Cancel selection","Unequip highlighted item","Previous item","Next item"};
                for(unsigned i=0;i<options.controllerActions.size();++i)padBinding(options.controllerActions[i],tr(names[i]),100+i);
                UI::TextWrapped("A button used as an explicit modifier does not also trigger a single-button browsing action. Rebind that action or use a chord.");
                if(UI::Button("Reset controller actions")){options.controllerActions=defaultControllerActions;changed=true;}
            }
            if(!state.vr&&UI::CollapsingHeader("Modifier buttons")) {
                for(unsigned b=1;b<unsigned(PadButton::count);++b) {
                    const auto mask=padButtonMask(PadButton(b));bool enabled=(options.controllerModifiers&mask)!=0;
                    if((b-1)%4)ImGui::SameLine(px(20+155*((b-1)%4)));
                    ImGui::PushID(int(b));if(UI::Checkbox(padLabel(PadButton(b),style).c_str(),&enabled)){if(enabled)options.controllerModifiers|=mask;else options.controllerModifiers&=~mask;changed=true;}ImGui::PopID();
                }
            }
            if(UI::CollapsingHeader("Shortcuts")) {
            ImGui::TextUnformatted(tr("Hold: browse  |  Release: select"));
            if(state.vr)ImGui::TextUnformatted(tr("Tap: group action  |  Main B/Y: cancel  |  Off A/X: unequip"));
            else ImGui::Text("%s: %s | %s: %s",padLabel(options.controllerActions[0],style).c_str(),tr("Cancel selection"),padLabel(options.controllerActions[1],style).c_str(),tr("Unequip highlighted item"));
            if(state.vr){ImGui::TextUnformatted(tr("Point + trigger: select"));ImGui::TextUnformatted(tr("Main / Off: Skyrim handedness"));}
            else {ImGui::TextUnformatted(tr("Editor: D-pad / left stick to navigate, A / Cross to confirm"));ImGui::TextUnformatted(tr("Right stick: cursor · A / Cross: click or drag"));ImGui::TextUnformatted(tr("Start / Options: close editor"));}
            }
            }
        } else if(tab_==3) {
            label(tr("EXPORT"));
            ImGui::SetNextItemWidth(px(340));textField("##preset name","Preset name",presetName_);
            ImGui::SetNextItemWidth(px(340));UI::Combo("##export scope",&exportScope_,"Appearance only\0Complete setup\0");
            if(*presetName_.data()&&!validPresetName(presetName_.data()))UI::TextWrapped("Use letters, numbers, spaces, - or _");
            ImGui::BeginDisabled(!validPresetName(presetName_.data()));
            const bool exists=std::find(state.presets.begin(),state.presets.end(),presetName_.data())!=state.presets.end();
            if(UI::Button(exists?"Update preset":"Export preset",{px(160),0})){auto c=command(EditAction::exportPreset,groupID);c.presetName=presetName_.data();c.appearanceOnly=exportScope_==0;c.replace=exists;emit(c);}ImGui::EndDisabled();
            ImGui::Separator();label(tr("IMPORT"));
            ImGui::SetNextItemWidth(px(340));
            if(ImGui::BeginCombo("##preset file",presetFile_.empty()?tr("Select preset"):presetFile_.c_str())){
                for(const auto& name:state.presets)if(ImGui::Selectable(name.c_str(),name==presetFile_))presetFile_=name;
                ImGui::EndCombo();
            }
            ImGui::SameLine();if(UI::Button("Refresh##presets"))emit(command(EditAction::refreshPresets,groupID));
            UI::Checkbox("Appearance only",&importAppearance_);
            if(!importAppearance_)UI::TextWrapped("Replaces groups and controls. Items may need reassignment.");
            ImGui::BeginDisabled(presetFile_.empty());
            if(UI::Button("Import preset",{px(160),0})){auto c=command(EditAction::importPreset,groupID);c.presetName=presetFile_;c.appearanceOnly=importAppearance_;emit(c);}ImGui::EndDisabled();
        } else {
            const std::array<const char*,5> sections{"Style","Groups","Selector","Equipment","Visibility"};
            for(int i=0;i<5;++i){if(i)ImGui::SameLine();const bool active=appearancePage_==i;if(active)ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_Header));if(UI::Button(sections[std::size_t(i)],{px(110),0})){appearancePage_=i;previewMode_=i==2?1:i==3?3:0;}if(active)ImGui::PopStyleColor();}
            ImGui::Separator();
            const float controlsWidth=std::min(px(340),ImGui::GetContentRegionAvail().x*.44f);
            beginPanel("appearance",{controlsWidth,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            if(UI::Button("Position",{-1,0})){placement_=true;placementTarget_=appearancePage_==3&&options.showEquipment?2:0;}
            const auto id=options.themeId;const auto& currentTheme=hud.themes().get(id);
            const auto slider=[&](const char* name,float& value,float low,float high,const char* format="%.0f"){label(tr(name));ImGui::SetNextItemWidth(-1);const auto key=std::string("##")+name;changed|=UI::SliderFloat(key.c_str(),&value,low,high,format);};
            const auto combo=[&](const char* name,const char* key,unsigned& value,const char* choices){label(tr(name));int current=int(value);ImGui::SetNextItemWidth(-1);if(UI::Combo(key,&current,choices)){value=unsigned(current);changed=true;}};
            if(appearancePage_==0) {
                label(tr("Language"));ImGui::SetNextItemWidth(-1);
                const char* languageName=options.language=="auto"?tr("Automatic"):options.language=="en"?"English":options.language.c_str();
                for(const auto& pack:localization().packs())if(pack->id==options.language)languageName=pack->name.c_str();
                if(ImGui::BeginCombo("##language",languageName)) {
                    if(ImGui::Selectable(tr("Automatic"),options.language=="auto")){options.language="auto";changed=true;}
                    if(ImGui::Selectable("English",options.language=="en")){options.language="en";changed=true;}
                    for(const auto& pack:localization().packs())if(pack->id!="en"&&ImGui::Selectable(pack->name.c_str(),options.language==pack->id)){options.language=pack->id;changed=true;}
                    ImGui::EndCombo();
                }
                label(tr("Theme"));ImGui::SetNextItemWidth(-1);
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
                if(!hud.themes().contains(id))UI::TextWrapped("Missing theme: %s",id.c_str());
                slider("Widget scale",options.scale,.55f,1.5f,"%.2fx");slider("Text size",options.textScale,.75f,1.5f,"%.2fx");slider("Opacity",options.opacity,.4f,1,"%.2f");
                ImGui::Separator();auto tweak=adjustments(options);bool tuned=false;
                if(UI::Checkbox("Custom accent",&tweak.customAccent)){tuned=true;if(tweak.customAccent){const auto color=ImGui::ColorConvertU32ToFloat4(currentTheme.accent);tweak.accent={color.x,color.y,color.z};}}
                if(tweak.customAccent){ImGui::SetNextItemWidth(-1);tuned|=ImGui::ColorEdit3("##accent",tweak.accent.data(),ImGuiColorEditFlags_NoInputs);}
                label(tr("Frame opacity"));ImGui::SetNextItemWidth(-1);tuned|=UI::SliderFloat("##Frame opacity",&tweak.frameOpacity,0,1,"%.2f");
                label(tr("Icon size"));ImGui::SetNextItemWidth(-1);tuned|=UI::SliderFloat("##Icon size",&tweak.iconScale,.65f,1.2f,"%.2fx");
                if(tuned){options.themeAdjustments[options.themeId]=tweak;changed=true;}
                if(hud.iconSets().size()>1) {
                label(tr("Icon set"));ImGui::SetNextItemWidth(-1);
                const auto currentIcons=hud.iconSets().find(options.itemIconSet);
                if(ImGui::BeginCombo("##item icon set",currentIcons==hud.iconSets().end()?tr("Quickslot"):currentIcons->second.name.c_str())) {
                    for(const auto& [id,pack]:hud.iconSets())if(ImGui::Selectable(pack.name.c_str(),options.itemIconSet==id)){options.itemIconSet=id;changed=true;}
                    ImGui::EndCombo();
                }
                }
                combo("Icon color","##item icon color",options.itemIconColor,"Original\0Theme\0White\0");
                if(options.itemIconColor==1)changed|=UI::Checkbox("Tint magic icons",&options.coloredMagic);
                changed|=UI::Checkbox("Key icons",&options.keyIcons);
                if(UI::Button("Reset theme",{-1,0})){options.themeAdjustments.erase(options.themeId);changed=true;}
            } else if(appearancePage_==1) {
                label(tr("Layout presets"));
                const auto preset=[&](unsigned n){layoutPreset(options,n);changed=true;};
                if(UI::Button("Cross layout",{-1,0}))preset(0);
                if(UI::Button("Diamond layout",{-1,0}))preset(1);
                if(UI::Button("Circle layout",{-1,0}))preset(2);
                ImGui::Separator();
                combo("Arrangement","##layout",options.layout,"Horizontal\0Vertical\0Diamond\0Cross\0Circle\0Staggered cross\0");
                combo("Tile shape","##shape",options.shape,"Cards\0Circles\0Squares\0Diamonds\0");
                slider("Spacing",options.spacing,0,40);
                changed|=UI::Checkbox("Show group names",&options.showLabels);changed|=UI::Checkbox("Show keys",&options.showKeys);
                if(options.shape==0)changed|=UI::Checkbox("Show card item names",&options.showCardNames);
            } else if(appearancePage_==2) {
                combo("Arrangement","##selector",options.selector,"Horizontal strip\0Arc\0Ring\0Vertical strip\0List\0");
                if(options.selector!=4)combo("Item shape","##selector shape",options.selectorShape,"Automatic\0Circles\0Squares\0Diamonds\0");
                slider("Spacing",options.selectorSpacing,0,32);
                combo("Position","##selector dock",options.selectorDock,"Above widget\0Below widget\0Left of widget\0Right of widget\0Independent\0");
                if(options.selectorDock!=4)slider("Distance",options.selectorGap,0,100);
                changed|=UI::Checkbox("Animate scrolling",&options.animateScroll);
                changed|=UI::Checkbox("Show selector hints",&options.showHints);
            } else if(appearancePage_==3) {
                label(tr("Visibility"));ImGui::SetNextItemWidth(-1);int visibility=int(equipmentVisibility(options));
                if(UI::Combo("##equipment visibility",&visibility,"Off\0Show always\0Dim when idle\0")){setEquipmentVisibility(options,EquipmentVisibility(visibility));changed=true;}
                if(options.showEquipment&&options.equipmentDim) {
                    slider("Idle delay",options.equipmentFadeDelay,0,20,"%.1f s");
                    slider("Idle opacity",options.equipmentIdleOpacity,.05f,.9f,"%.2f");
                }
                combo("Style","##equipment style",options.equipmentStyle,"List\0Circles\0Diamonds\0Squares\0Text only\0");
                if(options.equipmentStyle)combo("Arrangement","##equipment layout",options.equipmentLayout,"Vertical\0Horizontal\0");
                if(options.equipmentStyle>0&&options.equipmentStyle<4)combo("Text position","##equipment text",options.equipmentTextPosition,"Top\0Bottom\0Left\0Right\0");
                slider("Scale",options.equipmentScale,.5f,2.f,"%.2fx");slider("Text size",options.equipmentTextScale,.75f,1.5f,"%.2fx");
                slider("Spacing",options.equipmentSpacing,0,4,"%.2fx");
                slider("X",options.equipmentX,0,1,"%.3f");slider("Y",options.equipmentY,0,1,"%.3f");
            } else {
                changed|=UI::Checkbox("Show keys",&options.showKeys);
                changed|=UI::Checkbox("Hide empty groups",&options.hideEmptyGroups);
                combo("Inactive groups","##inactive groups",options.inactiveGroups,"Show\0Dim\0Hide\0");
                if(options.inactiveGroups==1)slider("Inactive opacity",options.inactiveGroupOpacity,.05f,.9f,"%.2f");
                label(tr("Widget visibility"));ImGui::SetNextItemWidth(-1);int visibility=int(widgetVisibility(options));
                if(UI::Combo("##widget visibility",&visibility,"Always visible\0Fade when idle\0Hide when idle\0Only while selecting\0")){setWidgetVisibility(options,WidgetVisibility(visibility));changed=true;}
                if(options.showIdle&&options.fadeIdle) {
                    slider("Idle delay",options.fadeDelay,0,20,"%.1f s");
                    if(widgetVisibility(options)==WidgetVisibility::fade)slider("Idle opacity",options.idleOpacity,0,1,"%.2f");
                    changed|=UI::Checkbox("Keep visible in combat",&options.keepCombatVisible);
                    changed|=UI::Checkbox("Keep visible with weapons drawn",&options.keepDrawnVisible);
                }
            }
            endPanel();ImGui::SameLine();
            beginPanel("live preview",{0,0},ImGuiChildFlags_Borders|ImGuiChildFlags_NavFlattened);
            label(tr("Preview"));ImGui::SetNextItemWidth(px(180));UI::Combo("##preview view",&previewMode_,"Widget\0Selector\0Both\0Equipment\0");snapshot.browsing=previewMode_==1||previewMode_==2;snapshot.selectorOnly=previewMode_==1;snapshot.equipmentOnly=previewMode_==3;
            const auto origin=ImGui::GetCursorScreenPos(),size=ImGui::GetContentRegionAvail();
            auto previewOptions=options;
            previewOptions.showIdle=true;if(snapshot.equipmentOnly)previewOptions.showEquipment=true;auto* d=ImGui::GetWindowDrawList();d->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
            hud.draw(snapshot,previewOptions,now,origin,size,d);d->PopClipRect();
            endPanel();
        }
        endPanel();
        ImGui::Separator();
        const auto message=!state.saveError.empty()?tr("Settings could not be saved"):!hint_.empty()?hint_:state.message;
        if(message.empty()&&state.controller&&!state.vr)UI::TextDisabled("Right stick: cursor · %s: select / drag",padLabel(PadButton::a,options.controllerStyle).c_str());
        else ImGui::TextColored(muted,"%s",message.c_str());
        if(UI::Button("Close",{px(170),0}))emit(command(EditAction::close,groupID));ImGui::SameLine();
        ImGui::BeginDisabled(!state.canUndo);if(UI::Button("Undo",{px(100),0}))emit(command(EditAction::undo,groupID));ImGui::EndDisabled();ImGui::SameLine();
        ImGui::BeginDisabled(!state.canRevert);if(UI::Button("Revert session",{px(145),0}))emit(command(EditAction::revert,groupID));ImGui::EndDisabled();
        ImGui::End();
    }

    auto& nav=*ImGui::GetCurrentContext();
    auto& local=nav.NavMoveResultLocal;const auto& other=nav.NavMoveResultOther;
    const bool inList=listNavigation&&(nav.NavMoveDir==ImGuiDir_Up||nav.NavMoveDir==ImGuiDir_Down);
    if(nav.NavMoveScoringItems&&!(nav.NavMoveFlags&ImGuiNavMoveFlags_IsTabbing)&&!inList&&local.ID&&other.ID&&nav.NavWindow&&
        other.Window->RootWindowForNav==nav.NavWindow->RootWindowForNav&&
        (other.DistBox<local.DistBox||(other.DistBox==local.DistBox&&other.DistCenter<local.DistCenter)))local=other;
    if(changed&&out.empty()){auto c=command(EditAction::settings,state.draft.active);c.options=options;c.continuous=ImGui::IsAnyItemActive();emit(std::move(c));}
    else if(changed&&!out.empty()&&out.front().action==EditAction::close){out.front().hasSettings=true;out.front().options=options;}
    ImGui::PopFont();return out;
}
}

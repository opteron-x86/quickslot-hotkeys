#pragma once
#include "EditorModel.h"
#include "Hud.h"
#include <array>

namespace quickslot {
std::string scanName(unsigned scan);
class Editor {
    std::uint64_t session_=0,catalogVersion_=0,restoreSerial_=0;
    std::array<char,128> search_{};
    std::array<std::array<char,128>,Arsenal::count> names_{};
    struct KeyPress {unsigned scan,modifiers;};
    std::vector<KeyPress> downs_;
    std::vector<PadButton> padDowns_;
    std::array<bool,unsigned(PadButton::count)> padKeys_{};
    bool controllerMode_=false,vrMode_=false,pointerMode_=false,pointerDown_=false;
    ImGuiID returnTextFocus_=0;
    ImVec2 pointerAxis_{};
    bool dismissingPopup_=false;
    bool textUpper_=false;
    std::array<char,128> padText_{};
    std::array<bool,256> keys_{};
    std::uint64_t candidate_=0;
    std::size_t selected_=0;
    int filter_=0,binding_=-1,tab_=0,placementTarget_=0,controlsPage_=0;
    bool placement_=false,previewOpen_=true;
    int placementPanelTarget_=-1;
    bool placementPanelRight_=false;
    std::string hint_;
    ImVec2 mouse_{},dragStart_{};
    int appearancePage_=0,previewMode_=0,exportScope_=0;
    bool importAppearance_=true;
    std::array<char,65> presetName_{};
    std::string presetFile_;
    Entry shortcut_=smartEntry(0,0,0);
    std::optional<std::size_t> shortcutIndex_;
    std::uint32_t librarySelection_=0,editingLibrary_=0;
    std::size_t shortcutGroup_=Arsenal::count;
    std::array<char,128> comboName_{};
    int builder_=0;
    std::array<std::array<char,128>,2> handSearch_{};
    std::array<std::uint64_t,2> comboCandidates_{};
    std::uint64_t shortcutRevision_=0;
public:
    void beginSession(const EditorState&);
    void input(const std::vector<EditorInput>&);
    bool controllerPointer() const{return pointerMode_;}
    std::vector<EditCommand> draw(const EditorState&,Hud&,double now);
};
}
